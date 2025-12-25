#ifndef HANDLER_H
#define HANDLER_H

#include "Packet.h"
#include "EventBus.hpp"
#include <memory>
#include <functional>

class ObjectManager;
class Room;
class User;

class Handler
{
private:
    ObjectManager &objMgr;
    std::function<void(const Packet &)> sendCallback;

    // 分组处理方法 - 按MsgType分段
    void HandleAuthPacket(const Packet &packet);         // 100-199: 账户操作
    void HandleLobbyPacket(const Packet &packet);        // 200-299: 大厅操作
    void HandleRoomPacket(const Packet &packet);         // 300-399: 房间操作
    void HandleGamePacket(const Packet &packet);         // 400-499: 游戏操作
    void HandleNotificationPacket(const Packet &packet); // 推送消息

    // 辅助函数（暂时保留，后续改为事件发布）
    User *GetUserBySessionId(uint64_t sessionId);
    uint64_t GetUserRoomId(User *user);
    void SendResponse(const Packet &request, MsgType responseType, const MapType &params = {});
    void SendError(const Packet &request, const std::string &errMsg);

    // 房间状态发送辅助函数
    void SendRoomStateToPlayer(uint64_t sessionId, Room *room);
    void SendBoardState(uint64_t sessionId, Room *room);
    void SendPlayerList(uint64_t sessionId, Room *room);
    void SendColorAssignment(uint64_t sessionId, Room *room);

public:
    Handler(ObjectManager &objMgr, std::function<void(const Packet &)> sendCallback);
    ~Handler();

    void HandlePacket(const Packet &packet);
};

#endif
