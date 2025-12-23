#include "Logger.h"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <algorithm>

// 静态成员初始化
Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

Logger::Logger()
    : currentLevel(LogLevel::INFO), outputToConsole(true), outputToFile(false)
{
    // 默认构造函数，不打开文件
}

Logger::~Logger()
{
    shutdown();
}

std::string Logger::getCurrentTime() const
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;
    auto timer = std::chrono::system_clock::to_time_t(now);

    std::tm bt;
#ifdef _WIN32
    localtime_s(&bt, &timer);
#else
    localtime_r(&timer, &bt);
#endif

    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) const
{
    switch (level)
    {
    case LogLevel::TRACE:
        return "TRACE";
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARN:
        return "WARN";
    case LogLevel::ERROR:
        return "ERROR";
    case LogLevel::FATAL:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

void Logger::writeLog(LogLevel level, const std::string &message)
{
    // 检查日志级别是否足够
    if (level < currentLevel)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(logMutex);

    std::string timeStr = getCurrentTime();
    std::string levelStr = levelToString(level);
    std::string logEntry = "[" + timeStr + "] [" + levelStr + "] " + message + "\n";

    // 输出到控制台
    if (outputToConsole)
    {
        // 错误和致命错误输出到stderr，其他输出到stdout
        if (level >= LogLevel::ERROR)
        {
            std::cerr << logEntry;
            std::cerr.flush();
        }
        else
        {
            std::cout << logEntry;
            std::cout.flush();
        }
    }

    // 输出到文件
    if (outputToFile && logFile.is_open())
    {
        logFile << logEntry;
        logFile.flush();
    }
}
void Logger::init(const std::string &filePath, LogLevel level, bool consoleOutput)
{
    Logger &logger = instance();
    bool openSuccess = false;

    // 1. 缩小锁的范围，只保护资源切换逻辑
    {
        std::lock_guard<std::mutex> lock(logger.logMutex);

        if (logger.logFile.is_open())
            logger.logFile.close();

        logger.currentLevel = level;
        logger.outputToConsole = consoleOutput;

        if (!filePath.empty())
        {
            std::filesystem::path path(filePath);
            if (path.has_parent_path())
            {
                std::filesystem::create_directories(path.parent_path());
            }

            logger.logFile.open(filePath, std::ios::out | std::ios::app);
            if (logger.logFile.is_open())
            {
                logger.outputToFile = true;
                logger.logFilePath = filePath;
                openSuccess = true;
            }
            else
            {
                logger.outputToFile = false;
            }
        }
        else
        {
            logger.outputToFile = false;
        }
    } // <--- 锁在这里释放

    // 2. 在锁之外打印初始日志，避免死锁
    if (!filePath.empty())
    {
        if (openSuccess)
        {
            logger.info("Logger initialized. Log file: " + filePath);
        }
        else
        {
            logger.error("Failed to open log file: " + filePath);
        }
    }
    else
    {
        logger.info("Logger initialized without file output.");
    }
}

void Logger::setLevel(LogLevel level)
{
    Logger &logger = instance();
    std::lock_guard<std::mutex> lock(logger.logMutex);
    logger.currentLevel = level;
}

void Logger::trace(const std::string &message)
{
    instance().writeLog(LogLevel::TRACE, message);
}

void Logger::debug(const std::string &message)
{
    instance().writeLog(LogLevel::DEBUG, message);
}

void Logger::info(const std::string &message)
{
    instance().writeLog(LogLevel::INFO, message);
}

void Logger::warn(const std::string &message)
{
    instance().writeLog(LogLevel::WARN, message);
}

void Logger::error(const std::string &message)
{
    instance().writeLog(LogLevel::ERROR, message);
}

void Logger::fatal(const std::string &message)
{
    instance().writeLog(LogLevel::FATAL, message);
}

void Logger::log(LogLevel level, const char *format, ...)
{
    if (!isEnabled(level))
    {
        return;
    }

    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    instance().writeLog(level, std::string(buffer));
}

bool Logger::isEnabled(LogLevel level)
{
    Logger &logger = instance();
    return level >= logger.currentLevel;
}

void Logger::flush()
{
    Logger &logger = instance();
    std::lock_guard<std::mutex> lock(logger.logMutex);

    if (logger.outputToConsole)
    {
        std::cout.flush();
        std::cerr.flush();
    }

    if (logger.outputToFile && logger.logFile.is_open())
    {
        logger.logFile.flush();
    }
}

void Logger::shutdown()
{
    Logger &logger = instance();
    std::lock_guard<std::mutex> lock(logger.logMutex);

    if (logger.logFile.is_open())
    {
        logger.logFile << "[" << logger.getCurrentTime() << "] [INFO] Logger shutting down.\n";
        logger.logFile.close();
    }

    logger.outputToFile = false;
}