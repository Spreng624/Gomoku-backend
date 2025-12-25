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
    Logger::init("./gomoku.log", LogLevel::DEBUG, true);
    LOG_DEBUG("============= Initializing Gomoku-backend =============");

    Database &db = Database::GetInstance();
    if (!db.Initialize("gomoku.db"))
    {
        LOG_ERROR("Failed to initialize database");
        return 1;
    }
    ObjectManager objMgr;
    Server server;
    Handler msgHandler(objMgr, [&server](const Packet &packet)
                       { server.SendPacket(packet); });
    Notifier broadcaster(objMgr);

    server.SetOnPacketCallback([&msgHandler](const Packet &packet)
                               { msgHandler.HandlePacket(packet); });
    broadcaster.SetSendPacketCallback([&server](const Packet &packet)
                                      { server.SendPacket(packet); });

    LOG_DEBUG("=======================================================");

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
