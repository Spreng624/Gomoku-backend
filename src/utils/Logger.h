#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

#ifdef ERROR
#undef ERROR // Windows 头文件定义了 ERROR 宏，需要取消定义
#endif

/**
 * @brief 日志级别枚举
 */
enum class LogLevel
{
    TRACE, ///< 跟踪信息，最详细的日志
    DEBUG, ///< 调试信息
    INFO,  ///< 一般信息
    WARN,  ///< 警告信息
    ERROR, ///< 错误信息
    FATAL  ///< 致命错误
};

/**
 * @brief 单例日志类
 *
 * 提供线程安全的日志记录功能，支持多级别日志输出到控制台和文件。
 * 使用Meyer's Singleton模式实现，保证线程安全且延迟初始化。
 */
class Logger
{
private:
    // 单例实例
    static Logger &instance();

    // 私有构造函数
    Logger();
    ~Logger();

    // 禁止拷贝和赋值
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    // 成员变量
    std::ofstream logFile;
    std::mutex logMutex;
    LogLevel currentLevel;
    bool outputToConsole;
    bool outputToFile;
    std::string logFilePath;

    /**
     * @brief 获取当前时间的格式化字符串
     * @return 格式为 "YYYY-MM-DD HH:MM:SS.mmm" 的时间字符串
     */
    std::string getCurrentTime() const;

    /**
     * @brief 获取日志级别的字符串表示
     * @param level 日志级别
     * @return 级别字符串（如 "INFO", "ERROR"）
     */
    std::string levelToString(LogLevel level) const;

    /**
     * @brief 实际写入日志的内部方法
     * @param level 日志级别
     * @param message 日志消息
     */
    void writeLog(LogLevel level, const std::string &message);

public:
    /**
     * @brief 获取Logger单例实例
     * @return Logger实例的引用
     */
    static Logger &getInstance()
    {
        return instance();
    }

    /**
     * @brief 初始化日志系统
     * @param filePath 日志文件路径（如果为空则不输出到文件）
     * @param level 最低日志级别
     * @param consoleOutput 是否输出到控制台
     */
    static void init(const std::string &filePath = "",
                     LogLevel level = LogLevel::INFO,
                     bool consoleOutput = true);

    /**
     * @brief 设置日志级别
     * @param level 新的最低日志级别
     */
    static void setLevel(LogLevel level);

    /**
     * @brief 记录TRACE级别日志
     * @param message 日志消息
     */
    static void trace(const std::string &message);

    /**
     * @brief 记录DEBUG级别日志
     * @param message 日志消息
     */
    static void debug(const std::string &message);

    /**
     * @brief 记录INFO级别日志
     * @param message 日志消息
     */
    static void info(const std::string &message);

    /**
     * @brief 记录WARN级别日志
     * @param message 日志消息
     */
    static void warn(const std::string &message);

    /**
     * @brief 记录ERROR级别日志
     * @param message 日志消息
     */
    static void error(const std::string &message);

    /**
     * @brief 记录FATAL级别日志
     * @param message 日志消息
     */
    static void fatal(const std::string &message);

    /**
     * @brief 格式化日志（类似printf）
     * @param level 日志级别
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    static void log(LogLevel level, const char *format, ...);

    /**
     * @brief 检查是否启用特定级别的日志
     * @param level 要检查的日志级别
     * @return 如果该级别日志会被记录则返回true
     */
    static bool isEnabled(LogLevel level);

    /**
     * @brief 刷新日志缓冲区
     */
    static void flush();

    /**
     * @brief 关闭日志系统
     */
    static void shutdown();
};

// 为了方便使用而定义的宏
#define LOG_TRACE(msg) Logger::trace(msg)
#define LOG_DEBUG(msg) Logger::debug(msg)
#define LOG_INFO(msg) Logger::info(msg)
#define LOG_WARN(msg) Logger::warn(msg)
#define LOG_ERROR(msg) Logger::error(msg)
#define LOG_FATAL(msg) Logger::fatal(msg)

#define LOG_TRACE_FMT(fmt, ...) Logger::log(LogLevel::TRACE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG_FMT(fmt, ...) Logger::log(LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO_FMT(fmt, ...) Logger::log(LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN_FMT(fmt, ...) Logger::log(LogLevel::WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR_FMT(fmt, ...) Logger::log(LogLevel::ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL_FMT(fmt, ...) Logger::log(LogLevel::FATAL, fmt, ##__VA_ARGS__)

#endif // LOGGER_H