#ifndef NOTIFIER_H
#define NOTIFIER_H

#include "EventBus.hpp"
#include "Packet.h"
#include <memory>
#include <functional>
#include <vector>

class ObjectManager;
class Room;

class Notifier
{
private:
    ObjectManager &objMgr;
    std::vector<std::shared_ptr<void>> tokens;

    // 回调函数：向客户端发包
    std::function<void(const Packet &)> sendPacketCb;

    // 监听事件的处理函数
    void OnPlayerJoined(uint64_t roomId, uint64_t userId);
    void OnPlayerLeft(uint64_t roomId, uint64_t userId);
    void OnPiecePlaced(uint64_t roomId, uint64_t userId, uint32_t x, uint32_t y);
    void OnGameStarted(uint64_t roomId);
    void OnGameEnded(uint64_t roomId, uint64_t winnerId);
    void OnRoomStatusChanged(uint64_t roomId, uint64_t userId, const std::string &status);
    void OnDrawRequested(uint64_t roomId, uint64_t userId);
    void OnDrawAccepted(uint64_t roomId, uint64_t userId);
    void OnGiveUpRequested(uint64_t roomId, uint64_t userId);
    void OnRoomCreated(uint64_t roomId, uint64_t ownerId);
    void OnUserLoggedIn(uint64_t userId);
    void OnRoomListUpdated();
    void OnChatMessageRecv(uint64_t roomId, uint64_t userId, const std::string &message);
    void OnRoomSync(uint64_t roomId);
    void OnGameSync(uint64_t roomId);

    // 辅助函数：通过 sessionId 发送推送消息
    void BroadcastToRoom(uint64_t roomId, const Packet &packet);
    void SendToSession(uint64_t sessionId, const Packet &packet);

    // 房间状态广播辅助函数
    void SendBoardStateToRoom(Room *room);
    void SendPlayerListToRoom(Room *room);
    void SendColorAssignmentToRoom(Room *room);

    // 广播辅助函数
    void BroadcastUserListUpdate();
    void BroadcastRoomListUpdate();

public:
    Notifier(ObjectManager &objMgr);
    ~Notifier();

    // 注册回调函数（Server 调用此方法提供发包回调）
    void SetSendPacketCallback(std::function<void(const Packet &)> cb);
};

#endif
