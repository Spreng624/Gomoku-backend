#ifndef MANAGER_H
#define MANAGER_H

#include "EventBus.hpp"
#include "User.h"
#include "Game.h"
#include "Room.h"
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <vector>

class ObjectManager
{
private:
    // 对象容器
    std::unordered_map<uint64_t, std::unique_ptr<User>> users;     // userId    -> User
    std::unordered_map<uint64_t, std::unique_ptr<Room>> rooms;     // roomId    -> Room
    std::unordered_map<std::string, uint64_t> usernameToUserIdMap; // username  -> userId
    std::unordered_map<uint64_t, uint64_t> sessionIdToUserIdMap;   // sessionId -> userId
    std::unordered_map<uint64_t, uint64_t> userIdToSessionIdMap;   // userId    -> sessionId（反向映射）
    std::unordered_map<uint64_t, uint64_t> userIdToRoomIdMap;      // userId    -> roomId（用户所在房间）

    uint64_t nextUserId = 1;
    uint64_t nextRoomId = 1;

    // 从数据库加载用户
    void LoadUsersFromDatabase();

public:
    ObjectManager();
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

    // --- Session 与 User 的映射 ---
    void MapSessionToUser(uint64_t sessionId, uint64_t userId);
    uint64_t GetUserIdBySessionId(uint64_t sessionId);
    uint64_t GetSessionIdByUserId(uint64_t userId); // 新增：反向查询
    void UnmapSession(uint64_t sessionId);

    // --- User 与 Room 的映射 ---
    uint64_t GetRoomIdByUserId(uint64_t userId);          // 查询用户所在房间ID，返回0表示不在任何房间
    void MapUserToRoom(uint64_t userId, uint64_t roomId); // 将用户映射到房间
    void UnmapUserFromRoom(uint64_t userId);              // 将用户从房间映射中移除

    // --- 列表查询 API ---
    std::vector<User *> GetUserList(size_t maxCount);
    std::vector<Room *> GetRoomList(size_t maxCount);
};

#endif