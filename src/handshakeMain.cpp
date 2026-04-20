#include "DiffieHellman.h"
#include "CryptoUtils.h"
#include <iostream>
#include <iomanip>

int main() {
    DiffieHellman Alice;
    DiffieHellman Bob;

    long long A = Alice.getPublicKey();
    long long B = Bob.getPublicKey();

    unsigned long long secretAlice = Alice.computeSharedSecret(B);
    unsigned long long secretBob = Bob.computeSharedSecret(A);

    if (secretAlice != secretBob) {
        perror("Compute shared secret");
    }

    CryptoUtils::Key hashed_key = CryptoUtils::deriveKey(secretBob);
    std::cout << "Derived key: ";
    for (unsigned char byte : hashed_key) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    std::cout << std::dec << std::endl;

    string plain_message = "Hello again! I have been encrypted some time, but now I'm plain text again!";

    unsigned char iv[16];
    auto cipher_message = CryptoUtils::encryptMessage(plain_message, iv, hashed_key);
    std::cout << "\nEncrypted message: ";
    for (int i = 0; i < cipher_message.size(); i++) {
        std::cout << static_cast<unsigned int>(cipher_message[i]) << std::flush;
    }
    std::cout << std::dec << std::endl;

    auto decrypted_message = CryptoUtils::decryptMessage(cipher_message, iv, hashed_key);
    std::cout << "\nDecrypted message: " << decrypted_message << std::endl;

    return 0;
}
