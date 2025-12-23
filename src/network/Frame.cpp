#include "Frame.h"

#include <iostream>
#include <cstring>

#define BUFFER_SIZE 4096
#define MAX_FRAME_SIZE 1024

Frame::Frame(Status status, uint64_t sessionId, std::array<uint8_t, 16> iv, std::vector<uint8_t> data)
{
    head.magic = MAGIC_NUMBER;
    head.length = sizeof(Header) + data.size();
    head.status = status;
    head.sessionId = sessionId;
    head.iv = iv;
    this->data = data;
}

bool Frame::ReadHeader(std::vector<uint8_t> &buffer)
{
    if (buffer.size() < sizeof(Header) || buffer.size() > MAX_FRAME_SIZE)
        return false;
    uint32_t magic = 0;
    std::copy_n(buffer.begin(), 4, reinterpret_cast<uint8_t *>(&magic));
    if (magic != MAGIC_NUMBER)
        return false;

    std::copy_n(buffer.data(), sizeof(Header), reinterpret_cast<uint8_t *>(&head));
    return true;
}

bool Frame::ReadBytes(std::vector<uint8_t> &buffer)
{
    if (!ReadHeader(buffer))
        return false;
    if (head.length > buffer.size() || head.length > MAX_FRAME_SIZE || head.length < sizeof(Header))
        return false;
    if (head.length == sizeof(Header))
        return true;
    data.resize(head.length - sizeof(Header));
    std::copy_n(buffer.data() + sizeof(Header), head.length - sizeof(Header), data.data());
    return true;
}

bool Frame::ReadStream(std::vector<uint8_t> &buffer)
{
    while (buffer.size() >= 4)
    {
        // 头部校验
        if (!ReadHeader(buffer))
        {
            buffer.erase(buffer.begin());
            continue;
        }
        // 检测半包
        if (buffer.size() < head.length)
        {
            return false;
        }
        // 尝试读取数据
        if (!ReadBytes(buffer))
        {
            buffer.erase(buffer.begin());
            continue;
        }
        // 成功读取一个完整的包
        buffer.erase(buffer.begin(), buffer.begin() + head.length);
        return true;
    }

    return false; // 剩余数据不足一个 Header
}

bool Frame::ParseKey(std::vector<uint8_t> &key, int len)
{
    if (data.size() < len)
        return false;
    std::copy_n(data.data(), len, key.data());
    key.resize(len);
    return true;
}

std::vector<uint8_t> Frame::ToBytes()
{
    std::vector<uint8_t> buffer(MAX_FRAME_SIZE);
    head.length = sizeof(Header) + data.size();
    std::copy_n(reinterpret_cast<uint8_t *>(&head), sizeof(Header), buffer.data());
    std::copy_n(data.data(), data.size(), buffer.data() + sizeof(Header));
    buffer.resize(head.length);
    return buffer;
}
