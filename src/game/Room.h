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
    Free = 0,
    Playing = 1,
    End = 2
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

    uint64_t GetRoomId() const;
    bool AddPlayer(uint64_t userId);
    int RemovePlayer(uint64_t userId);

    bool EditRoomSetting(uint64_t userId, const MapType &settings);
    bool StartGame(uint64_t userId);
    bool TakeBlack(uint64_t userId);
    bool TakeWhite(uint64_t userId);
    bool CancelTake(uint64_t userId);

    bool TakeMove(uint64_t userId, uint32_t x, uint32_t y);
    bool BackMove(uint64_t userId, uint32_t x, uint32_t y);
    bool Draw(uint64_t userId);
    bool GiveUp(uint64_t userId);

    bool IsUserInRoom(uint64_t userId) const;

    std::string GetError() const;
};

#endif