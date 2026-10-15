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
    int recvd_bytes;

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


map<string,vector<unread_message> > UNREAD_MESSAGES;


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

              // While there is some client waiting to connect, accept that connection
    int client_sock = accept(listening_sock, (sockaddr*) &client,(socklen_t*) &client_len);
    printf("Client connected!\n");
    //TODO: if no username, fuck you
    fds.push_back({client_sock, POLLIN, 0});
    Client *new_client = new Client(client_sock);
    new_client->recvd_bytes = 0;
    memset(new_client->recvbuffer, 0, sizeof(new_client->recvbuffer));
    memset(new_client->sendbuffer, 0, sizeof(new_client->sendbuffer));
    
    clients[client_sock] = new_client;
    nfds++;

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

    if (!getline(ss, command.cmd, ',')) return {};
    if (command.cmd == "SENDMSG"){
        if (!getline(ss, command.from, ',')) return {};
        if (!getline(ss, command.to, ',')) return {};
        if (!getline(ss, command.msg)) return {};
    } else if (command.cmd == "GETMSG"){
        if (!getline(ss, command.from, ',')) return {};
    } else if (command.cmd == "STATUSREQ") return command;
    else if (command.cmd == "USERNAME"){
        if (!getline(ss, command.from)) return {};
    } else return {};
    return command;
}


int sendMessage(command_input &command){ // store a message on the server
    unread_message new_message;
    new_message.message = command.msg;
    new_message.from = command.from;
    if (UNREAD_MESSAGES.count(command.to) > 0){
        UNREAD_MESSAGES[command.to].push_back(new_message);
    } else {
        vector<unread_message> entry = {new_message};
        UNREAD_MESSAGES[command.to] = entry;
    } printf("MESSAGE SENT\n");
    return 0;
}


string getMessage(string client, string from){
    /* Returns a list of unread messages from */
    vector<string> client_messages;
    string client_message;
    if (UNREAD_MESSAGES.count(client) == 0) return "EMPTY";
    vector<unread_message> client_message_vector = UNREAD_MESSAGES[client];
    for (int i = 0; i < client_message_vector.size(); i++){
        if (client_message_vector[i].from == from){
            client_message = client_message_vector[i].message;
            client_messages.pop_back();}
    }
    return client_message;
}


string statusRequest(){
    printf("STATUSREQ requested\n");
    string message_statuses;
    int i = 0;
    for (const auto& [name, messages] : UNREAD_MESSAGES){
        int message_len = messages.size();
        if (message_len > 0){
            message_statuses += (name + ":" + to_string(message_len));
            i++;
            if (i != 1) message_statuses += ",";
        }
    }
    return message_statuses;
}

string createFrame(string message){

    //Create the frame that we edit and then return
    string frame = "";

    uint16_t length = message.size() + 5; // add 5 because of SOH,length(2bytes),STX and ETX

    frame += (char)0x01; //SOH

    frame += (char)(length >> 8); //Get the first byte
    frame += (char)(length & 0xFF); //Get the second byte

    frame += (char)0x02; //STX

    frame += message; //Add the message

    frame += (char)0x03; //ETX

    return frame;
}

bool sendToClient(Client* client, string message){
    printf("we sending it, dw\n");
    string frame = createFrame(message);
    int sent = send(client->sock, frame.data(), frame.size(), 0);
    if (sent < 0) return false;
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

        vector<struct pollfd> readyFds = fds;

        // Check if the listening socket has new readable data
        if (fds[0].revents & POLLIN){
            client_len = sizeof(client_sock_addr);
            add_clients(listening_sock, client_sock_addr, client_len, fds);
        }
        for(size_t i = 1; i < readyFds.size(); i++){
            printf("something is afoot\n");
            if(readyFds[i].revents & POLLIN){
                int client_sock = readyFds[i].fd;
                Client *client = clients[client_sock];
                unsigned char *buffer = client->recvbuffer;
            // Keep recieving until there is nothing more to recieve
            int recvd;
                recvd = recv(client_sock, buffer + client->recvd_bytes, sizeof(client->recvbuffer) - client->recvd_bytes, MSG_DONTWAIT);                        
                // if it's a client disconnectiong, log it and remove from vector
                //TODO: add to server log
                printf("We got something\n");
                // in this case we don't have any command to deal with, so we move on to the next fd
                if (recvd == 0) {
                    remove_clients(client);
                    continue;
                }
                printf("its not a disconnect...\n");
                client->recvd_bytes += recvd;
                
                // if we don't have at least 4 bytes, we can't know what the length of the frame should be so we move on
                if (client->recvd_bytes < 4) continue;
                
                // Extract the length
                uint16_t length;
                memset(&length, 0, sizeof(length));
                memcpy(&length, buffer + 1, sizeof(u_int16_t));

                size_t frame_len = ntohs(length);
                // Unless the frame is whole, we can´t go any further
                if (client->recvd_bytes < frame_len) continue;


                /* ------ Command handling ------ */
                printf("HOLY SHIT WE GOT IT, WE PROCESSING NOW\n");
                int command_byte_len = frame_len - 5;
                
                vector <unsigned char> message(command_byte_len + 1);             // account for null terminator
                memcpy(message.data(), buffer + 4, (size_t)(command_byte_len));
                message[command_byte_len] = '\0';

                command_input command = extract_command((char*)message.data());

                if (command.cmd == "SENDMSG"){
                    if (command.from == "" || command.to == "" || command.msg == ""){
                        printf("Incorrectly formatted command. Usage: SENDMSG,<from>,<to>,<msg>\n");
                        continue;
                    }
                    if (sendMessage(command) < 0){
                        cout << ("Failed to send message to " + (string) command.to);
                        sendToClient(client, "OK");
                        continue;
                    } //TODO: add server log
                } else if (command.cmd == "GETMSG"){
                    if (command.from == ""){
                        printf("Incorrectly formatted command. Usage: GETMSG,<from>\n");
                        continue;
                    }
                    string gotten_message = getMessage(client->name, command.from);
                    sendToClient(client, gotten_message);
                    //TODO: add server log


                } else if (command.cmd == "STATUSREQ"){
                    string status = statusRequest();
                    sendToClient(client, status);
                } else if (command.cmd == "USERNAME"){
                    client->name = command.from;
                    cout << client->name << " has logged in" << endl; 
                    sendToClient(client, "Welcome");
                } else sendToClient(client, "INVALID COMMAND");
            }
        }
    }
}

