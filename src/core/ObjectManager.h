#ifndef MANAGER_H
#define MANAGER_H

#include "EventBus.hpp"
#include "User.h"
#include "Player.h"
#include "Game.h"
#include "Room.h"
#include <unordered_map>
#include <memory>
#include <cstdint>

class ObjectManager
{
private:
    EventBus<Event> &eventBus;

    // 对象容器
    std::unordered_map<uint64_t, std::unique_ptr<User>> users;     // userId -> User
    std::unordered_map<std::string, uint64_t> usernameToUserIdMap; // username -> userId
    std::unordered_map<uint64_t, std::unique_ptr<Room>> rooms;     // roomId -> Room
    std::unordered_map<uint64_t, std::unique_ptr<Player>> players; // playerId -> Player
    std::unordered_map<uint64_t, uint64_t> sessionIdToUserIdMap;   // sessionId -> userId
    std::unordered_map<uint64_t, uint64_t> sessionIdToPlayerIdMap; // sessionId -> playerId
    std::unordered_map<uint64_t, uint64_t> userIdToSessionIdMap;   // userId -> sessionId（反向映射）

    uint64_t nextUserId = 1;
    uint64_t nextRoomId = 1;

    // 从数据库加载用户
    void LoadUsersFromDatabase();

public:
    ObjectManager(EventBus<Event> &eventBus);
    ~ObjectManager();

    // --- User 生命周期 API ---
    User *CreateUser(const std::string &username, const std::string &password);
    User *GetUserByUsername(const std::string &username);
    User *GetUserByUserId(uint64_t userId);
    bool RemoveUser(uint64_t userId);

    // --- Room 生命周期 API ---
    Room *CreateRoom(uint64_t ownerId);
    Room *GetRoom(uint64_t roomId);
    bool RemoveRoom(uint64_t roomId);

    // --- Player 生命周期 API ---
    Player *CreatePlayer(uint64_t sessionId, uint64_t userId = 0); // userId=0 表示游客
    Player *GetPlayer(uint64_t playerId);
    Player *GetPlayerBySessionId(uint64_t sessionId);
    bool RemovePlayer(uint64_t playerId);

    // --- Session 与 User/Player 的映射 ---
    void MapSessionToUser(uint64_t sessionId, uint64_t userId);
    uint64_t GetUserIdBySessionId(uint64_t sessionId);
    uint64_t GetSessionIdByUserId(uint64_t userId); // 新增：反向查询
    void UnmapSession(uint64_t sessionId);
};

#endif