#ifndef PLAYER_H
#define PLAYER_H

#include "EventBus.hpp"
#include "User.h"
#include "Packet.h"
#include <cstdint>

enum PlayerStatus
{
    Offline, // 掉线
    Free,    // 大厅
    Playing, // 正在游戏
    InRoom   // 房间内
};

class Player
{
private:
    EventBus<Event> &eventBus;
    std::vector<std::shared_ptr<void>> tokens;

    uint64_t lastPushUserListTime;
    uint64_t lastPushRoomListTime;

    int SendErrorMsg(std::string error);

public:
    uint64_t userId;
    uint64_t playerId;
    uint64_t sessionId;
    PlayerStatus status;
    int roomId;
    bool isGuest;

    Player(EventBus<Event> &eventBus);

    int Init(User &user, std::string sessionId, PlayerStatus status);

    // 推送相关
    int PushUserList();
    int PushRoomList();
};

#endif
