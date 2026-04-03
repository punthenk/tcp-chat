#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <optional>

using std::string;

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;

struct Client {
    string name;
    int socket;
    sockaddr_in address;

    Client(string name, int socket, sockaddr_in address)
        : name(std::move(name)), socket(socket), address(address) {}
};

struct Chat {
    Client client1;
    Client client2;

    Chat(Client client1, Client client2) : client1(client1), client2(client2) {}
};

std::optional<Client> waiting_client;

std::vector<Chat> chats;
std::mutex chats_mutex;

void handle_client(int client_fd, sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE] = {0};

    // First message = username because we want to store the username with each client
    // and that is the first message sent by the client
    int bytes = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes <= 0) {
        close(client_fd);
        return;
    }

    string username(buffer, bytes);
    {
        std::lock_guard<std::mutex> lock(chats_mutex);
        Client client(username, client_fd, client_addr);

        if (waiting_client.has_value()) {
            Chat chat(client, waiting_client.value());
            chats.push_back(chat);
            waiting_client.reset();
        } else {
            waiting_client = client;
        }
    }

    // Read loop for this client
    while (true) {
        bytes = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes <= 0) break;
        string message(buffer, bytes);
        std::cout << username << ": " << message << std::endl;
        std::cout << "Waiting client: " << waiting_client->name << std::endl;
        {
            std::lock_guard<std::mutex> lock(chats_mutex);
            const string wire = username + ": " + message;
            for (const Chat& chat : chats) {
                if (chat.client1.socket == client_fd) {
                    std::cout << "sending message to client2\n";
                    send(chat.client2.socket, wire.c_str(), wire.size(), 0);
                } else if (chat.client2.socket == client_fd) {
                    std::cout << "sending message to client1\n";
                    send(chat.client1.socket, wire.c_str(), wire.size(), 0);
                }
            }
        }

        memset(buffer, 0, BUFFER_SIZE);
    }

    // Disconnect
    {
        std::lock_guard<std::mutex> lock(chats_mutex);

        std::optional<Client> remaining_client;

        if (waiting_client.has_value() && waiting_client->socket == client_fd) {
            waiting_client.reset();
        }

        chats.erase(std::remove_if(chats.begin(), chats.end(),
                                     [client_fd, &remaining_client](const Chat &chat) {
                                         if (chat.client1.socket == client_fd) {
                                             remaining_client = chat.client2;
                                             return true;
                                         }
                                         if (chat.client2.socket == client_fd) {
                                             remaining_client = chat.client1;
                                             return true;
                                         }

                                         return false;
                                     }), chats.end());

        if (remaining_client.has_value()) {
            if (waiting_client.has_value()) {
                chats.emplace_back(remaining_client.value(), waiting_client.value());
                waiting_client.reset();
            } else {
                waiting_client = remaining_client;
            }
        }
    }

    close(client_fd);
}

int main() {
    int server_fd;
    struct sockaddr_in address{};
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
            std::lock_guard<std::mutex> lock(chats_mutex);
            std::cout << "Total chats: " << chats.size() << std::endl;
            std::cout << "Waiting client: " << waiting_client->name << std::endl;
        }
    }
}
