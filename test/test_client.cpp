#include "src/network/Client.h"
#include "src/utils/Logger.h"
#include <iostream>
#include <thread>
#include <chrono>

int main()
{
    // 初始化Logger
    Logger::init("test_client.log", LogLevel::DEBUG, true);
    LOG_INFO("Starting Client test...");

    // 创建Client实例
    Client client("127.0.0.1", 8080);

    // 尝试连接
    LOG_INFO("Attempting to connect to server...");
    int result = client.Connect();

    if (result != 0)
    {
        LOG_ERROR("Failed to connect to server");
        std::cout << "Connection failed. Make sure server is running on port 8080." << std::endl;
        return 1;
    }

    LOG_INFO("Connected successfully. Starting client loop in separate thread...");

    // 在单独线程中运行客户端
    std::thread clientThread([&client]()
                             { client.Run(); });

    // 等待一段时间让握手完成
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 检查连接状态
    if (client.IsConnected())
    {
        LOG_INFO("Client is connected and session is active!");
        std::cout << "Client is connected and session is active!" << std::endl;
    }
    else
    {
        LOG_WARN("Client is not fully connected (handshake may not be complete)");
        std::cout << "Client connected but session not active (encryption not implemented)" << std::endl;
    }

    // 等待一段时间
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 断开连接
    LOG_INFO("Disconnecting...");
    client.Disconnect();

    // 等待线程结束
    if (clientThread.joinable())
    {
        clientThread.join();
    }

    LOG_INFO("Client test completed.");
    std::cout << "Test completed successfully!" << std::endl;

    Logger::shutdown();
    return 0;
}