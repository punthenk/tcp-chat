#include "DiffieHellman.h"
#include <iostream>

int main() {
    DiffieHellman Alice;
    DiffieHellman Bob;

    long long A = Alice.getPublicKey();
    long long B = Bob.getPublicKey();

    long long secretAlice = Alice.computeSharedSecret(B);
    long long secretBob = Bob.computeSharedSecret(A);

    if (secretAlice == secretBob) {
        std::cout << "\nBoth keys are the same\n";
        std::cout << "Alice: " << secretAlice << "\n";
        std::cout << "Bob: " << secretBob << "\n";
        return 0;
    }
    std::cout << "\nBoth keys are different\n";
    std::cout << "Alice: " << secretAlice << "\n";
    std::cout << "Bob: " << secretBob << "\n";

    return 0;
}