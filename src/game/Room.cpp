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

    if (playerIds.size() >= 10)
    {
        error = "Room is full";
        return false;
    }

    if (playerIds.empty())
    {
        ownerId = userId;
    }

    playerIds.push_back(userId);

    EventBus<Event>::GetInstance().Publish(Event::PlayerJoined, roomId, userId);
    EventBus<Event>::GetInstance().Publish(Event::RoomListUpdated);
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

    // 发布玩家离开事件
    EventBus<Event>::GetInstance().Publish(Event::PlayerLeft, roomId, userId);

    // 发布房间列表更新事件
    EventBus<Event>::GetInstance().Publish(Event::RoomListUpdated);

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

    // 发布房间状态变化事件（设置修改）
    EventBus<Event>::GetInstance().Publish(Event::RoomStatusChanged, roomId, userId, "settings_updated");

    // 发布房间列表更新事件
    EventBus<Event>::GetInstance().Publish(Event::RoomListUpdated);

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

    // 发布游戏开始事件
    EventBus<Event>::GetInstance().Publish(Event::GameStarted, roomId);

    // 发布房间状态变化事件
    EventBus<Event>::GetInstance().Publish(Event::RoomStatusChanged, roomId, userId, "playing");

    // 发布房间列表更新事件
    EventBus<Event>::GetInstance().Publish(Event::RoomListUpdated);

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

    // 发布房间状态变化事件（颜色选择）
    EventBus<Event>::GetInstance().Publish(Event::RoomStatusChanged, roomId, userId, "black_selected");

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

    // 发布房间状态变化事件（颜色选择）
    EventBus<Event>::GetInstance().Publish(Event::RoomStatusChanged, roomId, userId, "white_selected");

    return true;
}

bool Room::CancelTake(uint64_t userId)
{
    if (userId == blackPlayerId)
    {
        blackPlayerId = 0;
        // 发布房间状态变化事件（取消颜色选择）
        EventBus<Event>::GetInstance().Publish(Event::RoomStatusChanged, roomId, userId, "black_canceled");
        return true;
    }
    if (userId == whitePlayerId)
    {
        whitePlayerId = 0;
        // 发布房间状态变化事件（取消颜色选择）
        EventBus<Event>::GetInstance().Publish(Event::RoomStatusChanged, roomId, userId, "white_canceled");
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

    // 发布棋子放置事件
    EventBus<Event>::GetInstance().Publish(Event::PiecePlaced, roomId, userId, x, y);

    Piece winner = game.checkWin(x, y);
    if (winner != Piece::EMPTY)
    {
        status = RoomStatus::End;

        // 确定胜利者ID
        uint64_t winnerId = 0;
        if (winner == Piece::BLACK)
        {
            winnerId = blackPlayerId;
        }
        else if (winner == Piece::WHITE)
        {
            winnerId = whitePlayerId;
        }

        // 发布游戏结束事件
        EventBus<Event>::GetInstance().Publish(Event::GameEnded, roomId, winnerId);

        // 发布房间状态变化事件
        EventBus<Event>::GetInstance().Publish(Event::RoomStatusChanged, roomId, winnerId, "game_ended");
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

    // 发布平局请求事件
    EventBus<Event>::GetInstance().Publish(Event::DrawRequested, roomId, userId);

    // 发布房间状态变化事件
    EventBus<Event>::GetInstance().Publish(Event::RoomStatusChanged, roomId, userId, "draw_requested");

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

    // 确定胜利者（对方）
    uint64_t winnerId = 0;
    if (userId == blackPlayerId)
    {
        winnerId = whitePlayerId;
    }
    else if (userId == whitePlayerId)
    {
        winnerId = blackPlayerId;
    }

    // 发布认输事件
    EventBus<Event>::GetInstance().Publish(Event::GiveUpRequested, roomId, userId);

    // 发布游戏结束事件（对方获胜）
    if (winnerId != 0)
    {
        EventBus<Event>::GetInstance().Publish(Event::GameEnded, roomId, winnerId);
    }

    // 发布房间状态变化事件
    EventBus<Event>::GetInstance().Publish(Event::RoomStatusChanged, roomId, userId, "give_up");

    return true;
}

std::string Room::GetError() const
{
    return error;
}