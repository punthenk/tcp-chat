#include <iostream>

class DiffieHellman {
public:
    DiffieHellman();

    long long int generatePrivateKey(unsigned long long p);
    long long int calculatePublicKey(long long int g, long long int privateKey, long long int p);
    unsigned long long int modPow(long long int base, long long int exp, long long int mod);
    long long int computeSharedSecret(long long int otherPublicKey);
    long long int getPublicKey();

private:
    unsigned long long p; // prime
    long long int g; // generator
    long long int privateKey; // The secret private key
    long long int publicKey; // The public key
};