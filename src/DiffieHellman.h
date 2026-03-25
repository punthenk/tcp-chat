#include <iostream>

class DiffieHellman {
public:
    DiffieHellman(); // Generates random private key

    long long int generatePrivateKey(unsigned long long p);
    long long int calculatePublicKey(long long int g, long long int privateKey, long long int p);
    long long int computeSharedSecret(long long int otherPublicKey);

private:
    unsigned long long p; // prime
    long long int g; // generator
    long long int privateKey; // The secret private key
    long long int publicKey; // The public key
};