#include "src/utils/Logger.h"
#include <iostream>
#include <thread>

void testBasicLogging()
{
    std::cout << "=== Testing basic logging ===" << std::endl;

    // 初始化Logger
    Logger::init("test.log", LogLevel::DEBUG, true);

    // 测试各个级别的日志
    Logger::trace("This is a TRACE message");
    Logger::debug("This is a DEBUG message");
    Logger::info("This is an INFO message");
    Logger::warn("This is a WARN message");
    Logger::error("This is an ERROR message");
    Logger::fatal("This is a FATAL message");

    // 测试格式化日志
    Logger::log(LogLevel::INFO, "Formatted log: %s %d %.2f", "test", 123, 3.14);
    LOG_INFO_FMT("Macro formatted: %s %d", "macro_test", 456);

    // 测试级别过滤
    Logger::setLevel(LogLevel::WARN);
    Logger::info("This INFO message should NOT appear");
    Logger::warn("This WARN message should appear");
    Logger::error("This ERROR message should appear");

    // 恢复级别
    Logger::setLevel(LogLevel::DEBUG);

    std::cout << "=== Basic logging test completed ===" << std::endl;
}

void testThreadSafety()
{
    std::cout << "=== Testing thread safety ===" << std::endl;

    auto logTask = [](int id)
    {
        for (int i = 0; i < 5; ++i)
        {
            LOG_INFO_FMT("Thread %d: Log message %d", id, i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    std::thread t1(logTask, 1);
    std::thread t2(logTask, 2);
    std::thread t3(logTask, 3);

    t1.join();
    t2.join();
    t3.join();

    std::cout << "=== Thread safety test completed ===" << std::endl;
}

int main()
{
    std::cout << "Starting Logger tests..." << std::endl;

    testBasicLogging();
    testThreadSafety();

    // 清理
    Logger::shutdown();

    std::cout << "All tests completed successfully!" << std::endl;
    return 0;
}