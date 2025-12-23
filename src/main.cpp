#include "EventBus.hpp"
#include "Server.h"
#include "ObjectManager.h"
#include "Handler.h"
#include "Notifier.h"
#include "Database.h"
#include "utils/Logger.h"

#include <iomanip>
#include <cstdint>
#include <limits>
#include <chrono>

#define PORT 8080

int main()
{
    // 初始化Logger
    Logger::init("./gomoku.log", LogLevel::DEBUG, true);
    LOG_INFO("Initializing Gomoku-backend...");

    // 初始化数据库
    Database &db = Database::GetInstance();
    if (!db.Initialize("gomoku.db"))
    {
        LOG_ERROR("Failed to initialize database");
        return 1;
    }

    EventBus<Event> eventBus;
    ObjectManager objMgr(eventBus);
    Server server;
    Handler msgHandler(objMgr, eventBus, [&server](const Packet &packet)
                       { server.SendPacket(packet); });
    Notifier broadcaster(eventBus, objMgr);

    // 连接 Server 与 Handler
    server.SetOnPacketCallback([&msgHandler](const Packet &packet)
                               { msgHandler.HandlePacket(packet); });

    // 连接 Server 与 Notifier
    broadcaster.SetSendPacketCallback([&server](const Packet &packet)
                                      { server.SendPacket(packet); });

    LOG_INFO("Gomoku-backend initialized.");
    if (server.Init() != 0)
    {
        LOG_ERROR("Failed to initialize server");
        Logger::shutdown();
        return 1;
    }
    server.Run();
    server.Stop();
    Logger::shutdown();
    return 0;
}
