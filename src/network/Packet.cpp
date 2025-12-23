#include "Packet.h"

#include <memory>

// 定义值类型

template <typename T>
void WriteBytes(std::vector<uint8_t> &buffer, T value)
{
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&value);

    for (size_t i = 0; i < sizeof(T); ++i)
    {
        buffer.push_back(bytes[i]);
    }
}

template <typename T>
T ReadBytes(const std::vector<uint8_t> &buffer, size_t &offset)
{
    if (offset + sizeof(T) > buffer.size())
    {
        // throw std::runtime_error("Buffer underflow during read_bytes");
    }

    T value;
    uint8_t *bytes = reinterpret_cast<uint8_t *>(&value);

    // 按字节从小到大读取 (小端序)
    for (size_t i = 0; i < sizeof(T); ++i)
    {
        bytes[i] = buffer[offset + i];
    }
    offset += sizeof(T);
    return value;
}

// [Map数据: [Map字段数量M 4 Byte] + M * [[键长度N 4 Byte][键字符串 N Byte][值类型索引 1 Byte][值数据 N Byte]]

// bool Packet::CheckParam(std::initializer_list<std::string> keys) const
// {
//     for (auto key : keys)
//     {
//         if (!GetParam(key))
//         {
//             mgr.SendErrorMsg(sock, "Invalid Parameter");
//             return false;
//         }
//     }
//     return true;
// }

std::vector<uint8_t> Packet::Serialize() const
{
    std::vector<uint8_t> buffer;

    // 序列化 msgType
    WriteBytes<uint32_t>(buffer, static_cast<uint32_t>(msgType));

    // 序列化 params map
    WriteBytes<uint32_t>(buffer, static_cast<uint32_t>(params.size()));

    for (const auto &pair : params)
    {
        WriteBytes<uint32_t>(buffer, static_cast<uint32_t>(pair.first.length()));

        for (char c : pair.first)
        {
            buffer.push_back(static_cast<uint8_t>(c));
        }

        uint8_t index = static_cast<uint8_t>(pair.second.index());
        buffer.push_back(index);

        std::visit([this, &buffer, index](const auto &value)
                   {
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, uint8_t>) {
                buffer.push_back(value);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, uint32_t>) {
                WriteBytes<uint32_t>(buffer, value);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, uint64_t>) {
                WriteBytes<uint64_t>(buffer, value);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::string>) {
                WriteBytes<uint32_t>(buffer, static_cast<uint32_t>(value.length()));
                for (char c : value) {
                    buffer.push_back(static_cast<uint8_t>(c));
                }
            } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, bool>) {
                buffer.push_back(value ? 1 : 0);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::vector<uint8_t>>) {
                WriteBytes<uint32_t>(buffer, static_cast<uint32_t>(value.size()));
                for (uint8_t byte : value) {
                    buffer.push_back(byte);
                }
            } },
                   pair.second);
    }

    return buffer;
}

bool Packet::Deserialize(const std::vector<uint8_t> &buffer)
{
    size_t offset = 0;

    // 反序列化 msgType
    msgType = static_cast<MsgType>(ReadBytes<uint32_t>(buffer, offset));

    uint32_t num_elements = ReadBytes<uint32_t>(buffer, offset);

    for (uint32_t i = 0; i < num_elements; ++i)
    {
        uint32_t key_len = ReadBytes<uint32_t>(buffer, offset);

        if (offset + key_len > buffer.size())
        {
            return false;
        }
        std::string key(reinterpret_cast<const char *>(buffer.data() + offset), key_len);
        offset += key_len;

        if (offset + 1 > buffer.size())
        {
            return false;
        }
        uint8_t index = buffer[offset++];

        ValueType value;
        switch (index)
        {
        case 0: // int
            if (offset + 4 > buffer.size())
                return false;
            value = ReadBytes<uint32_t>(buffer, offset);
            break;
        case 1: // uint8_t
            if (offset + 1 > buffer.size())
                return false;
            value = buffer[offset++];
            break;
        case 2: // uint32_t
            if (offset + 4 > buffer.size())
                return false;
            value = ReadBytes<uint32_t>(buffer, offset);
            break;
        case 3: // uint64_t
            if (offset + 8 > buffer.size())
                return false;
            value = ReadBytes<uint64_t>(buffer, offset);
            break;
        case 4: // std::string
        {
            uint32_t str_len = ReadBytes<uint32_t>(buffer, offset);
            if (offset + str_len > buffer.size())
            {
                return false;
            }
            value = std::string(reinterpret_cast<const char *>(buffer.data() + offset), str_len);
            offset += str_len;
            break;
        }
        case 5: // bool
            if (offset + 1 > buffer.size())
            {
                return false;
            }
            value = (buffer[offset++] != 0);
            break;
        case 6: // std::vector<uint8_t>
        {
            uint32_t vec_len = ReadBytes<uint32_t>(buffer, offset);
            if (offset + vec_len > buffer.size())
            {
                return false;
            }
            std::vector<uint8_t> vec(buffer.data() + offset, buffer.data() + offset + vec_len);
            value = vec;
            offset += vec_len;
            break;
        }
        default:
            break;
        }

        params[key] = value;
    }
    msgType = static_cast<MsgType>(GetParam<uint32_t>("msgType", MsgType::None));

    return true;
}

Packet::Packet()
{
    params.clear();
    msgType = (MsgType)GetParam<uint32_t>("msgType", MsgType::None);
    AddParam("msgType", static_cast<uint32_t>(msgType));
}

Packet::Packet(uint64_t sessionId, MsgType type) : sessionId(sessionId), msgType(type)
{
    AddParam("msgType", static_cast<uint32_t>(msgType));
}

Packet::~Packet() {}

std::vector<uint8_t> Packet::ToBytes() const
{
    return Serialize();
}

bool Packet::FromData(uint64_t sessionId, const std::vector<uint8_t> &data)
{
    this->sessionId = sessionId;
    return Deserialize(data);
}

int Packet::AddParam(const std::string &key, const ValueType &value)
{
    params[key] = value;
    return 0;
}

int Packet::ClearParams()
{
    params.clear();
    return 0;
}
