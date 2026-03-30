#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using std::string;

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;
const string PROMPT = "Enter your message: ";
std::mutex io_mutex;

void receive_loop(int sock) {
    char buffer[BUFFER_SIZE];
    while (true) {
        const int bytes = recv(sock, buffer, BUFFER_SIZE, 0);
        string raw(buffer, bytes);
        if (bytes <= 0) {
            std::lock_guard<std::mutex> lock(io_mutex);
            std::cout << "\nServer disconnected." << std::endl;
            break;
        }

        std::cout << "\r\x1b[2K" << raw << std::endl;
        std::cout << PROMPT << std::flush;
    }
}

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

    std::thread receiver(receive_loop, sock);
    receiver.detach();

    while (true) {
        {
            std::lock_guard<std::mutex> lock(io_mutex);
            std::cout << PROMPT << std::flush;
        }
        std::getline(std::cin, message);

        if (!std::cin || message == "quit") break;
        if (message.empty()) continue;

        send(sock, message.c_str(), message.size(), 0);
    }

    close(sock);
    return 0;
}