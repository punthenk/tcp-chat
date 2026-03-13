#include <iostream>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>

using std::string;

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;

    struct client {
        string name;
        int socket;
        sockaddr_in address;
    };
    
    // Clients
    std::vector<client> clients;

    socklen_t addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    // Start listening for incoming connections
    while (1) {

        // Accept incoming connection
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (new_socket < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        char buffer[BUFFER_SIZE] = {0};

        int bytes = recv(new_socket, buffer, BUFFER_SIZE, 0);
        if (bytes <= 0) {
            close(new_socket);
            continue;
        }

        string username(buffer, bytes);

        clients.push_back({username, new_socket, address});

        std::cout << "Client connected: " << username << std::endl;
        std::cout << "Total clients: " << clients.size() << std::endl;

        while (true) {
            bytes = recv(new_socket, buffer, BUFFER_SIZE, 0);

            if (bytes == 0) {
                clients.erase(std::remove_if(clients.begin(), clients.end(), [new_socket](const client& c) {
                    return c.socket == new_socket;
                }), clients.end());
                std::cout << "Client disconnected: " << username << std::endl;
                break;
            }

            if (bytes < 0) {
                perror("recv");
                break;
            }

            string message(buffer, bytes);
            std::cout << "Received from " << username << ": " << message << std::endl;

            ssize_t sent = send(new_socket, message.data(), message.size(), 0);
            if (sent < 0) {
                perror("send");
                break;
            }
        }

        close(new_socket);
    }
}
