#ifndef GAMEROOM_H
#define GAMEROOM_H

#include "Game.h"
#include "Packet.h"
#include "EventBus.hpp"
#include <vector>
#include <cstdint>
#include <string>

enum class RoomStatus
{
    Free,
    Playing,
    End
};

class Room
{
private:
    Game game;
    // 配置项
    int boardSize;
    bool isGraded;
    bool enableTakeback;
    int baseTimeSeconds;
    int byoyomiSeconds;
    int byoyomiCount;

    std::string error;

    // 辅助函数
    bool isInRoom(uint64_t userId) const;

public:
    // 基础信息
    uint64_t roomId;
    RoomStatus status;
    uint64_t ownerId;
    uint64_t blackPlayerId;
    uint64_t whitePlayerId;
    std::vector<uint64_t> playerIds;

    Room(uint64_t roomId);
    ~Room();

    bool SyncSeat(uint64_t userId, uint64_t blackUserId, uint64_t whiteUserId);
    uint64_t GetRoomId() const;
    bool AddPlayer(uint64_t userId);
    int RemovePlayer(uint64_t userId);

    bool EditRoomSetting(uint64_t userId, const MapType &settings);
    bool StartGame(uint64_t userId);
    bool TakeBlack(uint64_t userId);
    bool TakeWhite(uint64_t userId);
    bool CancelTake(uint64_t userId);

    bool MakeMove(uint64_t userId, uint32_t x, uint32_t y);
    bool BackMove(uint64_t userId, uint32_t x, uint32_t y);
    bool Draw(uint64_t userId);
    bool GiveUp(uint64_t userId);

    bool IsUserInRoom(uint64_t userId) const;

    std::string GetError() const;
};

#endif