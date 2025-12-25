#include "ObjectManager.h"
#include "Database.h"
#include "Logger.h"
#include <algorithm>
#include <sstream>

ObjectManager::ObjectManager()
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

    // 发布房间创建事件
    EventBus<Event>::GetInstance().Publish(Event::RoomCreated, roomId, ownerId);

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

    // 清理该房间中所有用户的映射
    Room *room = it->second.get();
    if (room)
    {
        // 遍历房间中的所有玩家ID
        for (uint64_t userId : room->playerIds)
        {
            userIdToRoomIdMap.erase(userId);
        }
    }

    rooms.erase(it);
    return true;
}

// --- Session 与 User 的映射 ---

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
}

// --- User 与 Room 的映射 ---

uint64_t ObjectManager::GetRoomIdByUserId(uint64_t userId)
{
    auto it = userIdToRoomIdMap.find(userId);
    if (it == userIdToRoomIdMap.end())
        return 0;
    return it->second;
}

void ObjectManager::MapUserToRoom(uint64_t userId, uint64_t roomId)
{
    userIdToRoomIdMap[userId] = roomId;
}

void ObjectManager::UnmapUserFromRoom(uint64_t userId)
{
    userIdToRoomIdMap.erase(userId);
}

// --- 列表查询 API ---

std::vector<User *> ObjectManager::GetUserList(size_t maxCount)
{
    std::vector<User *> result;
    result.reserve(std::min(maxCount, users.size()));

    size_t count = 0;
    for (const auto &pair : users)
    {
        if (count >= maxCount)
            break;

        result.push_back(pair.second.get());
        count++;
    }

    return result;
}

std::vector<Room *> ObjectManager::GetRoomList(size_t maxCount)
{
    std::vector<Room *> result;
    result.reserve(std::min(maxCount, rooms.size()));

    size_t count = 0;
    for (const auto &pair : rooms)
    {
        if (count >= maxCount)
            break;

        result.push_back(pair.second.get());
        count++;
    }

    return result;
}
