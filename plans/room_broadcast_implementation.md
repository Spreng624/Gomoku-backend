# 房间创建和广播逻辑实现计划

## 需求分析

用户 CreateRoom 后，Room 直接广播 Event，Notifier 直接广播给所有房间的用户。
房间被创建时广播多个事件：

1. 棋盘
2. 黑白棋手
3. PlayerList
4. 房主创建房间的消息

玩家进入后，给玩家发送：

1. 棋盘
2. 黑白棋手
3. PlayerList
   再发送广播：玩家加入房间

## 当前架构分析

### 现有组件

1. **Handler.cpp** - 处理客户端请求

    - `OnCreateRoom()`: 创建房间，返回 roomId
    - `OnJoinRoom()`: 加入房间，发布 PlayerJoined 事件

2. **Notifier.cpp** - 处理事件广播

    - 已订阅多个游戏事件
    - 有`BroadcastToRoom()`和`SendToSession()`方法

3. **Room.cpp** - 房间逻辑

    - 管理玩家列表、棋盘状态等

4. **ObjectManager.cpp** - 对象管理
    - 创建和管理房间、用户、玩家

### 需要修改的文件

## 修改方案

### 1. 修改 Packet.h - 添加新的消息类型

在 MsgType 枚举中添加：

-   `RoomCreated = 0x1008` - 房间创建推送
-   `BoardState = 0x1009` - 棋盘状态推送
-   `PlayerList = 0x100A` - 玩家列表推送
-   `ColorAssignment = 0x100B` - 黑白棋手分配推送

### 2. 修改 Handler.cpp - OnCreateRoom 方法

在房间创建成功后，需要：

1. 将房主添加到房间（调用`room->AddPlayer(userId)`）
2. 发布房间创建事件到 EventBus
3. 触发房间初始状态广播

修改后的`OnCreateRoom`逻辑：

```cpp
void Handler::OnCreateRoom(const Packet &packet)
{
    // ... 现有验证逻辑

    // 创建房间
    Room *room = objMgr.CreateRoom(userId);
    if (!room)
    {
        // ... 错误处理
    }

    // 将房主添加到房间
    if (!room->AddPlayer(userId))
    {
        // ... 错误处理
    }

    // 发送创建响应
    MapType response;
    response["roomId"] = uint64_t(room->GetRoomId());
    SendResponse(packet, MsgType::CreateRoom, response);

    // 发布房间创建事件，触发广播
    eventBus.Publish(Event::RoomCreated, room->GetRoomId(), userId);
}
```

### 3. 修改 Handler.cpp - OnJoinRoom 方法

在玩家加入房间后，需要：

1. 给新加入的玩家发送房间当前状态
2. 广播玩家加入事件给其他玩家

修改后的`OnJoinRoom`逻辑：

```cpp
void Handler::OnJoinRoom(const Packet &packet)
{
    // ... 现有验证和加入逻辑

    // 发送加入房间响应
    MapType response;
    response["roomId"] = roomId;
    response["success"] = true;
    SendResponse(packet, MsgType::JoinRoom, response);

    // 给新玩家发送房间当前状态
    SendRoomStateToPlayer(packet.sessionId, room);

    // 发布玩家加入事件（广播给其他玩家）
    eventBus.Publish(Event::PlayerJoined, roomId, userId);
}
```

### 4. 添加辅助函数 - SendRoomStateToPlayer

需要在 Handler 中添加辅助函数，用于向单个玩家发送房间状态：

```cpp
void Handler::SendRoomStateToPlayer(uint64_t sessionId, Room *room)
{
    if (!room) return;

    // 1. 发送棋盘状态
    SendBoardState(sessionId, room);

    // 2. 发送玩家列表
    SendPlayerList(sessionId, room);

    // 3. 发送黑白棋手分配
    SendColorAssignment(sessionId, room);
}
```

### 5. 修改 Notifier.cpp - 添加房间创建事件处理

在 Notifier 构造函数中添加对`RoomCreated`事件的订阅：

```cpp
// 在构造函数中添加
auto token9 = eventBus.Subscribe<uint64_t, uint64_t>(Event::RoomCreated,
    std::function<void(uint64_t, uint64_t)>(
        [this](uint64_t roomId, uint64_t ownerId)
        { OnRoomCreated(roomId, ownerId); }));
tokens.push_back(token9);
```

添加`OnRoomCreated`处理函数：

```cpp
void Notifier::OnRoomCreated(uint64_t roomId, uint64_t ownerId)
{
    Room *room = objMgr.GetRoom(roomId);
    if (!room) return;

    // 1. 广播房间创建消息
    Packet roomCreatedPush(0, MsgType::RoomCreated);
    roomCreatedPush.AddParam("roomId", roomId);
    roomCreatedPush.AddParam("ownerId", ownerId);
    roomCreatedPush.AddParam("boardSize", (uint32_t)room->boardSize);
    BroadcastToRoom(roomId, roomCreatedPush);

    // 2. 广播棋盘状态
    SendBoardStateToRoom(room);

    // 3. 广播玩家列表
    SendPlayerListToRoom(room);

    // 4. 广播黑白棋手分配
    SendColorAssignmentToRoom(room);
}
```

### 6. 添加 Notifier 辅助函数

在 Notifier 中添加以下辅助函数：

```cpp
void Notifier::SendBoardStateToRoom(Room *room)
{
    Packet boardStatePush(0, MsgType::BoardState);
    boardStatePush.AddParam("roomId", room->GetRoomId());
    // TODO: 添加棋盘数据
    BroadcastToRoom(room->GetRoomId(), boardStatePush);
}

void Notifier::SendPlayerListToRoom(Room *room)
{
    Packet playerListPush(0, MsgType::PlayerList);
    playerListPush.AddParam("roomId", room->GetRoomId());
    // TODO: 添加玩家列表数据
    BroadcastToRoom(room->GetRoomId(), playerListPush);
}

void Notifier::SendColorAssignmentToRoom(Room *room)
{
    Packet colorPush(0, MsgType::ColorAssignment);
    colorPush.AddParam("roomId", room->GetRoomId());
    colorPush.AddParam("blackPlayerId", room->blackPlayerId);
    colorPush.AddParam("whitePlayerId", room->whitePlayerId);
    BroadcastToRoom(room->GetRoomId(), colorPush);
}
```

### 7. 修改 EventBus.hpp - 添加 RoomCreated 事件

在 Event 枚举中添加：

```cpp
RoomCreated,      // 房间创建完成
```

## 实施步骤

1. 切换到 Code 模式
2. 修改 Packet.h 添加新的 MsgType
3. 修改 EventBus.hpp 添加 RoomCreated 事件
4. 修改 Handler.cpp 的 OnCreateRoom 方法
5. 修改 Handler.cpp 的 OnJoinRoom 方法
6. 在 Handler 中添加 SendRoomStateToPlayer 辅助函数
7. 修改 Notifier.cpp 添加 RoomCreated 事件处理
8. 在 Notifier 中添加广播辅助函数
9. 编译测试

## 注意事项

1. 需要确保房间创建时房主已经被添加到房间
2. 广播消息的 sessionId 应该为 0（服务器推送）
3. 需要处理房间可能为空的情况
4. 棋盘数据的序列化需要实现
5. 玩家列表数据的格式需要定义
