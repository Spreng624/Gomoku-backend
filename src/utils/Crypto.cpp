#include "Crypto.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/rand.h>
#endif

std::vector<uint8_t> GenerateRandomBytes(size_t size)
{
#ifdef _WIN32
    std::vector<uint8_t> buffer(size);
    if (BCryptGenRandom(NULL, buffer.data(), (ULONG)size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
    {
        return std::vector<uint8_t>(size, 0);
    }
    return buffer;
#else
    std::vector<uint8_t> buffer(size);
    if (RAND_bytes(buffer.data(), (int)size) != 1)
    {
        return std::vector<uint8_t>(size, 0);
    }
    return buffer;
#endif
}

// TODO: 实现以下加密算法
// 1. DH 密钥交换
// 2. AES-CBC 加密/解密
// 3. 数字签名验证

DHContext::DHContext()
    : isActive(false), lastHeartbeat(GetTimeMS()), lastActiveTime(0),
      sk(0), pk(0), pk2(0), iv(16), sharedKey(0), sig(0)
{
    KeyGen();
}

void DHContext::KeyGen()
{
    // TODO: 实现 DH 密钥生成
    sk = GenerateRandomBytes(32);  // 私钥
    pk = GenerateRandomBytes(32);  // 公钥
    sig = GenerateRandomBytes(32); // 签名
}

bool DHContext::CalculateSharedKey()
{
    sharedKey = GenerateRandomBytes(32);
    isActive = true;
    // TODO: 实现 DH 共享密钥计算
    return true;
}

bool DHContext::Encrypt(std::vector<uint8_t> &data)
{
    // TODO: 实现 AES-CBC 加密
    return true;
}

bool DHContext::Decrypt(std::vector<uint8_t> &data)
{
    // TODO: 检查 IV 防重放，实现 AES-CBC 解密
    return true;
}

void DHContext::nextIV()
{
    iv = GenerateRandomBytes(16);
}

std::vector<uint8_t> DHContext::Get_Pk_Sig()
{
    std::vector<uint8_t> pk_sig;
    pk_sig.insert(pk_sig.end(), pk.begin(), pk.end());
    pk_sig.insert(pk_sig.end(), sig.begin(), sig.end());
    return pk_sig;
}

SessionContext::SessionContext(int s, uint64_t id)
    : sock(s), sessionId(id)
{
    isActive = false;
    lastHeartbeat = GetTimeMS();
}