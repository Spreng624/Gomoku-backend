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

    bool seatChanged = false;
    if (userId == blackPlayerId)
    {
        blackPlayerId = 0;
        seatChanged = true;
    }
    if (userId == whitePlayerId)
    {
        whitePlayerId = 0;
        seatChanged = true;
    }

    // 如果座位发生变化，发布SyncSeat事件
    if (seatChanged)
    {
        EventBus<Event>::GetInstance().Publish(Event::SyncSeat, roomId, blackPlayerId, whitePlayerId);
    }

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
    if (status == RoomStatus::Playing)
    {
        error = "Game already started";
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

bool Room::SyncSeat(uint64_t userId, uint64_t blackPlayerId, uint64_t whitePlayerId)
{
    // 基础过滤
    if (status == RoomStatus::Playing)
    {
        error = "Game already started";
        return false;
    }
    if (!isInRoom(userId))
    {
        error = "Player not in room";
        return false;
    }

    if (userId == 0)
    {
        // 用来主动推送SyncSeat事件
        EventBus<Event>::GetInstance().Publish(Event::SyncSeat, roomId, this->blackPlayerId, this->whitePlayerId);
        return true;
    }
    if (blackPlayerId == 0 && whitePlayerId == 0)
    {
        // 取消就坐请求，如果玩家已经就坐，则取消座位
        if (userId == this->blackPlayerId)
        {
            this->blackPlayerId = 0;
        }
        else if (userId == this->whitePlayerId)
        {
            this->whitePlayerId = 0;
        }

        EventBus<Event>::GetInstance().Publish(Event::SyncSeat, roomId, this->blackPlayerId, this->whitePlayerId);
        return true;
    }
    else if (userId == blackPlayerId && whitePlayerId == 0)
    {
        // 玩家选择黑棋
        if (this->blackPlayerId == 0 || this->blackPlayerId == userId)
        {
            this->blackPlayerId = userId;
            if (this->whitePlayerId == userId)
                this->whitePlayerId = 0;
            EventBus<Event>::GetInstance().Publish(Event::SyncSeat, roomId, this->blackPlayerId, this->whitePlayerId);
            return true;
        }
    }
    else if (userId == whitePlayerId && blackPlayerId == 0)
    {
        // 玩家选择白棋
        if (this->whitePlayerId == 0 || this->whitePlayerId == userId)
        {
            this->whitePlayerId = userId;
            if (this->blackPlayerId == userId)
                this->blackPlayerId = 0;
            EventBus<Event>::GetInstance().Publish(Event::SyncSeat, roomId, this->blackPlayerId, this->whitePlayerId);
            return true;
        }
    }

    error = "Invalid Seat";
    return false;
}

bool Room::MakeMove(uint64_t userId, uint32_t x, uint32_t y)
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

    if (game.checkWin(x, y) != Piece::EMPTY)
    {
        status = RoomStatus::End;
        EventBus<Event>::GetInstance().Publish(Event::GameEnded, roomId, userId);
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

// 私有辅助方法
bool Room::isInRoom(uint64_t userId) const
{
    if (std::find(playerIds.begin(), playerIds.end(), userId) != playerIds.end())
    {
        return true;
    }
    return false;
}