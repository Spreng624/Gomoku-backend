#include "ObjectManager.h"
#include "Database.h"
#include "Logger.h"
#include <algorithm>
#include <sstream>

ObjectManager::ObjectManager(EventBus<Event> &eventBus) : eventBus(eventBus)
{
    // 从数据库加载所有用户
    LoadUsersFromDatabase();
}

ObjectManager::~ObjectManager()
{
}

void ObjectManager::LoadUsersFromDatabase()
{
    Database &db = Database::GetInstance();
    if (!db.IsInitialized())
        return;

    auto results = db.Query("SELECT id, username, password FROM users;");

    for (const auto &row : results)
    {
        if (row.size() < 3)
            continue;

        uint64_t userId = std::stoull(row[0]);
        std::string username = row[1];
        std::string password = row[2];

        auto user = std::make_unique<User>(username, password);
        user->id = userId;
        user->LoadFromDatabase(userId);

        users[userId] = std::move(user);
        usernameToUserIdMap[username] = userId;

        if (userId >= nextUserId)
            nextUserId = userId + 1;
    }

    LOG_INFO("Loaded " + std::to_string(users.size()) + " users from database");
}

// --- User 生命周期 API ---

User *ObjectManager::CreateUser(const std::string &username, const std::string &password)
{
    // 检查用户名是否已存在
    if (usernameToUserIdMap.find(username) != usernameToUserIdMap.end())
    {
        return nullptr;
    }

    Database &db = Database::GetInstance();

    // 插入到数据库
    std::ostringstream sql;
    sql << "INSERT INTO users (username, password) VALUES ('" << username << "', '" << password << "');";

    if (!db.Execute(sql.str()))
    {
        return nullptr;
    }

    // 查询刚插入的 ID
    std::ostringstream querySql;
    querySql << "SELECT id FROM users WHERE username='" << username << "';";
    std::string idStr = db.QueryValue(querySql.str());

    if (idStr.empty())
        return nullptr;

    uint64_t userId = std::stoull(idStr);

    auto user = std::make_unique<User>(username, password);
    user->id = userId;
    User *ptr = user.get();

    users[userId] = std::move(user);
    usernameToUserIdMap[username] = userId;

    return ptr;
}

User *ObjectManager::GetUserByUsername(const std::string &username)
{
    auto it = usernameToUserIdMap.find(username);
    if (it == usernameToUserIdMap.end())
        return nullptr;
    return users[it->second].get();
}

User *ObjectManager::GetUserByUserId(uint64_t userId)
{
    auto it = users.find(userId);
    if (it == users.end())
        return nullptr;
    return it->second.get();
}

bool ObjectManager::RemoveUser(uint64_t userId)
{
    auto it = users.find(userId);
    if (it == users.end())
        return false;

    std::string username = it->second->GetUsername();
    usernameToUserIdMap.erase(username);
    users.erase(it);

    // 从数据库删除
    Database &db = Database::GetInstance();
    std::ostringstream sql;
    sql << "DELETE FROM users WHERE id=" << userId << ";";
    db.Execute(sql.str());

    return true;
}

// --- Room 生命周期 API ---

Room *ObjectManager::CreateRoom(uint64_t ownerId)
{
    uint64_t roomId = nextRoomId++;
    auto room = std::make_unique<Room>(roomId);
    Room *ptr = room.get();
    rooms[roomId] = std::move(room);
    return ptr;
}

Room *ObjectManager::GetRoom(uint64_t roomId)
{
    auto it = rooms.find(roomId);
    if (it == rooms.end())
        return nullptr;
    return it->second.get();
}

bool ObjectManager::RemoveRoom(uint64_t roomId)
{
    auto it = rooms.find(roomId);
    if (it == rooms.end())
        return false;
    rooms.erase(it);
    return true;
}

// --- Player 生命周期 API ---

Player *ObjectManager::CreatePlayer(uint64_t sessionId, uint64_t userId)
{
    // 检查 sessionId 是否已有玩家
    if (sessionIdToPlayerIdMap.find(sessionId) != sessionIdToPlayerIdMap.end())
    {
        return nullptr; // 该 session 已有玩家
    }

    uint64_t playerId = sessionId; // 简单起见，用 sessionId 作为 playerId
    auto player = std::make_unique<Player>(eventBus);
    Player *ptr = player.get();
    players[playerId] = std::move(player);
    sessionIdToPlayerIdMap[sessionId] = playerId;

    // 如果指定了 userId，也建立映射
    if (userId != 0)
    {
        sessionIdToUserIdMap[sessionId] = userId;
    }

    return ptr;
}

Player *ObjectManager::GetPlayer(uint64_t playerId)
{
    auto it = players.find(playerId);
    if (it == players.end())
        return nullptr;
    return it->second.get();
}

Player *ObjectManager::GetPlayerBySessionId(uint64_t sessionId)
{
    auto it = sessionIdToPlayerIdMap.find(sessionId);
    if (it == sessionIdToPlayerIdMap.end())
        return nullptr;
    return GetPlayer(it->second);
}

bool ObjectManager::RemovePlayer(uint64_t playerId)
{
    auto it = players.find(playerId);
    if (it == players.end())
        return false;

    // 清理 sessionId 映射
    for (auto &pair : sessionIdToPlayerIdMap)
    {
        if (pair.second == playerId)
        {
            sessionIdToPlayerIdMap.erase(pair.first);
            break;
        }
    }

    players.erase(it);
    return true;
}

// --- Session 与 User/Player 的映射 ---

void ObjectManager::MapSessionToUser(uint64_t sessionId, uint64_t userId)
{
    sessionIdToUserIdMap[sessionId] = userId;
    userIdToSessionIdMap[userId] = sessionId; // 新增：维护反向映射
}

uint64_t ObjectManager::GetUserIdBySessionId(uint64_t sessionId)
{
    auto it = sessionIdToUserIdMap.find(sessionId);
    if (it == sessionIdToUserIdMap.end())
        return 0;
    return it->second;
}

uint64_t ObjectManager::GetSessionIdByUserId(uint64_t userId)
{
    auto it = userIdToSessionIdMap.find(userId);
    if (it == userIdToSessionIdMap.end())
        return 0;
    return it->second;
}

void ObjectManager::UnmapSession(uint64_t sessionId)
{
    auto it = sessionIdToUserIdMap.find(sessionId);
    if (it != sessionIdToUserIdMap.end())
    {
        uint64_t userId = it->second;
        userIdToSessionIdMap.erase(userId); // 清理反向映射
    }
    sessionIdToUserIdMap.erase(sessionId);
    sessionIdToPlayerIdMap.erase(sessionId);
}
