#ifndef CRYPTO_H
#define CRYPTO_H

#include <vector>
#include <cstdint>
#include "TimeWheel.hpp"

std::vector<uint8_t> GenerateRandomBytes(size_t size);

enum class CryptoAlgorithm
{
    AES,
    DES,
    RC4
};

enum class CryptoMode
{
    ECB,
    CBC
};

class DHContext
{
public:
    bool isActive;
    uint64_t lastHeartbeat;
    uint64_t lastActiveTime;

    std::vector<uint8_t> sk, pk, pk2, iv, sharedKey, sig;

    DHContext();
    void KeyGen();
    bool CalculateSharedKey();
    bool Encrypt(std::vector<uint8_t> &data);
    bool Decrypt(std::vector<uint8_t> &data);
    void nextIV();
    std::vector<uint8_t> Get_Pk_Sig();
};

class SessionContext : public DHContext
{
public:
    int sock;
    uint64_t sessionId;

    SessionContext(int s, uint64_t id);
};

#endif