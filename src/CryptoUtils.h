#include <iostream>
#pragma once

using std::string;

class CryptoUtils {
public:
    using Key = std::array<unsigned char, 32>;
    static Key deriveKey(unsigned long long key);
    static std::vector<unsigned char> encryptMessage(string &plainMessage, unsigned char iv[16], Key key);
    static string decryptMessage(std::vector<unsigned char>& encryptedMessage, const unsigned char iv[16], Key key);
};