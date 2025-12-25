#include "Notifier.h"
#include "ObjectManager.h"
#include "Logger.h"

Notifier::Notifier(ObjectManager &objMgr)
    : objMgr(objMgr)
{
    // 订阅所有游戏事件
    auto token1 = EventBus<Event>::GetInstance().Subscribe<uint64_t, uint64_t>(Event::PlayerJoined,
                                                                               std::function<void(uint64_t, uint64_t)>(
                                                                                   [this](uint64_t roomId, uint64_t userId)
                                                                                   { OnPlayerJoined(roomId, userId); }));
    auto token2 = EventBus<Event>::GetInstance().Subscribe<uint64_t, uint64_t>(Event::PlayerLeft,
                                                                               std::function<void(uint64_t, uint64_t)>(
                                                                                   [this](uint64_t roomId, uint64_t userId)
                                                                                   { OnPlayerLeft(roomId, userId); }));
    auto token3 = EventBus<Event>::GetInstance().Subscribe<uint64_t, uint64_t, uint32_t, uint32_t>(Event::PiecePlaced,
                                                                                                   std::function<void(uint64_t, uint64_t, uint32_t, uint32_t)>(
                                                                                                       [this](uint64_t roomId, uint64_t userId, uint32_t x, uint32_t y)
                                                                                                       { OnPiecePlaced(roomId, userId, x, y); }));
    auto token4 = EventBus<Event>::GetInstance().Subscribe<uint64_t, uint64_t>(Event::GameEnded,
                                                                               std::function<void(uint64_t, uint64_t)>(
                                                                                   [this](uint64_t roomId, uint64_t winnerId)
                                                                                   { OnGameEnded(roomId, winnerId); }));
    auto token5 = EventBus<Event>::GetInstance().Subscribe<uint64_t, uint64_t, const std::string &>(Event::RoomStatusChanged,
                                                                                                    std::function<void(uint64_t, uint64_t, const std::string &)>(
                                                                                                        [this](uint64_t roomId, uint64_t userId, const std::string &status)
                                                                                                        { OnRoomStatusChanged(roomId, userId, status); }));
    auto token6 = EventBus<Event>::GetInstance().Subscribe<uint64_t, uint64_t>(Event::DrawRequested,
                                                                               std::function<void(uint64_t, uint64_t)>(
                                                                                   [this](uint64_t roomId, uint64_t userId)
                                                                                   { OnDrawRequested(roomId, userId); }));
    auto token7 = EventBus<Event>::GetInstance().Subscribe<uint64_t, uint64_t>(Event::DrawAccepted,
                                                                               std::function<void(uint64_t, uint64_t)>(
                                                                                   [this](uint64_t roomId, uint64_t userId)
                                                                                   { OnDrawAccepted(roomId, userId); }));
    auto token8 = EventBus<Event>::GetInstance().Subscribe<uint64_t, uint64_t>(Event::GiveUpRequested,
                                                                               std::function<void(uint64_t, uint64_t)>(
                                                                                   [this](uint64_t roomId, uint64_t userId)
                                                                                   { OnGiveUpRequested(roomId, userId); }));
    auto token9 = EventBus<Event>::GetInstance().Subscribe<uint64_t, uint64_t>(Event::RoomCreated,
                                                                               std::function<void(uint64_t, uint64_t)>(
                                                                                   [this](uint64_t roomId, uint64_t ownerId)
                                                                                   { OnRoomCreated(roomId, ownerId); }));
    auto token10 = EventBus<Event>::GetInstance().Subscribe<uint64_t>(Event::UserLoggedIn,
                                                                      std::function<void(uint64_t)>(
                                                                          [this](uint64_t userId)
                                                                          { OnUserLoggedIn(userId); }));
    auto token11 = EventBus<Event>::GetInstance().Subscribe(Event::RoomListUpdated,
                                                            std::function<void()>(
                                                                [this]()
                                                                { OnRoomListUpdated(); }));
    auto token12 = EventBus<Event>::GetInstance().Subscribe<uint64_t>(Event::GameStarted,
                                                                      std::function<void(uint64_t)>(
                                                                          [this](uint64_t roomId)
                                                                          { OnGameStarted(roomId); }));
    auto token13 = EventBus<Event>::GetInstance().Subscribe<uint64_t, uint64_t, const std::string &>(Event::ChatMessageRecv,
                                                                                                     std::function<void(uint64_t, uint64_t, const std::string &)>(
                                                                                                         [this](uint64_t roomId, uint64_t userId, const std::string &message)
                                                                                                         { OnChatMessageRecv(roomId, userId, message); }));
    auto token14 = EventBus<Event>::GetInstance().Subscribe<uint64_t>(Event::RoomSync,
                                                                      std::function<void(uint64_t)>(
                                                                          [this](uint64_t roomId)
                                                                          { OnRoomSync(roomId); }));
    auto token15 = EventBus<Event>::GetInstance().Subscribe<uint64_t>(Event::GameSync,
                                                                      std::function<void(uint64_t)>(
                                                                          [this](uint64_t roomId)
                                                                          { OnGameSync(roomId); }));

    // 保存令牌以防止过早销毁
    tokens.push_back(token1);
    tokens.push_back(token2);
    tokens.push_back(token3);
    tokens.push_back(token4);
    tokens.push_back(token5);
    tokens.push_back(token6);
    tokens.push_back(token7);
    tokens.push_back(token8);
    tokens.push_back(token9);
    tokens.push_back(token10);
    tokens.push_back(token11);
    tokens.push_back(token12);
    tokens.push_back(token13);
    tokens.push_back(token14);
    tokens.push_back(token15);
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
    // 使用 SyncUsersToRoom 通知玩家加入
    Packet push(0, MsgType::SyncUsersToRoom);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);
    push.AddParam("action", "joined");

    BroadcastToRoom(roomId, push);
}

void Notifier::OnPlayerLeft(uint64_t roomId, uint64_t userId)
{
    Packet push(0, MsgType::SyncUsersToRoom);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);

    BroadcastToRoom(roomId, push);
}

void Notifier::OnPiecePlaced(uint64_t roomId, uint64_t userId, uint32_t x, uint32_t y)
{
    // 使用 MakeMove 推送，因为棋子放置就是落子
    Packet push(0, MsgType::MakeMove);
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
    // 使用 SyncGame 通知房间状态变化
    Packet push(0, MsgType::SyncGame);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);
    push.AddParam("status", status);

    BroadcastToRoom(roomId, push);
}

void Notifier::OnDrawRequested(uint64_t roomId, uint64_t userId)
{
    // 使用 Draw 推送
    Packet push(0, MsgType::Draw);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);
    push.AddParam("action", "request");

    BroadcastToRoom(roomId, push);
}

void Notifier::OnDrawAccepted(uint64_t roomId, uint64_t userId)
{
    // 使用 Draw 推送
    Packet push(0, MsgType::Draw);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);
    push.AddParam("action", "accept");

    BroadcastToRoom(roomId, push);
}

void Notifier::OnGiveUpRequested(uint64_t roomId, uint64_t userId)
{
    // 使用 GiveUp 推送
    Packet push(0, MsgType::GiveUp);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);

    BroadcastToRoom(roomId, push);
}

// --- 房间创建事件处理 ---

void Notifier::OnRoomCreated(uint64_t roomId, uint64_t ownerId)
{
    Room *room = objMgr.GetRoom(roomId);
    if (!room)
    {
        LOG_WARN("Room " + std::to_string(roomId) + " not found for RoomCreated event");
        return;
    }

    LOG_INFO("Room created: roomId=" + std::to_string(roomId) + ", ownerId=" + std::to_string(ownerId));

    // 1. 广播房间状态变化消息
    Packet roomStatusPush(0, MsgType::SyncGame);
    roomStatusPush.AddParam("roomId", roomId);
    roomStatusPush.AddParam("userId", ownerId);
    roomStatusPush.AddParam("status", "created");
    roomStatusPush.AddParam("boardSize", (uint32_t)15); // TODO: 从room获取实际棋盘大小
    BroadcastToRoom(roomId, roomStatusPush);

    // 2. 广播棋盘状态
    SendBoardStateToRoom(room);

    // 3. 广播玩家列表
    SendPlayerListToRoom(room);

    // 4. 广播黑白棋手分配
    SendColorAssignmentToRoom(room);
}

void Notifier::SendBoardStateToRoom(Room *room)
{
    if (!room)
        return;

    // 使用 SyncGame 推送棋盘状态
    Packet boardStatePush(0, MsgType::SyncGame);
    boardStatePush.AddParam("roomId", room->GetRoomId());
    boardStatePush.AddParam("boardSize", (uint32_t)15); // TODO: 从room获取实际棋盘大小

    // TODO: 添加棋盘数据序列化
    // 目前先发送空棋盘状态

    LOG_DEBUG("Broadcasting board state for room " + std::to_string(room->GetRoomId()));
    BroadcastToRoom(room->GetRoomId(), boardStatePush);
}

void Notifier::SendPlayerListToRoom(Room *room)
{
    if (!room)
        return;

    // 使用 SyncUsersToRoom 推送玩家列表
    Packet playerListPush(0, MsgType::SyncUsersToRoom);
    playerListPush.AddParam("roomId", room->GetRoomId());
    playerListPush.AddParam("playerCount", (uint32_t)room->playerIds.size());

    // TODO: 添加玩家ID列表序列化
    // 目前先发送玩家数量

    LOG_DEBUG("Broadcasting player list for room " + std::to_string(room->GetRoomId()) +
              " with " + std::to_string(room->playerIds.size()) + " players");
    BroadcastToRoom(room->GetRoomId(), playerListPush);
}

void Notifier::SendColorAssignmentToRoom(Room *room)
{
    if (!room)
        return;

    // 使用 SyncSeat 推送颜色分配
    Packet seatPush(0, MsgType::SyncSeat);
    seatPush.AddParam("roomId", room->GetRoomId());
    seatPush.AddParam("blackPlayerId", room->blackPlayerId);
    seatPush.AddParam("whitePlayerId", room->whitePlayerId);

    LOG_DEBUG("Broadcasting color assignment for room " + std::to_string(room->GetRoomId()) +
              ": black=" + std::to_string(room->blackPlayerId) +
              ", white=" + std::to_string(room->whitePlayerId));
    BroadcastToRoom(room->GetRoomId(), seatPush);
}

// --- 用户登录事件处理 ---

void Notifier::OnUserLoggedIn(uint64_t userId)
{
    LOG_INFO("User logged in: userId=" + std::to_string(userId));

    // 广播用户列表更新给所有在线用户
    BroadcastUserListUpdate();
}

// --- 房间列表更新事件处理 ---

void Notifier::OnRoomListUpdated()
{
    LOG_INFO("Room list updated, broadcasting to all online users");

    // 广播房间列表更新给所有在线用户
    BroadcastRoomListUpdate();
}

// --- 广播辅助函数 ---

void Notifier::BroadcastUserListUpdate()
{
    // 获取所有用户列表
    std::vector<User *> allUsers = objMgr.GetUserList(1000); // 获取大量用户，确保覆盖所有

    // 统计在线用户数量
    size_t onlineCount = 0;

    // 为每个在线用户发送用户列表更新
    for (User *user : allUsers)
    {
        if (!user)
            continue;

        uint64_t userId = user->GetID();
        uint64_t sessionId = objMgr.GetSessionIdByUserId(userId);

        if (sessionId == 0)
            continue; // 用户不在线

        onlineCount++;

        // 创建用户列表包
        Packet userListPush(sessionId, MsgType::updateUsersToLobby);

        // 获取用户列表（前10个）
        std::vector<User *> userList = objMgr.GetUserList(10);

        // 构造用户列表字符串（与Handler::OnGetUserList相同的逻辑）
        std::string userListStr;
        for (size_t i = 0; i < userList.size(); i++)
        {
            User *listUser = userList[i];
            if (listUser)
            {
                // 判断用户是否在线
                uint64_t userSessionId = objMgr.GetSessionIdByUserId(listUser->GetID());
                std::string status = (userSessionId != 0) ? "在线" : "离线";

                userListStr += listUser->GetUsername() + " (" + status + ")";

                if (i != userList.size() - 1)
                {
                    userListStr += ", ";
                }
            }
        }

        userListPush.AddParam("userList", userListStr);
        userListPush.AddParam("count", uint32_t(userList.size()));

        // 发送给该session
        SendToSession(sessionId, userListPush);
    }

    if (onlineCount == 0)
    {
        LOG_DEBUG("No online users to broadcast user list update");
    }
    else
    {
        LOG_DEBUG("Broadcast user list update to " + std::to_string(onlineCount) + " online users");
    }
}

void Notifier::BroadcastRoomListUpdate()
{
    // 获取所有用户列表
    std::vector<User *> allUsers = objMgr.GetUserList(1000); // 获取大量用户，确保覆盖所有

    // 统计在线用户数量
    size_t onlineCount = 0;

    // 为每个在线用户发送房间列表更新
    for (User *user : allUsers)
    {
        if (!user)
            continue;

        uint64_t userId = user->GetID();
        uint64_t sessionId = objMgr.GetSessionIdByUserId(userId);

        if (sessionId == 0)
            continue; // 用户不在线

        onlineCount++;

        // 创建房间列表包
        Packet roomListPush(sessionId, MsgType::updateRoomsToLobby);

        // 从ObjectManager获取房间列表
        std::vector<Room *> roomList = objMgr.GetRoomList(10); // 默认获取前10个房间

        // 构造房间列表字符串（与Handler::OnGetRoomList相同的逻辑）
        std::string roomListStr;
        for (size_t i = 0; i < roomList.size(); i++)
        {
            Room *room = roomList[i];
            if (room)
            {
                // 获取房间状态
                std::string statusStr;
                switch (room->status)
                {
                case RoomStatus::Free:
                    statusStr = "空闲";
                    break;
                case RoomStatus::Playing:
                    statusStr = "对战中";
                    break;
                case RoomStatus::End:
                    statusStr = "已结束";
                    break;
                default:
                    statusStr = "未知";
                    break;
                }

                // 获取房间描述
                std::string description;
                if (room->status == RoomStatus::Playing)
                {
                    // 对战中：显示对战双方
                    User *blackUser = objMgr.GetUserByUserId(room->blackPlayerId);
                    User *whiteUser = objMgr.GetUserByUserId(room->whitePlayerId);

                    std::string blackName = blackUser ? blackUser->GetUsername() : "未知玩家";
                    std::string whiteName = whiteUser ? whiteUser->GetUsername() : "未知玩家";
                    description = blackName + " vs " + whiteName;
                }
                else if (room->status == RoomStatus::Free)
                {
                    // 空闲：显示房主信息
                    User *owner = objMgr.GetUserByUserId(room->ownerId);
                    std::string ownerName = owner ? owner->GetUsername() : "未知玩家";
                    description = ownerName + " (等待对手)";
                }
                else
                {
                    description = "房间已结束";
                }

                // 格式： "#001", "空闲", "等待玩家加入"
                roomListStr += "#" + std::to_string(room->GetRoomId()) + ", " +
                               statusStr + ", " + description;

                if (i != roomList.size() - 1)
                {
                    roomListStr += ", ";
                }
            }
        }

        roomListPush.AddParam("roomList", roomListStr);
        roomListPush.AddParam("count", uint32_t(roomList.size()));

        // 发送给该session
        SendToSession(sessionId, roomListPush);
    }

    if (onlineCount == 0)
    {
        LOG_DEBUG("No online users to broadcast room list update");
    }
    else
    {
        LOG_DEBUG("Broadcast room list update to " + std::to_string(onlineCount) + " online users");
    }
}

// --- 新增的推送消息处理函数 ---

void Notifier::OnGameStarted(uint64_t roomId)
{
    Packet push(0, MsgType::GameStarted);
    push.AddParam("roomId", roomId);

    BroadcastToRoom(roomId, push);
}

void Notifier::OnChatMessageRecv(uint64_t roomId, uint64_t userId, const std::string &message)
{
    // 使用 ChatMessage 推送聊天消息
    Packet push(0, MsgType::ChatMessage);
    push.AddParam("roomId", roomId);
    push.AddParam("userId", userId);
    push.AddParam("message", message);

    BroadcastToRoom(roomId, push);
}

void Notifier::OnRoomSync(uint64_t roomId)
{
    // 房间同步可以使用 SyncGame
    Packet push(0, MsgType::SyncGame);
    push.AddParam("roomId", roomId);

    // TODO: 添加房间同步数据
    BroadcastToRoom(roomId, push);
}

void Notifier::OnGameSync(uint64_t roomId)
{
    // 游戏同步可以使用 SyncGame
    Packet push(0, MsgType::SyncGame);
    push.AddParam("roomId", roomId);

    // TODO: 添加游戏同步数据
    BroadcastToRoom(roomId, push);
}
