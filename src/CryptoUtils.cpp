#include "CryptoUtils.h"
#include <iostream>
#include <string>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

CryptoUtils::Key CryptoUtils::deriveKey(unsigned long long sharedSecret) {
    CryptoUtils::Key key;
    SHA256(
        reinterpret_cast<unsigned char*>(&sharedSecret),
        sizeof(sharedSecret),
        key.data()
    );
    return key;
}

std::vector<unsigned char> CryptoUtils::encryptMessage(string& plainMessage, unsigned char iv[16], Key key) {
    // Create a random 16 bytes IV
    RAND_bytes(iv, 16);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();

    // Initialize with AES-256-CBC, the key, and the IV
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv);

    std::vector<unsigned char> ciphertext(plainMessage.size() + 16);
    int len = 0;
    int total = 0;

    EVP_EncryptUpdate(ctx, ciphertext.data(), &len, reinterpret_cast<const unsigned char*>(plainMessage.data()), plainMessage.size());
    total += len;

    EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
    total += len;

    EVP_CIPHER_CTX_free(ctx);
    ciphertext.resize(total);
    return ciphertext;
}

string CryptoUtils::decryptMessage(std::vector<unsigned char>& cipherMessage, const unsigned char iv[16], Key key) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();

    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv);

    std::vector<unsigned char> plaintext(cipherMessage.size());
    int len = 0;
    int total = 0;

    EVP_DecryptUpdate(ctx, plaintext.data(), &len,
        cipherMessage.data(), cipherMessage.size());
    total += len;

    EVP_DecryptFinal_ex(ctx, plaintext.data() + total, &len);
    total += len;

    EVP_CIPHER_CTX_free(ctx);

    return string(reinterpret_cast<char*>(plaintext.data()), total);
}