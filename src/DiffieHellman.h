#include <iostream>

class DiffieHellman {
public:
    DiffieHellman();

    unsigned long long int generatePrivateKey(unsigned long long p);
    unsigned long long int calculatePublicKey(long long int g, long long int privateKey, long long int p);
    unsigned long long int modPow(long long int base, long long int exp, long long int mod);
    unsigned long long int computeSharedSecret(long long int otherPublicKey);
    unsigned long long int getPublicKey();

private:
    unsigned long long p; // prime
    unsigned long long int g; // generator
    unsigned long long int privateKey; // The secret private key
    unsigned long long int publicKey; // The public key
};