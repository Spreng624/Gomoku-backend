#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <vector>
#include <variant>
#include <string>
#include <map>

#define BU99ER_SIZE 4096

using ValueType = std::variant<
    int, uint8_t, uint32_t, uint64_t,
    std::string, bool, std::vector<uint8_t>>;
using MapType = std::map<std::string, ValueType>;

// 协商
enum class NegStatus : uint8_t
{
    Ask,    // 发起请求
    Accept, // 同意
    Reject  // 拒绝
};

// 消息类型（注：Sync代表全量同步，作为请求时意味着需要同步）
enum MsgType : uint32_t
{
    // 空消息, 可用作心跳包
    None = 0,

    // 100-199 账户操作
    Login = 100,  // username, password --> success, (username, rating) / error
    SignIn,       // username, password --> success, (username, rating) / error
    LoginAsGuest, // None               --> success, (username, rating) / error
    LogOut,       // None               --> success, None / error

    // 200-299 大厅操作
    CreateRoom = 200,   // None   --> success, None / error
    JoinRoom,           // roomId --> success, None / error
    QuickMatch,         // None   --> success, None / error
    updateUsersToLobby, // None   --> success, userList / error
                        //        <-- userList
    updateRoomsToLobby, // None   --> success, roomList / error
                        //        <-- roomList

    // 300-399 房间内部操作
    SyncSeat = 300,  // P1, P2  --> success, (P1, P2) / error
                     //         <-- P1, P2
    SyncRoomSetting, // config  --> success, None / error
                     //         <-- config
    ChatMessage,     // msg     --> success, None / error
                     //         <-- username, msg
    SyncUsersToRoom, //         <-- playerListStr
    ExitRoom,        // None    --> success, None / error

    // 400-499 游戏操作
    GameStarted,    // None      --> success, None / error
                    //           <-- None
    GameEnded,      //           <-- msg    唯一无请求的消息
    MakeMove = 400, // x, y      --> success, None / error
                    //           <-- x, y
    GiveUp,         // None      --> success, None / error
    Draw,           // negStatus --> success, None / error
                    //           <-- negStatus
    UndoMove,       // negStatus --> success, None / error
                    //           <-- negStatus
    SyncGame,       // None      --> success, None / error
                    //           <-- statusStr 一个 配置||行棋历史 的字符串搞定同步问题

    // 9999 错误
    Error = 9999,
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
