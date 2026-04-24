#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "DiffieHellman.h"
#include "protocol.h"
#include "CryptoUtils.h"

using std::string;

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;
const string PROMPT = "> ";
std::mutex io_mutex;
DiffieHellman dh;

unsigned long long shared_secret = 0;
CryptoUtils::Key hashed_key;

void receive_loop(int sock, string username) {
    char buffer[BUFFER_SIZE];
    while (true) {
        uint8_t type;
        int bytes = recv(sock, &type, 1, 0);
        if (bytes <= 0) {
            std::cout << "\nYou are disconnected." << std::endl;
            break;
        }

        switch (type) {
            case MSG_REQ_PUBKEY: {
                unsigned long long pubkey = dh.getPublicKey();
                std::cout << "\r\x1b[2K" << "Trying to connect to other client..." << std::endl;
                send(sock, &pubkey, sizeof(pubkey), 0);
                break;
            }
            case MSG_PUBKEY: {
                unsigned long long theirKey;
                recv(sock, &theirKey, sizeof(theirKey), 0);
                shared_secret = dh.computeSharedSecret(theirKey);
                hashed_key = CryptoUtils::deriveKey(shared_secret);
                break;
            }
            case MSG_VERIFY_REQ: {
                unsigned char iv[16];
                string msg = "OK";
                auto ciphertext = CryptoUtils::encryptMessage(msg, iv, hashed_key);
                uint32_t len = ciphertext.size();
                send(sock, iv, 16, 0);
                send(sock, &len, sizeof(len), 0);
                send(sock, ciphertext.data(), len, 0);
                break;
            }
            case MSG_VERIFY: {
                unsigned char iv[16];
                recv(sock, iv, 16, 0);

                uint32_t len;
                recv(sock, &len, sizeof(len), 0);

                std::vector<unsigned char> ciphertext(len);
                recv(sock, ciphertext.data(), len, 0);
                string plain_message = CryptoUtils::decryptMessage(ciphertext, iv, hashed_key);
                int type = MSG_COMPUTE_SHARED_SECRET_SUCCESS;
                if (plain_message != "OK") {
                    {
                        std::lock_guard<std::mutex> lock(io_mutex);
                        std::cout << "\r\033[K";
                        std::cout << "There went something wrong with connecting safely to the other client. Please try again" << std::endl;
                    }
                    type = MSG_COMPUTE_SHARED_SECRET_FAILING;
                }
                send(sock, &type, 1, 0);
                break;
            }
            case MSG_CHAT: {
                unsigned char iv[16];
                recv(sock, iv, 16, 0);

                uint32_t len;
                recv(sock, &len, sizeof(len), 0);

                std::vector<unsigned char> ciphertext(len);
                recv(sock, ciphertext.data(), len, 0);

                {
                    string msg = CryptoUtils::decryptMessage(ciphertext, iv, hashed_key);
                    std::lock_guard<std::mutex> lock(io_mutex);
                    std::cout << "\r\033[K";
                    std::cout << msg << "\n";
                    std::cout << PROMPT << std::flush;
                }
                break;
            }
            case MSG_WAIT: {
                char buffer[BUFFER_SIZE];
                int len = recv(sock, buffer, BUFFER_SIZE, 0);
                string msg(buffer, len);
                std::cout << "\r\x1b[2K" << msg << std::endl;
                break;
            }
            case MSG_CONNECT_TO_CHAT: {
                std::cout << "\r\x1b[2K\r" << "**** You are connect to a chat as " << username << " ****" << std::endl;
                std::cout << PROMPT << std::flush;
                break;
            }
            default: {
                std::cout << "You received an unknown message type: " << type << std::endl;
            }
        }
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    string message;
    unsigned char iv[16];

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

    std::thread receiver(receive_loop, sock, username);
    receiver.detach();

    std::cout << PROMPT << std::flush;

    while (true) {
        std::getline(std::cin, message);

        if (!std::cin || message == "quit") break;
        if (message.empty()) continue;

        {
            std::lock_guard<std::mutex> lock(io_mutex);
            std::cout << "\033[1A\r\033[K";  // go up one line, clear it
            std::cout << "<" << username << "> " << message << "\n";
            std::cout << PROMPT << std::flush;
        }

        message = "<" + username + "> " + message;
        auto ciphertext = CryptoUtils::encryptMessage(message, iv, hashed_key);
        uint32_t len = ciphertext.size();

        send(sock, iv, 16, 0);
        send(sock, &len, sizeof(len), 0);
        send(sock, ciphertext.data(), len, 0);
    }

    close(sock);
    return 0;
}
