#include "Handler.h"
#include "ObjectManager.h"
#include "Logger.h"

Handler::Handler(ObjectManager &objMgr, std::function<void(const Packet &)> sendCallback)
    : objMgr(objMgr), sendCallback(sendCallback)
{
}

Handler::~Handler() {}

// 分组处理方法实现
void Handler::HandleAuthPacket(const Packet &packet)
{
    std::string username = packet.GetParam<std::string>("username");
    std::string password = packet.GetParam<std::string>("password");
    switch (packet.msgType)
    {
    case MsgType::Login:
    {
        LOG_INFO("Login attempt for user: " + username);

        // 业务逻辑：验证用户
        User *user = objMgr.GetUserByUsername(username);
        if (!user || user->GetPassword() != password)
        {
            LOG_WARN("Login failed for user: " + username + " - Invalid username or password");
            LOG_DEBUG("Provided password: " + password + ", Expected password: " + (user ? user->GetPassword() : "N/A"));
            SendError(packet, "Invalid username or password");
            return;
        }

        // 映射 sessionId 到 userId
        objMgr.MapSessionToUser(packet.sessionId, user->GetID());

        LOG_INFO("Login successful for user: " + username + " (ID: " + std::to_string(user->GetID()) + ")");

        MapType response;
        response["success"] = true;
        response["username"] = username;
        response["rating"] = user->GetRanking();
        SendResponse(packet, MsgType::Login, response);

        // 发布用户登录事件，触发用户列表广播
        EventBus<Event>::GetInstance().Publish(Event::UserLoggedIn, user->GetID());
        return;
    }
    case MsgType::SignIn:
    {
        // 业务逻辑：注册新用户
        User *user = objMgr.CreateUser(username, password);
        if (!user)
        {
            SendError(packet, "Username already exists");
            return;
        }

        objMgr.MapSessionToUser(packet.sessionId, user->GetID());

        MapType response;
        response["success"] = true;
        response["username"] = username;
        response["rating"] = user->GetRanking();
        SendResponse(packet, MsgType::SignIn, response);

        // 发布用户登录事件，触发用户列表广播
        EventBus<Event>::GetInstance().Publish(Event::UserLoggedIn, user->GetID());
        return;
    }
    case MsgType::LoginAsGuest:
    {
        // 业务逻辑：创建游客账户（简化处理，直接生成 ID）
        uint64_t guestId = 1000000 + packet.sessionId; // 简单示例

        objMgr.MapSessionToUser(packet.sessionId, guestId);

        MapType response;
        response["success"] = true;
        response["username"] = "Guest_" + std::to_string(guestId);
        response["rating"] = 0; // 默认 rating
        SendResponse(packet, MsgType::LoginAsGuest, response);

        // 发布用户登录事件，触发用户列表广播
        EventBus<Event>::GetInstance().Publish(Event::UserLoggedIn, guestId);
        return;
    }
    case MsgType::LogOut:
    {
        // 业务逻辑：注销用户
        uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
        if (userId != 0)
            objMgr.UnmapSession(packet.sessionId);

        MapType response;
        response["success"] = true;
        SendResponse(packet, MsgType::LogOut, response);
        return;
    }
    default:
        LOG_DEBUG("Unhandled auth MsgType: " + std::to_string(static_cast<uint32_t>(packet.msgType)));
        SendError(packet, "Unhandled authentication message type");
        break;
    }
}

void Handler::HandleLobbyPacket(const Packet &packet)
{
    switch (packet.msgType)
    {
    case MsgType::CreateRoom:
    {
        User *user = GetUserBySessionId(packet.sessionId);
        if (!user)
        {
            LOG_WARN("Create room failed: User not logged in (sessionId: " + std::to_string(packet.sessionId) + ")");
            SendError(packet, "Not logged in");
            return;
        }

        LOG_INFO("Creating room for user ID: " + std::to_string(user->GetID()));

        // 业务逻辑：创建房间
        Room *room = objMgr.CreateRoom(user->GetID());
        if (!room)
        {
            LOG_ERROR("Failed to create room for user ID: " + std::to_string(user->GetID()));
            SendError(packet, "Failed to create room");
            return;
        }

        // 将房主添加到房间
        if (!room->AddPlayer(user->GetID()))
        {
            LOG_ERROR("Failed to add owner to room: " + room->GetError());
            SendError(packet, "Failed to create room: " + room->GetError());
            return;
        }

        // 更新用户到房间的映射
        objMgr.MapUserToRoom(user->GetID(), room->GetRoomId());

        LOG_INFO("Room created successfully: roomId=" + std::to_string(room->GetRoomId()) +
                 ", ownerId=" + std::to_string(user->GetID()));

        MapType response;
        response["roomId"] = uint64_t(room->GetRoomId());
        SendResponse(packet, MsgType::CreateRoom, response);

        // 发布房间创建事件，触发广播
        EventBus<Event>::GetInstance().Publish(Event::RoomCreated, room->GetRoomId(), user->GetID());

        // 发布房间列表更新事件，触发房间列表广播
        EventBus<Event>::GetInstance().Publish(Event::RoomListUpdated);
        return;
    }
    case MsgType::JoinRoom:
    {
        User *user = GetUserBySessionId(packet.sessionId);
        if (!user)
        {
            SendError(packet, "Not logged in");
            return;
        }

        uint32_t roomId = packet.GetParam<uint32_t>("roomId");
        Room *room = objMgr.GetRoom(roomId);
        if (!room)
        {
            SendError(packet, "Room not found");
            return;
        }

        // 业务逻辑：加入房间
        if (!room->AddPlayer(user->GetID()))
        {
            SendError(packet, "Failed to join room: " + room->GetError());
            return;
        }

        // 更新用户到房间的映射
        objMgr.MapUserToRoom(user->GetID(), roomId);

        // 发送加入房间响应
        MapType response;
        response["roomId"] = roomId;
        response["success"] = true;
        SendResponse(packet, MsgType::JoinRoom, response);

        // 发布玩家加入事件（广播给其他玩家）
        EventBus<Event>::GetInstance().Publish(Event::PlayerJoined, roomId, user->GetID());
        return;
    }
    case MsgType::QuickMatch:
    {
        uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
        if (userId == 0)
        {
            SendError(packet, "Not logged in");
            return;
        }

        // 业务逻辑：快速匹配
        // 这里需要实现匹配逻辑，目前先返回错误
        SendError(packet, "Quick match not implemented yet");
        return;
    }
    case MsgType::updateUsersToLobby:
    {
        // 获取最大数量参数，默认为10
        size_t maxCount = packet.GetParam<uint32_t>("maxCount", 10);

        // 从 ObjectManager 获取用户列表
        std::vector<User *> userList = objMgr.GetUserList(maxCount);

        // 构造用户列表字符串
        std::string userListStr;
        for (size_t i = 0; i < userList.size(); i++)
        {
            User *user = userList[i];
            if (user)
            {
                // 判断用户是否在线
                uint64_t sessionId = objMgr.GetSessionIdByUserId(user->GetID());
                std::string status = (sessionId != 0) ? "在线" : "离线";

                // 格式： "玩家1 (在线)"
                userListStr += user->GetUsername() + " (" + status + ")";

                if (i != userList.size() - 1)
                {
                    userListStr += ", ";
                }
            }
        }

        LOG_DEBUG("User list requested, returning " + std::to_string(userList.size()) + " users");

        MapType response;
        response["userList"] = userListStr;
        response["count"] = uint32_t(userList.size());
        SendResponse(packet, MsgType::updateUsersToLobby, response);
        return;
    }
    case MsgType::updateRoomsToLobby:
    {
        // 获取最大数量参数，默认为10
        size_t maxCount = packet.GetParam<uint32_t>("maxCount", 10);

        // 从 ObjectManager 获取房间列表
        std::vector<Room *> roomList = objMgr.GetRoomList(maxCount);

        // 构造房间列表字符串
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

                    std::string blackName = blackUser ? blackUser->GetUsername() : "";
                    std::string whiteName = whiteUser ? whiteUser->GetUsername() : "";
                    description = blackName + " vs " + whiteName;
                }
                else if (room->status == RoomStatus::Free)
                {
                    // 空闲：显示房主信息
                    User *owner = objMgr.GetUserByUserId(room->ownerId);
                    std::string ownerName = owner ? owner->GetUsername() : "";
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

        LOG_DEBUG("Room list requested, returning " + std::to_string(roomList.size()) + " rooms");

        MapType response;
        response["roomList"] = roomListStr;
        response["count"] = uint32_t(roomList.size());
        SendResponse(packet, MsgType::updateRoomsToLobby, response);
        return;
    }
    default:
        LOG_DEBUG("Unhandled lobby MsgType: " + std::to_string(static_cast<uint32_t>(packet.msgType)));
        SendError(packet, "Unhandled lobby message type");
        break;
    }
}

void Handler::HandleRoomPacket(const Packet &packet)
{
    switch (packet.msgType)
    {
    case MsgType::SyncSeat:
    {
        LOG_DEBUG("SyncSeat received");
        User *user = GetUserBySessionId(packet.sessionId);
        if (!user)
        {
            SendError(packet, "Not logged in");
            return;
        }

        // 从UserContext获取roomId
        uint64_t roomId = GetUserRoomId(user);
        if (roomId == 0)
        {
            SendError(packet, "You are not in a room");
            return;
        }

        Room *room = objMgr.GetRoom(roomId);
        if (!room)
        {
            SendError(packet, "Room not found");
            return;
        }

        std::string blackUsername = packet.GetParam<std::string>("P1", "");
        std::string whiteUsername = packet.GetParam<std::string>("P2", "");

        User *blackUser = objMgr.GetUserByUsername(blackUsername);
        User *whiteUser = objMgr.GetUserByUsername(whiteUsername);

        if (!room->SyncSeat(user->GetID(),
                            blackUser ? blackUser->GetID() : 0,
                            whiteUser ? whiteUser->GetID() : 0))
        {
            SendError(packet, "Failed to sync seat: " + room->GetError());
            return;
        }

        MapType response;
        response["success"] = true;
        SendResponse(packet, MsgType::SyncSeat, response);
        return;
    }
    case MsgType::SyncRoomSetting:
    {
        User *user = GetUserBySessionId(packet.sessionId);
        if (!user)
        {
            SendError(packet, "Not logged in");
            return;
        }

        // 从UserContext获取roomId
        uint64_t roomId = GetUserRoomId(user);
        if (roomId == 0)
        {
            SendError(packet, "You are not in a room");
            return;
        }

        Room *room = objMgr.GetRoom(roomId);
        if (!room)
        {
            SendError(packet, "Room not found");
            return;
        }

        // 直接使用packet.params而不是GetParams()
        if (!room->EditRoomSetting(user->GetID(), packet.params))
        {
            SendError(packet, "Failed to edit room setting: " + room->GetError());
            return;
        }

        MapType response;
        response["success"] = true;
        SendResponse(packet, MsgType::SyncRoomSetting, response);
        return;
    }
    case MsgType::ChatMessage:
    {
        User *user = GetUserBySessionId(packet.sessionId);
        if (!user)
        {
            SendError(packet, "Not logged in");
            return;
        }

        // 从UserContext获取roomId
        uint64_t roomId = GetUserRoomId(user);
        if (roomId == 0)
        {
            SendError(packet, "You are not in a room");
            return;
        }

        std::string message = packet.GetParam<std::string>("message");

        Room *room = objMgr.GetRoom(roomId);
        if (!room)
        {
            SendError(packet, "Room not found");
            return;
        }

        // 发送聊天消息响应
        MapType response;
        response["success"] = true;
        SendResponse(packet, MsgType::ChatMessage, response);

        // 发布聊天消息接收事件，Notifier 会处理广播
        EventBus<Event>::GetInstance().Publish(Event::ChatMessageRecv, roomId, user->GetID(), message);
        return;
    }
    case MsgType::SyncUsersToRoom:
    {
        User *user = GetUserBySessionId(packet.sessionId);
        if (!user)
        {
            SendError(packet, "Not logged in");
            return;
        }

        // 从UserContext获取roomId
        uint64_t roomId = GetUserRoomId(user);
        if (roomId == 0)
        {
            SendError(packet, "You are not in a room");
            return;
        }

        Room *room = objMgr.GetRoom(roomId);
        if (!room)
        {
            SendError(packet, "Room not found");
            return;
        }

        // 构造玩家列表字符串
        std::string playerListStr;
        for (size_t i = 0; i < room->playerIds.size(); i++)
        {
            uint64_t playerId = room->playerIds[i];
            User *player = objMgr.GetUserByUserId(playerId);
            if (player)
            {
                playerListStr += player->GetUsername();
                // 添加在线状态
                uint64_t sessionId = objMgr.GetSessionIdByUserId(playerId);
                if (sessionId != 0)
                    playerListStr += " (在线)";
                else
                    playerListStr += " (离线)";
            }
            else
            {
                playerListStr += "";
            }

            if (i != room->playerIds.size() - 1)
                playerListStr += ", ";
        }

        MapType response;
        response["playerListStr"] = playerListStr;
        SendResponse(packet, MsgType::SyncUsersToRoom, response);
        return;
    }
    case MsgType::ExitRoom:
    {
        User *user = GetUserBySessionId(packet.sessionId);
        if (!user)
        {
            SendError(packet, "Not logged in");
            return;
        }

        uint64_t roomId = objMgr.GetRoomIdByUserId(user->GetID());
        Room *room = objMgr.GetRoom(roomId);

        // 从房间映射中移除用户
        objMgr.UnmapUserFromRoom(user->GetID());

        MapType response;
        response["success"] = true;
        SendResponse(packet, MsgType::ExitRoom, response);

        // 发布玩家离开事件（事件驱动架构）
        EventBus<Event>::GetInstance().Publish(Event::PlayerLeft, roomId, user->GetID());
        return;
    }
    default:
        LOG_DEBUG("Unhandled room MsgType: " + std::to_string(static_cast<uint32_t>(packet.msgType)));
        SendError(packet, "Unhandled room message type");
        break;
    }
}

void Handler::HandleGamePacket(const Packet &packet)
{
    switch (packet.msgType)
    {
    case MsgType::GameStarted:
    {
        User *user = GetUserBySessionId(packet.sessionId);
        if (!user)
        {
            SendError(packet, "Not logged in");
            return;
        }

        // 从UserContext获取roomId
        uint64_t roomId = GetUserRoomId(user);
        if (roomId == 0)
        {
            SendError(packet, "You are not in a room");
            return;
        }

        Room *room = objMgr.GetRoom(roomId);
        if (!room)
        {
            SendError(packet, "Room not found");
            return;
        }

        // 业务逻辑：开始游戏
        if (!room->StartGame(user->GetID()))
        {
            SendError(packet, "Failed to start game: " + room->GetError());
            return;
        }

        MapType response;
        response["success"] = true;
        SendResponse(packet, MsgType::GameStarted, response);
        return;
    }
    case MsgType::GameEnded:
    {
        // GameEnded 是服务器推送消息，客户端不应主动发送
        SendError(packet, "GameEnded is a server push message, not a request");
        return;
    }
    case MsgType::MakeMove:
    {
        User *user = GetUserBySessionId(packet.sessionId);
        if (!user)
        {
            LOG_WARN("Make move failed: User not logged in (sessionId: " + std::to_string(packet.sessionId) + ")");
            SendError(packet, "Not logged in");
            return;
        }

        // 从UserContext获取roomId
        uint64_t roomId = GetUserRoomId(user);
        if (roomId == 0)
        {
            LOG_WARN("Make move failed: User not in a room (userId: " + std::to_string(user->GetID()) + ")");
            SendError(packet, "You are not in a room");
            return;
        }

        uint32_t x = packet.GetParam<uint32_t>("x");
        uint32_t y = packet.GetParam<uint32_t>("y");

        LOG_DEBUG("Make move request: userId=" + std::to_string(user->GetID()) +
                  ", roomId=" + std::to_string(roomId) +
                  ", position=(" + std::to_string(x) + "," + std::to_string(y) + ")");

        Room *room = objMgr.GetRoom(roomId);
        if (!room)
        {
            LOG_WARN("Make move failed: Room not found (roomId: " + std::to_string(roomId) + ")");
            SendError(packet, "Room not found");
            return;
        }

        // 业务逻辑：落子验证
        if (!room->MakeMove(user->GetID(), x, y))
        {
            LOG_WARN("Illegal move: userId=" + std::to_string(user->GetID()) +
                     ", roomId=" + std::to_string(roomId) +
                     ", position=(" + std::to_string(x) + "," + std::to_string(y) +
                     "), error=" + room->GetError());
            SendError(packet, "Illegal move: " + room->GetError());
            return;
        }

        LOG_INFO("Move made successfully: userId=" + std::to_string(user->GetID()) +
                 ", roomId=" + std::to_string(roomId) +
                 ", position=(" + std::to_string(x) + "," + std::to_string(y) + ")");

        // 发送落子响应
        MapType response;
        response["x"] = x;
        response["y"] = y;
        response["success"] = true;
        SendResponse(packet, MsgType::MakeMove, response);
        return;
    }
    case MsgType::GiveUp:
    {
        User *user = GetUserBySessionId(packet.sessionId);
        if (!user)
        {
            SendError(packet, "Not logged in");
            return;
        }

        // 从UserContext获取roomId
        uint64_t roomId = GetUserRoomId(user);
        if (roomId == 0)
        {
            SendError(packet, "You are not in a room");
            return;
        }

        Room *room = objMgr.GetRoom(roomId);
        if (!room)
        {
            SendError(packet, "Room not found");
            return;
        }

        // 业务逻辑：认输
        if (!room->GiveUp(user->GetID()))
        {
            SendError(packet, "Give up failed");
            return;
        }

        MapType response;
        response["success"] = true;
        SendResponse(packet, MsgType::GiveUp, response);
        return;
    }
    case MsgType::Draw:
    {
        User *user = GetUserBySessionId(packet.sessionId);
        if (!user)
        {
            SendError(packet, "Not logged in");
            return;
        }

        // 从UserContext获取roomId
        uint64_t roomId = GetUserRoomId(user);
        if (roomId == 0)
        {
            SendError(packet, "You are not in a room");
            return;
        }

        Room *room = objMgr.GetRoom(roomId);
        if (!room)
        {
            SendError(packet, "Room not found");
            return;
        }

        // 获取协商状态参数
        NegStatus negStatus = static_cast<NegStatus>(packet.GetParam<uint8_t>("negStatus", static_cast<uint8_t>(NegStatus::Ask)));

        // 根据协商状态处理平局请求
        bool success = false;
        if (negStatus == NegStatus::Ask)
        {
            // 请求平局
            success = room->Draw(user->GetID());
            if (!success)
            {
                SendError(packet, "Draw request failed");
                return;
            }
        }
        else if (negStatus == NegStatus::Accept || negStatus == NegStatus::Reject)
        {
            // 接受或拒绝平局请求（目前简单处理为成功）
            // TODO: 实现平局协商逻辑
            success = true;
            // 发布平局接受/拒绝事件
            if (negStatus == NegStatus::Accept)
                EventBus<Event>::GetInstance().Publish(Event::DrawAccepted, roomId, user->GetID());
        }
        else
        {
            SendError(packet, "Invalid negStatus");
            return;
        }

        MapType response;
        response["success"] = success;
        response["negStatus"] = static_cast<uint8_t>(negStatus);
        SendResponse(packet, MsgType::Draw, response);
        return;
    }
    case MsgType::UndoMove:
    {
        User *user = GetUserBySessionId(packet.sessionId);
        if (!user)
        {
            SendError(packet, "Not logged in");
            return;
        }

        // 从UserContext获取roomId
        uint64_t roomId = GetUserRoomId(user);
        if (roomId == 0)
        {
            SendError(packet, "You are not in a room");
            return;
        }

        Room *room = objMgr.GetRoom(roomId);
        if (!room)
        {
            SendError(packet, "Room not found");
            return;
        }

        // 获取协商状态参数
        NegStatus negStatus = static_cast<NegStatus>(packet.GetParam<uint8_t>("negStatus", static_cast<uint8_t>(NegStatus::Ask)));

        // 根据协商状态处理悔棋请求
        bool success = false;
        if (negStatus == NegStatus::Ask)
        {
            // 请求悔棋（需要目标位置，但协议未定义，暂时使用默认值）
            // 注意：Room::BackMove 需要 x, y 但未实现，暂时返回成功
            success = room->BackMove(user->GetID(), 0, 0); // 总是失败
            if (!success)
            {
                // 由于功能未实现，返回错误或模拟成功
                // 暂时返回成功以保持兼容性
                success = true;
            }
        }
        else if (negStatus == NegStatus::Accept || negStatus == NegStatus::Reject)
        {
            // 接受或拒绝悔棋请求
            success = true;
            // TODO: 实现悔棋协商逻辑
        }
        else
        {
            SendError(packet, "Invalid negStatus");
            return;
        }

        MapType response;
        response["success"] = success;
        response["negStatus"] = static_cast<uint8_t>(negStatus);
        SendResponse(packet, MsgType::UndoMove, response);
        return;
    }
    case MsgType::SyncGame:
    {
        User *user = GetUserBySessionId(packet.sessionId);
        if (!user)
        {
            SendError(packet, "Not logged in");
            return;
        }

        // 从UserContext获取roomId
        uint64_t roomId = GetUserRoomId(user);
        if (roomId == 0)
        {
            SendError(packet, "You are not in a room");
            return;
        }

        Room *room = objMgr.GetRoom(roomId);
        if (!room)
        {
            SendError(packet, "Room not found");
            return;
        }

        // 构造游戏状态字符串（简化版）
        std::string statusStr;
        if (room->status == RoomStatus::Playing)
        {
            statusStr = "playing";
            // 可以添加更多信息，如棋盘大小、当前回合等
        }
        else if (room->status == RoomStatus::Free)
        {
            statusStr = "free";
        }
        else if (room->status == RoomStatus::End)
        {
            statusStr = "ended";
        }
        else
        {
            statusStr = "unknown";
        }

        MapType response;
        response["success"] = true;
        response["statusStr"] = statusStr;
        SendResponse(packet, MsgType::SyncGame, response);
        return;
    }
    default:
        LOG_DEBUG("Unhandled game MsgType: " + std::to_string(static_cast<uint32_t>(packet.msgType)));
        SendError(packet, "Unhandled game message type");
        break;
    }
}

void Handler::HandlePacket(const Packet &packet)
{
    if (packet.msgType == MsgType::None)
    {
        LOG_TRACE("HeartBeat");
        return;
    }

    LOG_DEBUG("Handling packet with MsgType: " + std::to_string(static_cast<uint32_t>(packet.msgType)));

    uint32_t msgValue = static_cast<uint32_t>(packet.msgType);

    // 根据MsgType范围路由到对应的分组处理方法
    if (msgValue >= 100 && msgValue < 200)
    {
        HandleAuthPacket(packet);
    }
    else if (msgValue >= 200 && msgValue < 300)
    {
        HandleLobbyPacket(packet);
    }
    else if (msgValue >= 300 && msgValue < 400)
    {
        HandleRoomPacket(packet);
    }
    else if (msgValue >= 400 && msgValue < 500)
    {
        HandleGamePacket(packet);
    }
    else
    {
        LOG_DEBUG("Unhandled MsgType range: " + std::to_string(msgValue));
        SendError(packet, "Unhandled message type range");
    }
}

// --- 辅助函数 ---

void Handler::SendResponse(const Packet &request, MsgType responseType, const MapType &params)
{
    // 创建响应包（事件驱动架构，不需要requestId）
    Packet response(request.sessionId, responseType);
    response.params = params;
    LOG_DEBUG("Sending response: msgType=" + std::to_string((int)responseType));
    if (sendCallback)
    {
        sendCallback(response);
    }
    else
    {
        LOG_ERROR("sendCallback is null, cannot send response");
    }
}

void Handler::SendError(const Packet &request, const std::string &errMsg)
{
    // 错误响应（事件驱动架构，不需要requestId）
    Packet errPacket(request.sessionId, MsgType::Error);
    errPacket.AddParam("error", errMsg);
    LOG_WARN("[Handler] Sending error: " + errMsg);
    if (sendCallback)
    {
        sendCallback(errPacket);
    }
    else
    {
        LOG_ERROR("[Handler] sendCallback is null, cannot send error");
    }
}

// --- 房间状态发送辅助函数 ---

void Handler::SendBoardState(uint64_t sessionId, Room *room)
{
    if (!room)
        return;

    // 创建棋盘状态推送包 - 使用 SyncGame
    Packet boardStatePush(sessionId, MsgType::SyncGame);
    boardStatePush.AddParam("roomId", room->GetRoomId());
    boardStatePush.AddParam("boardSize", (uint32_t)15); // TODO: 从room获取实际棋盘大小

    // TODO: 添加棋盘数据序列化
    // 目前先发送空棋盘状态

    LOG_DEBUG("Sending board state to session " + std::to_string(sessionId) +
              " for room " + std::to_string(room->GetRoomId()));

    if (sendCallback)
    {
        sendCallback(boardStatePush);
    }
}

void Handler::SendPlayerList(uint64_t sessionId, Room *room)
{
    if (!room)
        return;

    // 创建玩家列表推送包 - 使用 SyncUsersToRoom
    Packet playerListPush(sessionId, MsgType::SyncUsersToRoom);
    playerListPush.AddParam("roomId", room->GetRoomId());
    playerListPush.AddParam("playerCount", (uint32_t)room->playerIds.size());

    // 添加玩家ID列表
    // TODO: 需要将玩家ID列表序列化为数组
    // 目前先发送简单的玩家数量信息

    LOG_DEBUG("Sending player list to session " + std::to_string(sessionId) +
              " for room " + std::to_string(room->GetRoomId()) +
              " with " + std::to_string(room->playerIds.size()) + " players");

    if (sendCallback)
    {
        sendCallback(playerListPush);
    }
}

// --- UserContext 相关辅助函数 ---

User *Handler::GetUserBySessionId(uint64_t sessionId)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(sessionId);
    if (userId == 0)
        return nullptr;
    return objMgr.GetUserByUserId(userId);
}

uint64_t Handler::GetUserRoomId(User *user)
{
    if (!user)
        return 0;

    // 使用ObjectManager接口查询用户所在的房间ID
    return objMgr.GetRoomIdByUserId(user->GetID());
}
