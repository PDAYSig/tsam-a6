#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <poll.h>
#include <string>
#include <cstdint>


#include <iostream>
#include <sstream>

using namespace std;


int connectToServer(string ip, int port){
    // Create a socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sock < 0){
        perror("Error creating socket");
        return -1;
    }

    //Set server IP and port
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) != 1){
        cerr << "Invalid IP address" << endl;
        close(sock);
        return -1;
    }

    //Connect to the server

    if(connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Error connecting to server");
        close(sock);
        return -1;
    }

    cout << "Connected to Server" << endl;

    return sock;
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



bool sendMessage(int sock, string message){
    
    if (message.size() > 4995){
        cerr << "Message is too long" << endl;
        return false;
    }
    
    
    string frame = createFrame(message);
    
    size_t totalSent = 0;
    while(totalSent < frame.size()){
        ssize_t sent = send(sock, frame.data() + totalSent, frame.size() - totalSent, 0);
        //frame.data() + totalSent: Where to start sending     
        //frame.size() - totalSent: How many bytes are left
        //This is because the send() might not send all at once


        if (sent < 0 && errno == EINTR){
            continue;
        }
        if (sent <= 0){
            perror("Error sending");
            return false;
        }

        totalSent += sent;
    } 
    return true;

}

bool receiveResponse(int sock, string& response){

    char header[4]; //SOH,length1,length2,STX

    int totalReceived = 0;

    while (totalReceived < 4){

        int received = recv(sock, header + totalReceived, 4 - totalReceived, 0);
        //Make sure that it works if you don't receive everything at once

        if (received < 0 && errno == EINTR){
            continue;
        }
        if (received <= 0){
            perror("Error receiving");
            return false;
        }
        totalReceived += received; 
    }

    //Check if SOH and STX are correct
    if (header[0] != 0x01 || header[3] != 0x02){
        return false;
    }

    //Combine the 2 length bytes
    uint16_t length = ((unsigned char)header[1] << 8) | (unsigned char)header[2];


    //Check if the frame is too small or too big
    if (length < 5 || length > 5000){
        return false;
    }

    //Create a empty string for the rest of the message (we already have first 4 bytes)
    string message(length - 4, '\0');

    totalReceived = 0;
    while (totalReceived < message.size()){

        int received = recv(sock, &message[totalReceived], message.size() - totalReceived, 0);

        if (received < 0 && errno == EINTR){
            continue;
        }

        if (received <= 0){
            perror("Error receiving");
            return false;
        }

        totalReceived += received;
    }

    //Check if ETX is correct
    if (message.back() != 0x03){
        cerr << "Last byte is not ETX" << endl;
        return false;
    }

    //Remove the ETX and store only the message
    response = message.substr(0,message.size() - 1);

    return true;

}

bool sendUsername(int sock, string userName){
    if (userName.empty()){
        return false;
    }

    if(!sendMessage(sock, "USERNAME," + userName)){
        return false;
    }
    
    string response;

    if (!receiveResponse(sock, response)){
        return false;
    }
    if (response == "OK"){
        return true;
    }

    return false;
}



int main(int argc, char* argv[]){
    if (argc != 3){
        printf("Usage: chat_client <ip  port>\n");
        printf("Ctrl-C to terminate\n");
        return 1;
    }
    string ip = argv[1];
    int port = stoi(argv[2]);


    int serversock = connectToServer(ip, port);

    if (serversock < 0){
        return 1;
    }

    string command;
    string response;
    
    //check username
    while (true){
        cout << "Enter username: " << endl;
        getline(cin, userName);

        if (sendUsername(serversock, username)){
            break;
        }
        else{
            cout << "Invalid username" << endl;
        }
    }

    while (true){
        cout << "Enter command (Q to quit): " << endl;

        //Get teh input
        if (!getline(cin, command)){
            break;
        }

        if (command == "Q"){
            break;    
        }
        if (command.empty()){
            continue;
        }

        cout << "Command: " << command << endl;
        
        if (!sendMessage(serversock, command)){
            cout << "Error sending message" << endl;
            break;
        }

        if (!receiveResponse(serversock, response)){
            cout << "Error receiving response" << endl;
            break;
        }

        cout << "Server response: " << response << endl;
    }

    close(serversock);

    return 0;
}