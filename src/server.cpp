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
    // 1. Send a request to client to send public key
    MessageType type = MSG_REQ_PUBKEY;
    send(client.socket, &type, 1, 0);

    // 2. Receive the public key
    unsigned long long pubkey = 0;
    ssize_t bytes = recv(client.socket, &pubkey, sizeof(pubkey), 0);
    if (bytes <= 0)
        return false;

    // 3. Store the public key in the Chat struct
    if (chat.client1.socket == client.socket) {
        chat.pubkey1 = pubkey;
    } else {
        chat.pubkey2 = pubkey;
    }

    // Check if both public keys are available and if so, proceed
    // and notify the other waiting thread
    {
        std::unique_lock<std::mutex> lock(handshake_mutex);
        if (chat.pubkey1 != 0 && chat.pubkey2 != 0) {
            chat.handshake_ready = true;
            handshake_cv.notify_one();
        }
    }
    // For the first thread we wait here until the other thread
    // has also put his public key in the Chat, and notify us to proceed
    {
        std::unique_lock<std::mutex> lock(handshake_mutex);
        handshake_cv.wait(lock, [&chat] { return chat.handshake_ready; });
    }

    // Send the client the other clients public key, for them to compute the shared secret
    long long other_pubkey;
    type = MSG_PUBKEY;
    if (chat.client1.socket != client.socket) {
        other_pubkey = chat.pubkey1;
    } else {
        other_pubkey = chat.pubkey2;
    }
    send(client.socket, &type, 1, 0);
    send(client.socket, &other_pubkey, sizeof(other_pubkey), 0);

    type = MSG_VERIFY_REQ;
    send(client.socket, &type, 1, 0);
    // Pass through the data
    unsigned char iv[16];
    recv(client.socket, iv, 16, 0);
    uint32_t len;
    recv(client.socket, &len, sizeof(len), 0);
    std::vector<unsigned char> data(len);
    recv(client.socket, data.data(), len, 0);

    type = MSG_VERIFY;
    send(client.socket, &type, 1, 0);
    send(client.socket, iv, 16, 0);
    send(client.socket, &len, sizeof(len), 0);
    send(client.socket, data.data(), len, 0);

    // Receive if the key exchange went successful
    MessageType success_computed_secret;
    bytes = recv(client.socket, &success_computed_secret, 1, 0);
    if (bytes <= 0 || success_computed_secret == MSG_COMPUTE_SHARED_SECRET_FAILING)
        return false;

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
        } else {
            string msg = "Waiting for other client...";
            MessageType type = MSG_WAIT;
            send(client.socket, &type, 1, 0);
            send(client.socket, msg.c_str(), msg.size(), 0);
            {
                std::unique_lock<std::mutex> lock(pair_mutex);
                pair_cv.wait(lock);
            }
            my_chat = find_my_chat(client_fd);
            if (my_chat != nullptr) {
                can_connect = do_dh_handshake(client, *my_chat);
            }
        }
    }

    // Read loop for this client
    if (can_connect) {
        MessageType type = MSG_CONNECT_TO_CHAT;
        send(client_fd, &type, 1, 0);

        my_chat->paired = true;
        int other_client_socket;

        if (my_chat->client1.socket == client_fd) {
            other_client_socket = my_chat->client2.socket;
        } else {
            other_client_socket = my_chat->client1.socket;
        }

        while (my_chat->paired == true) {
            uint8_t type = 0;
            int bytes = recv(client_fd, &type, 1, 0);
            if (bytes <= 0) {
                MessageType type = MSG_DISCONNECT;
                send(client_fd, &type, 1, 0);
                send(other_client_socket, &type, 1, 0);
            }

            switch (type) {
                case MSG_CHAT: {
                    // 1. Receive the iv
                    unsigned char iv[16];
                    recv(client_fd, &iv, 16, 0);

                    // 2. Receive the length of the ciphertext
                    uint32_t len;
                    recv(client_fd, &len, sizeof(len), 0);

                    // If length is 0 that means the real data is not coming through right
                    if (len <= 0) {
                        MessageType type = MSG_DISCONNECT;
                        send(other_client_socket, &type, 1, 0);
                        send(client_fd, &type, 1, 0);
                        break;
                    }

                    // 3. Receive the ciphertext
                    std::vector<unsigned char> data(len);
                    recv(client_fd, data.data(), len, 0);

                    {
                        std::lock_guard<std::mutex> lock(chats_mutex);
                        MessageType type = MSG_CHAT;
                        send(other_client_socket, &type, 1, 0);

                        send(other_client_socket, iv, 16, 0);
                        send(other_client_socket, &len, sizeof(len), 0);
                        send(other_client_socket, data.data(), len, 0);
                    }
                    break;
                }
                case MSG_DISCONNECT: {
                    type = MSG_DISCONNECT;
                    send(other_client_socket, &type, 1, 0);
                    break;
                }
            }
        }
    }

    // Disconnect
    {
        std::lock_guard<std::mutex> lock(chats_mutex);
        my_chat->paired = false;
        if (waiting_client.has_value() && waiting_client->socket == client_fd) {
            waiting_client.reset();
        }

        chats.erase(std::remove_if(chats.begin(), chats.end(),
                                   [client_fd](const Chat &chat) {
                                       if (chat.client1.socket == client_fd) {
                                           return true;
                                       }
                                       if (chat.client2.socket == client_fd) {
                                           return true;
                                       }
                                       return false;
                                   }), chats.end());
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
