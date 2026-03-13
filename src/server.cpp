#include <iostream>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>

using std::string;

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;

struct Client {
    string name;
    int socket;
    sockaddr_in address;
};

std::vector<Client> clients;
std::mutex clients_mutex;

void handle_client(int client_fd, sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE] = {0};

    // First message = username because we want to store the username with each client
    int bytes = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes <= 0) {
        close(client_fd);
        return;
    }

    string username(buffer, bytes);
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.push_back({username, client_fd, client_addr});
    }

    // Read loop for this client
    while (true) {
        bytes = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes <= 0) break;
        string message(buffer, bytes);
        std::cout << "Received from " << username << ": " << message << std::endl;

        // For now: echo message back only to the sender
        if (send(client_fd, message.data(), message.size(), 0) < 0) break;
    }

    // Disconnect
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(),
                                     [client_fd](const Client &c) {
                                         return c.socket == client_fd;
                                     }), clients.end());
    }

    close(client_fd);
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

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
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    // Start listening for incoming connections
    while (true) {
        // Accept incoming connection
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &addrlen);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        std::thread(handle_client, client_fd, client_addr).detach();

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            std::cout << "Total clients: " << clients.size() << std::endl;
        }
    }
}
