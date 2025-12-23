#include "Room.h"
#include "EventBus.hpp"
#include <algorithm>

Room::Room(uint64_t roomId) : roomId(roomId), status(RoomStatus::Free),
                              ownerId(0), blackPlayerId(0), whitePlayerId(0),
                              boardSize(15), isGraded(false), enableTakeback(true),
                              baseTimeSeconds(600), byoyomiSeconds(30), byoyomiCount(5)
{
    game = Game(boardSize);
}

Room::~Room() {}

uint64_t Room::GetRoomId() const
{
    return roomId;
}

bool Room::AddPlayer(uint64_t userId)
{
    if (std::find(playerIds.begin(), playerIds.end(), userId) != playerIds.end())
    {
        error = "Player already in room";
        return false;
    }

    if (playerIds.size() >= 2)
    {
        error = "Room is full";
        return false;
    }

    if (playerIds.empty())
    {
        ownerId = userId;
    }

    playerIds.push_back(userId);
    return true;
}

int Room::RemovePlayer(uint64_t userId)
{
    auto it = std::find(playerIds.begin(), playerIds.end(), userId);
    if (it == playerIds.end())
    {
        error = "Player not in room";
        return -1;
    }

    playerIds.erase(it);

    if (userId == ownerId)
    {
        if (!playerIds.empty())
        {
            ownerId = playerIds[0];
        }
        else
        {
            ownerId = 0;
        }
    }

    if (userId == blackPlayerId)
        blackPlayerId = 0;
    if (userId == whitePlayerId)
        whitePlayerId = 0;

    return 0;
}

bool Room::EditRoomSetting(uint64_t userId, const MapType &settings)
{
    if (userId != ownerId)
    {
        error = "Only room owner can edit settings";
        return false;
    }

    if (status == RoomStatus::Playing)
    {
        error = "Cannot edit settings while playing";
        return false;
    }

    auto boardSizeIt = settings.find("boardSize");
    if (boardSizeIt != settings.end())
    {
        if (const uint32_t *val = std::get_if<uint32_t>(&boardSizeIt->second))
        {
            boardSize = *val;
            game = Game(boardSize);
        }
    }

    return true;
}

bool Room::StartGame(uint64_t userId)
{
    if (userId != ownerId)
    {
        error = "Only room owner can start game";
        return false;
    }

    if (status == RoomStatus::Playing)
    {
        error = "Game already started";
        return false;
    }

    if (playerIds.size() < 2)
    {
        error = "Need at least 2 players";
        return false;
    }

    if (blackPlayerId == 0 || whitePlayerId == 0)
    {
        error = "Both players must choose a color";
        return false;
    }

    status = RoomStatus::Playing;
    game.reset();
    return true;
}

bool Room::TakeBlack(uint64_t userId)
{
    if (status == RoomStatus::Playing)
    {
        error = "Game already started";
        return false;
    }

    if (std::find(playerIds.begin(), playerIds.end(), userId) == playerIds.end())
    {
        error = "Player not in room";
        return false;
    }

    if (blackPlayerId != 0 && blackPlayerId != userId)
    {
        error = "Black already taken";
        return false;
    }

    blackPlayerId = userId;
    return true;
}

bool Room::TakeWhite(uint64_t userId)
{
    if (status == RoomStatus::Playing)
    {
        error = "Game already started";
        return false;
    }

    if (std::find(playerIds.begin(), playerIds.end(), userId) == playerIds.end())
    {
        error = "Player not in room";
        return false;
    }

    if (whitePlayerId != 0 && whitePlayerId != userId)
    {
        error = "White already taken";
        return false;
    }

    whitePlayerId = userId;
    return true;
}

bool Room::CancelTake(uint64_t userId)
{
    if (userId == blackPlayerId)
    {
        blackPlayerId = 0;
        return true;
    }
    if (userId == whitePlayerId)
    {
        whitePlayerId = 0;
        return true;
    }

    error = "Player did not take any color";
    return false;
}

bool Room::TakeMove(uint64_t userId, uint32_t x, uint32_t y)
{
    if (status != RoomStatus::Playing)
    {
        error = "Game not in progress";
        return false;
    }

    Piece piece;
    if (userId == blackPlayerId)
    {
        piece = Piece::BLACK;
    }
    else if (userId == whitePlayerId)
    {
        piece = Piece::WHITE;
    }
    else
    {
        error = "Player is not in this game";
        return false;
    }

    if (!game.makeMove(x, y, piece))
    {
        error = "Illegal move";
        return false;
    }

    Piece winner = game.checkWin(x, y);
    if (winner != Piece::EMPTY)
    {
        status = RoomStatus::End;
        // TODO: 通过 EventBus 发送游戏结束事件
    }

    return true;
}

bool Room::BackMove(uint64_t userId, uint32_t x, uint32_t y)
{
    if (!enableTakeback)
    {
        error = "Takeback disabled";
        return false;
    }

    error = "Not implemented";
    return false;
}

bool Room::Draw(uint64_t userId)
{
    if (status != RoomStatus::Playing)
    {
        error = "Game not in progress";
        return false;
    }

    if (userId != blackPlayerId && userId != whitePlayerId)
    {
        error = "Player is not in this game";
        return false;
    }

    status = RoomStatus::End;
    // TODO: 发送平局事件
    return true;
}

bool Room::GiveUp(uint64_t userId)
{
    if (status != RoomStatus::Playing)
    {
        error = "Game not in progress";
        return false;
    }

    if (userId != blackPlayerId && userId != whitePlayerId)
    {
        error = "Player is not in this game";
        return false;
    }

    status = RoomStatus::End;
    // TODO: 发送认输事件，对方获胜
    return true;
}

std::string Room::GetError() const
{
    return error;
}