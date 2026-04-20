#include "CryptoUtils.h"
#include <iostream>
#include <openssl/sha.h>

CryptoUtils::Key CryptoUtils::deriveKey(unsigned long long sharedSecret) {
    CryptoUtils::Key key;
    SHA256(
        reinterpret_cast<unsigned char*>(&sharedSecret),
        sizeof(sharedSecret),
        key.data()
    );
    return key;
}

int CryptoUtils::encryptMessage(string message, int key) {
}
