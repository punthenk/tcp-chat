#include <iostream>
#pragma once

using std::string;

class CryptoUtils {
public:
    using Key = std::array<unsigned char, 32>;
    static Key deriveKey(unsigned long long key);
    int encryptMessage(string message, int key);
    int decryptMessage(string encryptedMessage, int iv, int key);
};