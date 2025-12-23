#include "Handler.h"
#include "ObjectManager.h"
#include "Logger.h"

Handler::Handler(ObjectManager &objMgr, EventBus<Event> &eventBus, std::function<void(const Packet &)> sendCallback)
    : objMgr(objMgr), eventBus(eventBus), sendCallback(sendCallback)
{
    // TODO: 订阅游戏事件
}

Handler::~Handler() {}

void Handler::HandlePacket(const Packet &packet)
{
    if (packet.msgType == MsgType::None)
    {
        LOG_TRACE("HeartBeat");
        return;
    }

    LOG_DEBUG("Handling packet with MsgType: " + std::to_string(static_cast<uint32_t>(packet.msgType)));
    switch (packet.msgType)
    {
    case MsgType::None:
        break;
    case MsgType::Login:
        OnLogin(packet);
        break;
    case MsgType::SignIn:
        OnSignIn(packet);
        break;
    case MsgType::LoginByGuest:
        OnLoginByGuest(packet);
        break;
    case MsgType::Guest2User:
        OnGuest2User(packet);
        break;
    case MsgType::EditUsername:
        OnEditUsername(packet);
        break;
    case MsgType::EditPassword:
        OnEditPassword(packet);
        break;
    case MsgType::LogOut:
        OnLogOut(packet);
        break;
    case MsgType::CreateRoom:
        OnCreateRoom(packet);
        break;
    case MsgType::CreateSingleRoom:
        OnCreateSingleRoom(packet);
        break;
    case MsgType::JoinRoom:
        OnJoinRoom(packet);
        break;
    case MsgType::ExitRoom:
        OnExitRoom(packet);
        break;
    case MsgType::QuickMatch:
        OnQuickMatch(packet);
        break;
    case MsgType::TakeBlack:
        OnTakeBlack(packet);
        break;
    case MsgType::TakeWhite:
        OnTakeWhite(packet);
        break;
    case MsgType::CancelTake:
        OnCancelTake(packet);
        break;
    case MsgType::StartGame:
        OnStartGame(packet);
        break;
    case MsgType::EditRoomSetting:
        OnEditRoomSetting(packet);
        break;
    case MsgType::MakeMove:
        OnMakeMove(packet);
        break;
    case MsgType::UndoMoveRequest:
        OnBackMove(packet);
        break;
    case MsgType::DrawRequest:
        OnDraw(packet);
        break;
    case MsgType::GiveUp:
        OnGiveUp(packet);
        break;
    case MsgType::GetUser:
        OnGetUser(packet);
        break;
    case MsgType::SubscribeUserList:
        OnSubscribeUserList(packet);
        break;
    case MsgType::SubscribeRoomList:
        OnSubscribeRoomList(packet);
        break;
    default:
        LOG_DEBUG("Unhandled MsgType: " + std::to_string(static_cast<uint32_t>(packet.msgType)));
        break;
    }
}

// --- 用户相关业务逻辑 ---

void Handler::OnLogin(const Packet &packet)
{
    std::string username = packet.GetParam<std::string>("username");
    std::string password = packet.GetParam<std::string>("password");

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
}

void Handler::OnSignIn(const Packet &packet)
{
    std::string username = packet.GetParam<std::string>("username");
    std::string password = packet.GetParam<std::string>("password");

    // 业务逻辑：注册新用户
    User *user = objMgr.CreateUser(username, password);
    if (!user)
    {
        SendError(packet, "Username already exists");
        return;
    }

    objMgr.MapSessionToUser(packet.sessionId, user->GetID());

    MapType response;
    response["userId"] = uint64_t(user->GetID());
    SendResponse(packet, MsgType::SignIn, response);
}

void Handler::OnLoginByGuest(const Packet &packet)
{
    // 业务逻辑：创建游客账户（简化处理，直接生成 ID）
    uint64_t guestId = 1000000 + packet.sessionId; // 简单示例

    objMgr.MapSessionToUser(packet.sessionId, guestId);

    MapType response;
    response["guestId"] = guestId;
    SendResponse(packet, MsgType::LoginByGuest, response);
}

void Handler::OnLogOut(const Packet &packet)
{
    // 业务逻辑：注销用户
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    objMgr.UnmapSession(packet.sessionId);

    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::LogOut, response);
}

// --- 房间相关业务逻辑 ---

void Handler::OnCreateRoom(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        LOG_WARN("Create room failed: User not logged in (sessionId: " + std::to_string(packet.sessionId) + ")");
        SendError(packet, "Not logged in");
        return;
    }

    LOG_INFO("Creating room for user ID: " + std::to_string(userId));

    // 业务逻辑：创建房间
    Room *room = objMgr.CreateRoom(userId);
    if (!room)
    {
        LOG_ERROR("Failed to create room for user ID: " + std::to_string(userId));
        SendError(packet, "Failed to create room");
        return;
    }

    LOG_INFO("Room created successfully: roomId=" + std::to_string(room->GetRoomId()) +
             ", ownerId=" + std::to_string(userId));

    MapType response;
    response["roomId"] = uint64_t(room->GetRoomId());
    SendResponse(packet, MsgType::CreateRoom, response);
}

void Handler::OnJoinRoom(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    uint64_t roomId = packet.GetParam<uint64_t>("roomId");
    Room *room = objMgr.GetRoom(roomId);
    if (!room)
    {
        SendError(packet, "Room not found");
        return;
    }

    // 业务逻辑：加入房间
    if (!room->AddPlayer(userId))
    {
        SendError(packet, "Failed to join room: " + room->GetError());
        return;
    }

    // 发送加入房间响应
    MapType response;
    response["roomId"] = roomId;
    response["success"] = true;
    SendResponse(packet, MsgType::JoinRoom, response);

    // 发布玩家加入事件（事件驱动架构）
    eventBus.Publish(Event::PlayerJoined, roomId, userId);
}

void Handler::OnExitRoom(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    uint64_t roomId = packet.GetParam<uint64_t>("roomId");
    Room *room = objMgr.GetRoom(roomId);
    if (!room)
    {
        SendError(packet, "Room not found");
        return;
    }

    // 业务逻辑：退出房间
    if (room->RemovePlayer(userId) != 0)
    {
        SendError(packet, "Failed to exit room");
        return;
    }

    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::ExitRoom, response);

    // 发布玩家离开事件（事件驱动架构）
    eventBus.Publish(Event::PlayerLeft, roomId, userId);
}

// --- 游戏相关业务逻辑 ---

void Handler::OnMakeMove(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        LOG_WARN("Make move failed: User not logged in (sessionId: " + std::to_string(packet.sessionId) + ")");
        SendError(packet, "Not logged in");
        return;
    }

    uint64_t roomId = packet.GetParam<uint64_t>("roomId");
    uint32_t x = packet.GetParam<uint32_t>("x");
    uint32_t y = packet.GetParam<uint32_t>("y");

    LOG_DEBUG("Make move request: userId=" + std::to_string(userId) +
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
    if (!room->TakeMove(userId, x, y))
    {
        LOG_WARN("Illegal move: userId=" + std::to_string(userId) +
                 ", roomId=" + std::to_string(roomId) +
                 ", position=(" + std::to_string(x) + "," + std::to_string(y) +
                 "), error=" + room->GetError());
        SendError(packet, "Illegal move: " + room->GetError());
        return;
    }

    LOG_INFO("Move made successfully: userId=" + std::to_string(userId) +
             ", roomId=" + std::to_string(roomId) +
             ", position=(" + std::to_string(x) + "," + std::to_string(y) + ")");

    // 发送落子响应
    MapType response;
    response["x"] = x;
    response["y"] = y;
    response["success"] = true;
    SendResponse(packet, MsgType::MakeMove, response);

    // 发布对手落子事件（事件驱动架构）
    // 注意：这里发布给房间内所有其他玩家，Notifier会处理广播逻辑
    eventBus.Publish(Event::OpponentMoved, roomId, userId, x, y);
}

void Handler::OnDraw(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    uint64_t roomId = packet.GetParam<uint64_t>("roomId");
    Room *room = objMgr.GetRoom(roomId);
    if (!room)
    {
        SendError(packet, "Room not found");
        return;
    }

    // 业务逻辑：平局请求
    if (!room->Draw(userId))
    {
        SendError(packet, "Draw request failed");
        return;
    }

    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::DrawRequest, response);
}

void Handler::OnGiveUp(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    uint64_t roomId = packet.GetParam<uint64_t>("roomId");
    Room *room = objMgr.GetRoom(roomId);
    if (!room)
    {
        SendError(packet, "Room not found");
        return;
    }

    // 业务逻辑：认输
    if (!room->GiveUp(userId))
    {
        SendError(packet, "Give up failed");
        return;
    }

    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::GiveUp, response);
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

// --- 新增的用户相关处理函数 ---

void Handler::OnGuest2User(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    std::string username = packet.GetParam<std::string>("username");
    std::string password = packet.GetParam<std::string>("password");

    // 业务逻辑：将游客账户转为正式用户
    // 这里需要实现具体的转换逻辑，目前先返回成功
    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::Guest2User, response);
}

void Handler::OnEditUsername(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    std::string newUsername = packet.GetParam<std::string>("newUsername");
    User *user = objMgr.GetUserByUserId(userId);
    if (!user)
    {
        SendError(packet, "User not found");
        return;
    }

    // 业务逻辑：修改用户名
    if (user->EditUsername(newUsername) != 0)
    {
        SendError(packet, "Failed to edit username");
        return;
    }

    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::EditUsername, response);
}

void Handler::OnEditPassword(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    std::string newPassword = packet.GetParam<std::string>("newPassword");
    User *user = objMgr.GetUserByUserId(userId);
    if (!user)
    {
        SendError(packet, "User not found");
        return;
    }

    // 业务逻辑：修改密码
    if (user->EditPassword(newPassword) != 0)
    {
        SendError(packet, "Failed to edit password");
        return;
    }

    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::EditPassword, response);
}

// --- 新增的房间相关处理函数 ---

void Handler::OnCreateSingleRoom(const Packet &packet)
{
    // 创建单人房间（与AI对战）
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    Room *room = objMgr.CreateRoom(userId);
    if (!room)
    {
        SendError(packet, "Failed to create room");
        return;
    }

    // TODO: 添加AI玩家到房间
    MapType response;
    response["roomId"] = uint64_t(room->GetRoomId());
    SendResponse(packet, MsgType::CreateSingleRoom, response);
}

void Handler::OnQuickMatch(const Packet &packet)
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
}

// --- 新增的座位操作处理函数 ---

void Handler::OnTakeBlack(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    uint64_t roomId = packet.GetParam<uint64_t>("roomId");
    Room *room = objMgr.GetRoom(roomId);
    if (!room)
    {
        SendError(packet, "Room not found");
        return;
    }

    if (!room->TakeBlack(userId))
    {
        SendError(packet, "Failed to take black: " + room->GetError());
        return;
    }

    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::TakeBlack, response);
}

void Handler::OnTakeWhite(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    uint64_t roomId = packet.GetParam<uint64_t>("roomId");
    Room *room = objMgr.GetRoom(roomId);
    if (!room)
    {
        SendError(packet, "Room not found");
        return;
    }

    if (!room->TakeWhite(userId))
    {
        SendError(packet, "Failed to take white: " + room->GetError());
        return;
    }

    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::TakeWhite, response);
}

void Handler::OnCancelTake(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    uint64_t roomId = packet.GetParam<uint64_t>("roomId");
    Room *room = objMgr.GetRoom(roomId);
    if (!room)
    {
        SendError(packet, "Room not found");
        return;
    }

    if (!room->CancelTake(userId))
    {
        SendError(packet, "Failed to cancel take: " + room->GetError());
        return;
    }

    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::CancelTake, response);
}

void Handler::OnStartGame(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    uint64_t roomId = packet.GetParam<uint64_t>("roomId");
    Room *room = objMgr.GetRoom(roomId);
    if (!room)
    {
        SendError(packet, "Room not found");
        return;
    }

    if (!room->StartGame(userId))
    {
        SendError(packet, "Failed to start game: " + room->GetError());
        return;
    }

    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::StartGame, response);
}

void Handler::OnEditRoomSetting(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    uint64_t roomId = packet.GetParam<uint64_t>("roomId");
    Room *room = objMgr.GetRoom(roomId);
    if (!room)
    {
        SendError(packet, "Room not found");
        return;
    }

    // 直接使用packet.params而不是GetParams()
    if (!room->EditRoomSetting(userId, packet.params))
    {
        SendError(packet, "Failed to edit room setting: " + room->GetError());
        return;
    }

    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::EditRoomSetting, response);
}

// --- 新增的游戏相关处理函数 ---

void Handler::OnBackMove(const Packet &packet)
{
    uint64_t userId = objMgr.GetUserIdBySessionId(packet.sessionId);
    if (userId == 0)
    {
        SendError(packet, "Not logged in");
        return;
    }

    uint64_t roomId = packet.GetParam<uint64_t>("roomId");
    uint32_t x = packet.GetParam<uint32_t>("x");
    uint32_t y = packet.GetParam<uint32_t>("y");

    Room *room = objMgr.GetRoom(roomId);
    if (!room)
    {
        SendError(packet, "Room not found");
        return;
    }

    if (!room->BackMove(userId, x, y))
    {
        SendError(packet, "Failed to back move: " + room->GetError());
        return;
    }

    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::UndoMoveRequest, response);
}

// --- 新增的查询和订阅处理函数 ---

void Handler::OnGetUser(const Packet &packet)
{
    uint64_t targetUserId = packet.GetParam<uint64_t>("userId");
    User *user = objMgr.GetUserByUserId(targetUserId);
    if (!user)
    {
        SendError(packet, "User not found");
        return;
    }

    MapType response;
    response["userId"] = targetUserId;
    response["username"] = user->GetUsername();
    response["rank"] = user->GetLevel();
    response["score"] = user->GetScore();
    response["ranking"] = user->GetRanking();
    SendResponse(packet, MsgType::GetUser, response);
}

void Handler::OnSubscribeUserList(const Packet &packet)
{
    // 订阅用户列表更新
    // 这里需要实现订阅逻辑，目前先返回成功
    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::SubscribeUserList, response);
}

void Handler::OnSubscribeRoomList(const Packet &packet)
{
    // 订阅房间列表更新
    // 这里需要实现订阅逻辑，目前先返回成功
    MapType response;
    response["success"] = true;
    SendResponse(packet, MsgType::SubscribeRoomList, response);
}
