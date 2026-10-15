#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <poll.h>
#include <errno.h>
#include <map>
#include <chrono>
#include <algorithm>


using namespace std;


#define BACKLOG  5                      // Allowed length of queue of waiting connections


class Client
{
    public:
    int sock;
    string name;
    unsigned char recvbuffer[5000];
    unsigned char sendbuffer[5000];

    Client(int socket) : sock(socket){};
    ~Client(){};
};


//TODO: Create server log functionality


map<int, Client*> clients; // Lookup table for per Client information


// list of poll file descriptors.
// Index 0 is always the listening socket, all others are connected clients.
vector<struct pollfd> fds;


struct command_input{
    string cmd;
    string from;
    string to;
    string msg;
};


struct unread_message{
    string message;
    string from;
};


map<string,vector<unread_message>* > UNREAD_MESSAGES;


int nfds = 1;
int open_socket(int port){
    int sock;
    int set = 1;
    sockaddr_in sk_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        perror("failed to open Socket");
        return (-1);
    }
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
    {
        perror("Failed to set SO_REUSEADDR:");
        close(sock);
    }

    memset(&sk_addr, 0, sizeof(sk_addr));

    sk_addr.sin_family       = AF_INET;
    sk_addr.sin_addr.s_addr = INADDR_ANY;
    sk_addr.sin_port        = htons(port);
    if(::bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0){
        perror("error binding socket");
        return (-1);
    }

    return sock;
}


void add_clients(int listening_sock, sockaddr_in client, size_t client_len, vector<struct pollfd>& fds){ //map<int, Client*>* clients
    printf("Client connected!\n");
   
    while (true){               // While there is some client waiting to connect, accept that connection
        int client_sock = accept(listening_sock, (sockaddr*) &client,(socklen_t*) &client_len);
        if (client_sock < 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            close(listening_sock);
            perror("Error connecting to client");
            exit(1);
        }
        printf("Client connected!\n");
        //TODO: if no username, fuck you
        fds.push_back({client_sock, POLLIN, 0});
        Client new_client = Client(client_sock);
        clients[client_sock] = &new_client;
        nfds++;
    }
}


void remove_clients(Client* client){
    printf("Disconnecting client\n");
    int client_socket = client->sock;
    close(client_socket);

    auto fdIt = find_if(fds.begin(), fds.end(),
                [client_socket](const struct pollfd &pfd)
                {
                    return pfd.fd == client_socket;
                });

    if(fdIt != fds.end())
    {
        fds.erase(fdIt);
    }
}


command_input extract_command(char* message){
    stringstream ss(message);
    command_input command{};

    if (!getline(ss, command.cmd, ',')) return command;
    if (command.cmd == "SENDMSG"){
        if (!getline(ss, command.from, ',')) return command;
        if (!getline(ss, command.to, ',')) return command;
        if (!getline(ss, command.msg, ',')) return command;
    } else if (command.cmd == "GETMSG"){
        if (!getline(ss, command.from, ',')) return command;
    } else if (command.cmd == "STATUSREQ") return command;
    return command;
}

int sendMessage(command_input &command){ // store a message on the server
    unread_message new_message;
    new_message.message = command.msg;
    new_message.from = command.from;
    if (UNREAD_MESSAGES.count(command.to) > 0){
        UNREAD_MESSAGES[command.to]->push_back(new_message);
    } else {
        vector<unread_message> entry = {new_message};
        UNREAD_MESSAGES[command.to] = &entry;
    } 
    return 0;
}


string getMessage(string client, string from){
    /* Returns a list of unread messages from */
    vector<string> client_messages;
    string client_message;
    if (UNREAD_MESSAGES.count(client) == 0) return "EMPTY";
    vector<unread_message> client_message_vector = *UNREAD_MESSAGES[client];
    for (int i = 0; i < client_message_vector.size(); i++){
        if (client_message_vector[i].from == from){
            client_message = client_message_vector[i].message;
            client_messages.pop_back();}
    }
    return client_message;
}


string statusRequest(){
    string message_statuses;
    int i = 0;
    for (const auto& [name, messages] : UNREAD_MESSAGES){
        int message_len = messages->size();
        if (message_len > 0){
            message_statuses += (name + ":" + to_string(message_len));
            i++;
            if (i != 1) message_statuses += ",";
        }
    }
    return message_statuses;
}


bool sendToClient(string c_name, string message){
    return true;
}

int main(int argc, const char* argv[]){
    if (argc != 2){
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }

    int port = stoi(argv[1]);
    struct sockaddr_in client_sock_addr;
    socklen_t client_len;
    int client_sock;                        // Socket of connecting client

    printf("Setting up listening socket...\n");
    int listening_sock = open_socket(port);
    if (listening_sock < 0){
        perror("Error creating listening socket");
        exit(1);
    }
    
    if (listen(listening_sock, BACKLOG) < 0){
        printf("Listen failed at %d\n", port);
        close(listening_sock);
        exit(0);
    } 
    else 
    // Add listen socket to the list of fds being polled.
    {
        fds.push_back({listening_sock, POLLIN});
        printf("Listening on port: %d\n", port);
    }
    
    //bool finished = false;
    while (true){
        // Wait for something to read
        printf("Waiting for data...\n");
        int n = poll(fds.data(), fds.size(), -1);

        if (n < 0){
            perror("Polling failed - Closing down\n");
            close(listening_sock);
            exit(1);
        }
        else
        {
            vector<struct pollfd> readyFds = fds;

            // Check if the listening socket has new readable data
            if (fds[0].revents & POLLIN){
                client_len = sizeof(client_sock_addr);
                add_clients(listening_sock, client_sock_addr, client_len, fds);
            }
            for(size_t i = 1; i < readyFds.size(); i++){
                if(readyFds[i].revents & POLLIN){
                    int client_sock = readyFds[i].fd;
                    Client *client = clients[client_sock];
                    unsigned char *buffer = client->recvbuffer;
                // Keep recieving until there is nothing more to recieve
                int recvd;
                    while (true){
                        recvd = recv(listening_sock, buffer, sizeof(client->recvbuffer), MSG_DONTWAIT);                        
                        // if it's a client disconnectiong, log it and remove from vector
                        //TODO: add to server log
                        if (recvd == 0) remove_clients(client); break;

                        

                    }

                    buffer[recvd] = '\0';

                    // Check the command
                    uint16_t length;
                    memset(&length, 0, sizeof(length));

                    memcpy(&length, buffer + 8, sizeof(u_int16_t));

                    int command_byte_len = ntohs(length) - 40;


                    unsigned char message[command_byte_len];
                    memcpy(message, buffer + 32, (size_t)(command_byte_len));

                    command_input command = extract_command((char*)message);

                    if (command.cmd == "SENDMSG"){
                        if (command.from == "" || command.to == "" || command.msg == ""){
                            printf("Incorrectly formatted command. Usage: SENDMSG,<to>,<from>,<msg>\n");
                            continue;
                        }
                        if (sendMessage(command) < 0){
                            cout << ("Failed to send message to " + (string) command.to);
                            continue;
                        }
                    } else if (command.cmd == "GETMSG"){
                        if (command.from == ""){
                            printf("Incorrectly formatted command. Usage: GETMSG,<from>\n");
                            continue;
                        }
                        string gotten_message = getMessage(client->name, command.from);
                        sendToClient(client->name, gotten_message);

                        // send message back to client
                    } else if (command.cmd == "STATUSREQ"){
                        string status = statusRequest();
                        sendToClient(client->name, status);
                    }
                    

                }
            }
        }
    }
}

