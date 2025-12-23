/**
 * @file Metrics.h
 * @brief 性能监控和指标收集模块
 *
 * 提供服务器运行时的关键性能指标收集功能：
 * 1. 请求统计（请求数、成功率、延迟）
 * 2. 连接统计（活跃连接数、连接总数）
 * 3. 游戏统计（游戏数、玩家数、房间数）
 * 4. 系统资源监控（内存、CPU使用率）
 * 5. 数据库性能指标
 */

#ifndef METRICS_H
#define METRICS_H

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Metrics
{

    /**
     * @brief 指标类型枚举
     */
    enum class MetricType
    {
        COUNTER,   // 计数器，只增不减
        GAUGE,     // 仪表盘，可增可减
        HISTOGRAM, // 直方图，用于统计分布
        TIMER      // 计时器，用于测量时间间隔
    };

    /**
     * @brief 指标数据点
     */
    struct MetricPoint
    {
        std::string name;                                // 指标名称
        MetricType type;                                 // 指标类型
        double value;                                    // 当前值
        std::chrono::system_clock::time_point timestamp; // 时间戳
        std::map<std::string, std::string> labels;       // 标签（用于维度切分）
    };

    /**
     * @brief 性能指标收集器
     */
    class MetricsCollector
    {
    public:
        static MetricsCollector &GetInstance();

        // 禁止拷贝和移动
        MetricsCollector(const MetricsCollector &) = delete;
        MetricsCollector &operator=(const MetricsCollector &) = delete;

        /**
         * @brief 初始化指标收集器
         * @param enableSystemMetrics 是否启用系统指标收集
         */
        void Initialize(bool enableSystemMetrics = true);

        /**
         * @brief 关闭指标收集器
         */
        void Shutdown();

        // ========== 计数器操作 ==========

        /**
         * @brief 增加计数器
         * @param name 指标名称
         * @param value 增加值
         * @param labels 标签
         */
        void IncrementCounter(const std::string &name, double value = 1.0,
                              const std::map<std::string, std::string> &labels = {});

        /**
         * @brief 获取计数器值
         */
        double GetCounterValue(const std::string &name,
                               const std::map<std::string, std::string> &labels = {}) const;

        // ========== 仪表盘操作 ==========

        /**
         * @brief 设置仪表盘值
         */
        void SetGauge(const std::string &name, double value,
                      const std::map<std::string, std::string> &labels = {});

        /**
         * @brief 增加仪表盘值
         */
        void IncrementGauge(const std::string &name, double value = 1.0,
                            const std::map<std::string, std::string> &labels = {});

        /**
         * @brief 减少仪表盘值
         */
        void DecrementGauge(const std::string &name, double value = 1.0,
                            const std::map<std::string, std::string> &labels = {});

        // ========== 计时器操作 ==========

        /**
         * @brief 计时器作用域类
         */
        class TimerScope
        {
        public:
            TimerScope(const std::string &name,
                       const std::map<std::string, std::string> &labels = {});
            ~TimerScope();

        private:
            std::string name_;
            std::map<std::string, std::string> labels_;
            std::chrono::steady_clock::time_point start_;
        };

        /**
         * @brief 记录时间间隔
         */
        void RecordTimer(const std::string &name, double durationMs,
                         const std::map<std::string, std::string> &labels = {});

        // ========== 指标查询 ==========

        /**
         * @brief 获取所有指标
         */
        std::vector<MetricPoint> GetAllMetrics() const;

        /**
         * @brief 获取指定指标的当前值
         */
        double GetMetricValue(const std::string &name,
                              const std::map<std::string, std::string> &labels = {}) const;

        /**
         * @brief 获取指标快照（JSON格式）
         */
        std::string GetMetricsSnapshot() const;

        /**
         * @brief 重置所有指标
         */
        void ResetAllMetrics();

        // ========== 预定义指标 ==========

        // 网络相关指标
        void RecordRequest(const std::string &endpoint, bool success, double durationMs);
        void RecordConnectionOpened();
        void RecordConnectionClosed();

        // 游戏相关指标
        void RecordGameCreated();
        void RecordGameFinished();
        void RecordMoveMade();

        // 数据库相关指标
        void RecordDatabaseQuery(const std::string &queryType, bool success, double durationMs);

    private:
        MetricsCollector();
        ~MetricsCollector();

        // 内部数据结构
        struct MetricData
        {
            MetricType type;
            std::atomic<double> value;
            std::map<std::string, std::string> labels;
            mutable std::mutex mutex;
        };

        // 生成指标键名
        std::string GenerateKey(const std::string &name,
                                const std::map<std::string, std::string> &labels) const;

        // 系统指标收集线程
        void SystemMetricsThread();

    private:
        mutable std::mutex metricsMutex_;
        std::map<std::string, std::shared_ptr<MetricData>> metrics_;

        std::atomic<bool> running_{false};
        std::thread systemMetricsThread_;

        // 预定义指标名称
        static constexpr const char *METRIC_REQUESTS_TOTAL = "gomoku_requests_total";
        static constexpr const char *METRIC_REQUESTS_DURATION = "gomoku_request_duration_ms";
        static constexpr const char *METRIC_CONNECTIONS_ACTIVE = "gomoku_connections_active";
        static constexpr const char *METRIC_CONNECTIONS_TOTAL = "gomoku_connections_total";
        static constexpr const char *METRIC_GAMES_ACTIVE = "gomoku_games_active";
        static constexpr const char *METRIC_GAMES_TOTAL = "gomoku_games_total";
        static constexpr const char *METRIC_MOVES_TOTAL = "gomoku_moves_total";
        static constexpr const char *METRIC_DB_QUERIES_TOTAL = "gomoku_db_queries_total";
        static constexpr const char *METRIC_DB_QUERY_DURATION = "gomoku_db_query_duration_ms";
        static constexpr const char *METRIC_MEMORY_USAGE = "gomoku_memory_usage_bytes";
        static constexpr const char *METRIC_CPU_USAGE = "gomoku_cpu_usage_percent";
    };

/**
 * @brief 便捷宏：创建计时器作用域
 */
#define METRICS_TIMER_SCOPE(name, ...) \
    Metrics::MetricsCollector::TimerScope __metrics_timer_scope(name, ##__VA_ARGS__)

/**
 * @brief 便捷宏：记录请求
 */
#define METRICS_RECORD_REQUEST(endpoint, success, duration) \
    Metrics::MetricsCollector::GetInstance().RecordRequest(endpoint, success, duration)

/**
 * @brief 便捷宏：记录数据库查询
 */
#define METRICS_RECORD_DB_QUERY(queryType, success, duration) \
    Metrics::MetricsCollector::GetInstance().RecordDatabaseQuery(queryType, success, duration)

} // namespace Metrics

#endif // METRICS_H