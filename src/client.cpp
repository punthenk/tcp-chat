#include <iostream>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using std::string;

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    string message;
    
    string username;
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);
    
    // Creating socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address");
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }
    
    // Send username
    send(sock, username.c_str(), username.size(), 0);

    while (1) {
        std::cout << "Enter your message: ";
        std::getline(std::cin, message);

        if (!std::cin) {
            break;
        }

        if (message == "quit") {
            break;
        }

        ssize_t sent = send(sock, message.c_str(), message.size(), 0);
        if (sent < 0) {
            perror("send");
            break;
        }

        int bytes = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes <= 0) {
            std::cout << "Server disconnected." << std::endl;
            break;
        }

        std::cout << "Echo: " << string(buffer, bytes) << std::endl;

        memset(buffer, 0, BUFFER_SIZE);
    }

    close(sock);
    return 0;
}
