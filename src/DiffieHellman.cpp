#include "DiffieHellman.h"
#include <iostream>
#include <random>

DiffieHellman::DiffieHellman() {
    p = 9223372036854775783LL;
    g = 2;
    privateKey = generatePrivateKey(p);
    publicKey = calculatePublicKey(g, privateKey, p);
    std::cout << "Public Key: " << publicKey << std::endl;
    std::cout << "Private Key: " << privateKey << std::endl;
}

long long int DiffieHellman::generatePrivateKey(unsigned long long p) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned long long> dist(1, p - 2);
    return dist(gen);
}

long long int modPow(long long int base, long long int exp, long long int mod) {
    long long int result = 1;
    base = base %= mod;
    while (exp > 0) {
        if (exp % 2 == 1)
            result = ((__int128)result * base) % mod;
        exp = exp / 2;
        base = ((__int128)base * base) % mod;
    }
    return result;
}

long long int DiffieHellman::calculatePublicKey(long long int g, long long int privateKey, long long int p) {
    return modPow(g, privateKey, p);
}
