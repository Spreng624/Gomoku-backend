#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <vector>
#include <variant>
#include <string>
#include <map>

#define BUFFER_SIZE 4096

using ValueType = std::variant<
    int, uint8_t, uint32_t, uint64_t,
    std::string, bool, std::vector<uint8_t>>;
using MapType = std::map<std::string, ValueType>;

// 消息类型
enum MsgType : uint32_t
{
    None = 0,
    // 0x100-0x1FF 用户操作（请求）
    Login = 0x100,
    SignIn,
    LoginByGuest,
    Guest2User,
    EditUsername,
    EditPassword,
    LogOut,

    // 0x200-0x2FF 房间操作（请求）
    CreateRoom = 0x200,
    CreateSingleRoom,
    JoinRoom,
    ExitRoom,
    QuickMatch,

    // 0x300-0x3FF 座位操作（请求）
    TakeBlack = 0x300,
    TakeWhite,
    CancelTake,
    StartGame,
    EditRoomSetting,

    // 0x400-0x4FF 游戏操作（请求）
    MakeMove = 0x400,
    UndoMoveRequest,
    UndoMoveResponse,
    DrawRequest,
    DrawResponse,
    GiveUp,

    // 0x500-0x5FF 查询和订阅（请求）
    GetUser = 0x500,
    SubscribeUserList,
    SubscribeRoomList,

    // 0x1000-0x1FFF 服务器推送（无需 requestId）
    OpponentMoved = 0x1000,     // 对手落子推送
    GameEnded = 0x1001,         // 游戏结束推送
    PlayerJoined = 0x1002,      // 玩家加入推送
    PlayerLeft = 0x1003,        // 玩家退出推送
    RoomStatusChanged = 0x1004, // 房间状态推送
    DrawRequested = 0x1005,     // 请求平局推送
    DrawAccepted = 0x1006,      // 平局被接受推送
    GiveUpRequested = 0x1007,   // 认输推送

    // 0xFF00-0xFFFF 错误消息
    Success = 0xFF00,
    Error = 0xFFFF,
    InvalidMsgType,
    InvalidArg,
    UnexpectedError
};

class Packet
{
private:
    // template <typename T>
    // void WriteBytes(std::vector<uint8_t> &buffer, T value);
    // template <typename T>
    // T ReadBytes(const std::vector<uint8_t> &buffer, size_t &offset);
    std::vector<uint8_t> Serialize() const;
    bool Deserialize(const std::vector<uint8_t> &buffer);

public:
    uint64_t sessionId;
    MsgType msgType;
    MapType params;

    Packet();
    Packet(uint64_t sessionId, MsgType msgType);
    ~Packet();

    bool FromData(uint64_t sessionId, const std::vector<uint8_t> &data);

    // 解包 API
    template <typename T>
    T GetParam(const std::string &key, const T &defaultValue = T()) const
    {
        auto it = params.find(key);
        if (it == params.end())
        {
            return defaultValue;
        }

        // 尝试直接提取类型 T
        if (const T *p = std::get_if<T>(&it->second))
        {
            return *p;
        }

        return defaultValue;
    }

    // bool CheckParam(const std::string &key) const;

    MapType GetParams() const;

    // 组包 API
    // int SetParams(const MapType &params);
    int AddParam(const std::string &key, const ValueType &value);
    int ClearParams();
    std::vector<uint8_t> ToBytes() const;
};

#endif // PROTOCOL_H