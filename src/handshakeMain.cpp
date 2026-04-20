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

    if (secretAlice == secretBob) {
        std::cout << "\nBoth keys are the same\n";
        std::cout << "Alice: " << secretAlice << "\n";
        std::cout << "Bob: " << secretBob << "\n";
    } else {
        std::cout << "\nBoth keys are different\n";
        std::cout << "Alice: " << secretAlice << "\n";
        std::cout << "Bob: " << secretBob << "\n";
    }

    std::cout << secretBob << std::endl;

    CryptoUtils::Key hashed_key = CryptoUtils::deriveKey(secretBob);
    std::cout << "Derived key: ";
    for (unsigned char byte : hashed_key) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    std::cout << std::dec << std::endl;

    return 0;
}
