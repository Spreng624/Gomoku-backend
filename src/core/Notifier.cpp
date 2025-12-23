#include "Notifier.h"
#include "ObjectManager.h"
#include "Logger.h"

Notifier::Notifier(EventBus<Event> &eventBus, ObjectManager &objMgr)
    : eventBus(eventBus), objMgr(objMgr)
{
    // 订阅所有游戏事件
    auto token1 = eventBus.Subscribe<uint64_t, uint64_t>(Event::PlayerJoined,
                                                         std::function<void(uint64_t, uint64_t)>(
                                                             [this](uint64_t roomId, uint64_t userId)
                                                             { OnPlayerJoined(roomId, userId); }));
    auto token2 = eventBus.Subscribe<uint64_t, uint64_t>(Event::PlayerLeft,
                                                         std::function<void(uint64_t, uint64_t)>(
                                                             [this](uint64_t roomId, uint64_t userId)
                                                             { OnPlayerLeft(roomId, userId); }));
    auto token3 = eventBus.Subscribe<uint64_t, uint64_t, uint32_t, uint32_t>(Event::OpponentMoved,
                                                                             std::function<void(uint64_t, uint64_t, uint32_t, uint32_t)>(
                                                                                 [this](uint64_t roomId, uint64_t userId, uint32_t x, uint32_t y)
                                                                                 { OnOpponentMoved(roomId, userId, x, y); }));
    auto token4 = eventBus.Subscribe<uint64_t, uint64_t>(Event::GameEnded,
                                                         std::function<void(uint64_t, uint64_t)>(
                                                             [this](uint64_t roomId, uint64_t winnerId)
                                                             { OnGameEnded(roomId, winnerId); }));
    auto token5 = eventBus.Subscribe<uint64_t, uint64_t, const std::string &>(Event::RoomStatusChanged,
                                                                              std::function<void(uint64_t, uint64_t, const std::string &)>(
                                                                                  [this](uint64_t roomId, uint64_t userId, const std::string &status)
                                                                                  { OnRoomStatusChanged(roomId, userId, status); }));
    auto token6 = eventBus.Subscribe<uint64_t, uint64_t>(Event::DrawRequested,
                                                         std::function<void(uint64_t, uint64_t)>(
                                                             [this](uint64_t roomId, uint64_t userId)
                                                             { OnDrawRequested(roomId, userId); }));
    auto token7 = eventBus.Subscribe<uint64_t, uint64_t>(Event::DrawAccepted,
                                                         std::function<void(uint64_t, uint64_t)>(
                                                             [this](uint64_t roomId, uint64_t userId)
                                                             { OnDrawAccepted(roomId, userId); }));
    auto token8 = eventBus.Subscribe<uint64_t, uint64_t>(Event::GiveUpRequested,
                                                         std::function<void(uint64_t, uint64_t)>(
                                                             [this](uint64_t roomId, uint64_t userId)
                                                             { OnGiveUpRequested(roomId, userId); }));

    // 保存令牌以防止过早销毁
    tokens.push_back(token1);
    tokens.push_back(token2);
    tokens.push_back(token3);
    tokens.push_back(token4);
    tokens.push_back(token5);
    tokens.push_back(token6);
    tokens.push_back(token7);
    tokens.push_back(token8);
}

Notifier::~Notifier()
{
}

void Notifier::SetSendPacketCallback(std::function<void(const Packet &)> cb)
{
    sendPacketCb = cb;
}

void Notifier::OnPlayerJoined(uint64_t roomId, uint64_t userId)
{
    // 创建推送包（事件驱动架构，不需要requestId）
    // 注意：sessionId 应该在 BroadcastToRoom 中设置，这里先用 0
    Packet push(0, MsgType::PlayerJoined);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);

    BroadcastToRoom(roomId, push);
}

void Notifier::OnPlayerLeft(uint64_t roomId, uint64_t userId)
{
    Packet push(0, MsgType::PlayerLeft);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);

    BroadcastToRoom(roomId, push);
}

void Notifier::OnOpponentMoved(uint64_t roomId, uint64_t userId, uint32_t x, uint32_t y)
{
    Packet push(0, MsgType::OpponentMoved);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);
    push.AddParam("x", x);
    push.AddParam("y", y);

    BroadcastToRoom(roomId, push);
}

void Notifier::OnGameEnded(uint64_t roomId, uint64_t winnerId)
{
    Packet push(0, MsgType::GameEnded);
    push.AddParam("roomId", roomId);
    push.AddParam("winnerId", winnerId);

    BroadcastToRoom(roomId, push);
}

void Notifier::BroadcastToRoom(uint64_t roomId, const Packet &packet)
{
    // 查询房间对象
    Room *room = objMgr.GetRoom(roomId);
    if (!room)
    {
        LOG_WARN("Room " + std::to_string(roomId) + " not found");
        return;
    }

    // 遍历房间内所有玩家，逐个发送推送消息
    for (uint64_t userId : room->playerIds)
    {
        uint64_t sessionId = objMgr.GetSessionIdByUserId(userId);
        if (sessionId != 0)
        {
            SendToSession(sessionId, packet);
        }
    }
}

void Notifier::SendToSession(uint64_t sessionId, const Packet &packet)
{
    if (!sendPacketCb)
    {
        LOG_ERROR("SendPacketCallback not set!");
        return;
    }

    // 创建会话级的包副本，设置正确的 sessionId
    Packet sessionPacket = packet;
    sessionPacket.sessionId = sessionId;
    sendPacketCb(sessionPacket);
}

// 新增的推送消息处理函数

void Notifier::OnRoomStatusChanged(uint64_t roomId, uint64_t userId, const std::string &status)
{
    Packet push(0, MsgType::RoomStatusChanged);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);
    push.AddParam("status", status);

    BroadcastToRoom(roomId, push);
}

void Notifier::OnDrawRequested(uint64_t roomId, uint64_t userId)
{
    Packet push(0, MsgType::DrawRequested);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);

    BroadcastToRoom(roomId, push);
}

void Notifier::OnDrawAccepted(uint64_t roomId, uint64_t userId)
{
    Packet push(0, MsgType::DrawAccepted);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);

    BroadcastToRoom(roomId, push);
}

void Notifier::OnGiveUpRequested(uint64_t roomId, uint64_t userId)
{
    Packet push(0, MsgType::GiveUpRequested);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);

    BroadcastToRoom(roomId, push);
}
