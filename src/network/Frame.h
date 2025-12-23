#ifndef FRAME_H
#define FRAME_H

#include "EventBus.hpp"
#include <vector>
#include <cstdint>

#define MAGIC_NUMBER 0x12345678

// [加密位 1bit]
// 包头
class Frame
{

public:
    enum Status : uint32_t
    {
        // 用于会话层的信息
        Hello,   // 请求 Session
        Pending, // 正在提交密钥
        Active,  // 请正常分发

        Success,        // 成功
        Activated,      // 已激活
        Inactive,       // 未激活
        NewSession,     // 新会话
        NoSession,      // 无效会话
        InvalidRequest, // 无效包
        Error,          // 错误
    };
    struct Header
    {
        uint32_t magic;             // 魔数
        uint32_t length;            // 包长度
        Status status;              // 可拓展标志位
        uint64_t sessionId;         // 会话ID
        std::array<uint8_t, 16> iv; // 加密向量
    } __attribute__((packed));

    Header head;
    std::vector<uint8_t> data;
    Frame(Status status = Status::Active, uint64_t sessionId = 0, std::array<uint8_t, 16> iv = {}, std::vector<uint8_t> data = {});
    bool ReadStream(std::vector<uint8_t> &buffer);
    bool ReadHeader(std::vector<uint8_t> &buffer);
    bool ReadBytes(std::vector<uint8_t> &buffer);
    std::vector<uint8_t> ToBytes();
    bool ParseKey(std::vector<uint8_t> &key, int len);
};

#endif // FRAME_H
