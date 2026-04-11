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
#include "protocol.h"

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
    unsigned long long pubkey1 = 0;
    unsigned long long pubkey2 = 0;
    bool paired = false;
    bool handshake_ready = false;
    Chat(Client client1, Client client2) : client1(client1), client2(client2) {}
};

std::optional<Client> waiting_client;

std::condition_variable pair_cv;
std::mutex pair_mutex;

std::condition_variable handshake_cv;
std::mutex handshake_mutex;

std::vector<Chat> chats;
std::mutex chats_mutex;

bool do_dh_handshake(Client client, Chat& chat) {
    std::cout << "Call the handshake method" << std::endl;


    // 1. Send a request to client to send public key
    MessageType type = MSG_REQ_PUBKEY;
    send(client.socket, &type, 1, 0);

    // 2. Receive the public key
    unsigned long long pubkey = 0;
    ssize_t bytes = recv(client.socket, &pubkey, sizeof(pubkey), 0);
    if (bytes <= 0)
        return false;
    std::cout << "Received pubkey: " << pubkey << std::endl;

    // 3. Store the public key in the Chat struct
    if (chat.client1.socket == client.socket) {
        chat.pubkey1 = pubkey;
    } else {
        chat.pubkey2 = pubkey;
    }

    {
        std::unique_lock<std::mutex> lock(handshake_mutex);
        if (chat.pubkey1 != 0 && chat.pubkey2 != 0) {
            chat.handshake_ready = true;
            handshake_cv.notify_one();
        }
    }
    {
        std::unique_lock<std::mutex> lock(handshake_mutex);
        handshake_cv.wait(lock, [&chat] { return chat.handshake_ready; });
    }

    std::cout << "Pubkey1: " << chat.pubkey1 << ", Username: " << chat.client1.name << std::endl;
    std::cout << "Pubkey2: " << chat.pubkey2 << ", Username: " << chat.client2.name << std::endl;


    if (chat.client1.socket != client.socket) {
        long long other_pubkey = chat.pubkey1;
        MessageType type = MSG_PUBKEY;
        send(client.socket, &type, 1, 0);
        send(client.socket, &other_pubkey, sizeof(other_pubkey), 0);
    } else if (chat.client2.socket != client.socket) {
        long long other_pubkey = chat.pubkey2;
        MessageType type = MSG_PUBKEY;
        send(client.socket, &type, 1, 0);
        send(client.socket, &other_pubkey, sizeof(other_pubkey), 0);
    }

    MessageType success_computed_secret;
    bytes = recv(client.socket, &success_computed_secret, sizeof(success_computed_secret), 0);
    if (bytes <= 0 || success_computed_secret == MSG_COMPUTE_SHARED_SECRET_FAILING) {
        std::cout << "Failed to compute shared secret successfully" << std::endl;
        return false;
    }

    std::cout << "Shared secret is computed successfully" << std::endl;
    return true;
}

Chat* find_my_chat(int client_fd) {
    std::lock_guard<std::mutex> lock(chats_mutex);
    for (Chat& chat: chats) {
        if (chat.client1.socket == client_fd || chat.client2.socket == client_fd && chat.paired == false) {
            std::cout << "Chat: " << chat.client1.name << " : " << chat.client2.name << std::endl;
            return &chat;
        }
    }
    return nullptr;
}

void handle_client(int client_fd, sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE] = {0};

    // First message = username because we want to store the username with each client
    // and that is the first message sent by the client
    int bytes = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes <= 0) {
        close(client_fd);
        return;
    }

    bool can_connect = false;
    string username(buffer, bytes);
    Client client(username, client_fd, client_addr);
    std::optional<Client> other_client;
    {
        std::lock_guard<std::mutex> lock(chats_mutex);

        if (waiting_client.has_value()) {
            other_client = waiting_client.value();
            Chat chat(client, waiting_client.value());
            chats.push_back(chat);
            waiting_client.reset();
            pair_cv.notify_one();
        } else {
            waiting_client = client;
        }
    }
    Chat *my_chat = nullptr;
    {
        if (other_client) {
            my_chat = find_my_chat(client_fd);
            can_connect = do_dh_handshake(client, *my_chat);
            std::cout << "Found the other client" << std::endl;
        } else {
            string msg = "Waiting for other client...";
            MessageType type = MSG_CHAT;
            send(client.socket, &type, 1, 0);
            send(client.socket, msg.c_str(), msg.size(), 0);
            {
                std::unique_lock<std::mutex> lock(pair_mutex);
                pair_cv.wait(lock);
                my_chat = find_my_chat(client_fd);
            }
            if (my_chat != nullptr) {
                can_connect = do_dh_handshake(client, *my_chat);
                std::cout << "Found the other client as waiting" << std::endl;
            }
        }
    }

    // Read loop for this client
    if (can_connect) {
        my_chat->paired = true;
        while (true && my_chat != nullptr) {
            bytes = recv(client_fd, buffer, BUFFER_SIZE, 0);
            if (bytes <= 0) break;
            string message(buffer, bytes);
            std::cout << username << ": " << message << std::endl;
            std::cout << "Waiting client: " << waiting_client->name << std::endl;
            {
                std::lock_guard<std::mutex> lock(chats_mutex);
                const string wire = username + ": " + message;
                MessageType type = MSG_CHAT;
                if (my_chat->client1.socket == client_fd) {
                    std::cout << "Sending message to client2" << std::endl;
                    send(my_chat->client2.socket, &type, 1, 0);
                    send(my_chat->client2.socket, wire.c_str(), wire.size(), 0);
                } else if (my_chat->client2.socket == client_fd) {
                    std::cout << "Sending message to client2" << std::endl;
                    send(my_chat->client1.socket, &type, 1, 0);
                    send(my_chat->client1.socket, wire.c_str(), wire.size(), 0);
                }
            }

            memset(buffer, 0, BUFFER_SIZE);
        }
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