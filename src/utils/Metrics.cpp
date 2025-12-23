/**
 * @file Metrics.cpp
 * @brief 性能监控和指标收集模块实现
 */

#include "Metrics.h"
#include "Logger.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace Metrics
{

    // ========== MetricsCollector 实现 ==========

    MetricsCollector &MetricsCollector::GetInstance()
    {
        static MetricsCollector instance;
        return instance;
    }

    MetricsCollector::MetricsCollector() : running_(false)
    {
        // 初始化预定义指标
        Initialize(false); // 先不启动系统指标线程
    }

    MetricsCollector::~MetricsCollector()
    {
        Shutdown();
    }

    void MetricsCollector::Initialize(bool enableSystemMetrics)
    {
        std::lock_guard<std::mutex> lock(metricsMutex_);

        if (running_)
        {
            return;
        }

        LOG_INFO("Initializing metrics collector...");

        // 初始化预定义指标
        SetGauge(METRIC_CONNECTIONS_ACTIVE, 0.0);
        SetGauge(METRIC_GAMES_ACTIVE, 0.0);
        SetGauge(METRIC_MEMORY_USAGE, 0.0);
        SetGauge(METRIC_CPU_USAGE, 0.0);

        // 启动系统指标收集线程
        if (enableSystemMetrics)
        {
            running_ = true;
            systemMetricsThread_ = std::thread(&MetricsCollector::SystemMetricsThread, this);
            LOG_INFO("System metrics collection started");
        }
    }

    void MetricsCollector::Shutdown()
    {
        if (running_)
        {
            running_ = false;
            if (systemMetricsThread_.joinable())
            {
                systemMetricsThread_.join();
            }
            LOG_INFO("Metrics collector shutdown");
        }
    }

    std::string MetricsCollector::GenerateKey(const std::string &name,
                                              const std::map<std::string, std::string> &labels) const
    {
        std::string key = name;
        if (!labels.empty())
        {
            key += "{";
            bool first = true;
            for (const auto &[label, value] : labels)
            {
                if (!first)
                    key += ",";
                key += label + "=\"" + value + "\"";
                first = false;
            }
            key += "}";
        }
        return key;
    }

    void MetricsCollector::IncrementCounter(const std::string &name, double value,
                                            const std::map<std::string, std::string> &labels)
    {
        std::string key = GenerateKey(name, labels);

        std::lock_guard<std::mutex> lock(metricsMutex_);
        auto it = metrics_.find(key);
        if (it == metrics_.end())
        {
            auto data = std::make_shared<MetricData>();
            data->type = MetricType::COUNTER;
            data->value.store(value);
            data->labels = labels;
            metrics_[key] = data;
        }
        else
        {
            double old = it->second->value.load();
            it->second->value.store(old + value);
        }
    }

    double MetricsCollector::GetCounterValue(const std::string &name,
                                             const std::map<std::string, std::string> &labels) const
    {
        std::string key = GenerateKey(name, labels);

        std::lock_guard<std::mutex> lock(metricsMutex_);
        auto it = metrics_.find(key);
        if (it != metrics_.end() && it->second->type == MetricType::COUNTER)
        {
            return it->second->value.load();
        }
        return 0.0;
    }

    void MetricsCollector::SetGauge(const std::string &name, double value,
                                    const std::map<std::string, std::string> &labels)
    {
        std::string key = GenerateKey(name, labels);

        std::lock_guard<std::mutex> lock(metricsMutex_);
        auto it = metrics_.find(key);
        if (it == metrics_.end())
        {
            auto data = std::make_shared<MetricData>();
            data->type = MetricType::GAUGE;
            data->value.store(value);
            data->labels = labels;
            metrics_[key] = data;
        }
        else
        {
            it->second->value.store(value);
        }
    }

    void MetricsCollector::IncrementGauge(const std::string &name, double value,
                                          const std::map<std::string, std::string> &labels)
    {
        std::string key = GenerateKey(name, labels);

        std::lock_guard<std::mutex> lock(metricsMutex_);
        auto it = metrics_.find(key);
        if (it == metrics_.end())
        {
            auto data = std::make_shared<MetricData>();
            data->type = MetricType::GAUGE;
            data->value.store(value);
            data->labels = labels;
            metrics_[key] = data;
        }
        else
        {
            double old = it->second->value.load();
            it->second->value.store(old + value);
        }
    }

    void MetricsCollector::DecrementGauge(const std::string &name, double value,
                                          const std::map<std::string, std::string> &labels)
    {
        std::string key = GenerateKey(name, labels);

        std::lock_guard<std::mutex> lock(metricsMutex_);
        auto it = metrics_.find(key);
        if (it == metrics_.end())
        {
            auto data = std::make_shared<MetricData>();
            data->type = MetricType::GAUGE;
            data->value.store(-value);
            data->labels = labels;
            metrics_[key] = data;
        }
        else
        {
            double old = it->second->value.load();
            it->second->value.store(old - value);
        }
    }

    void MetricsCollector::RecordTimer(const std::string &name, double durationMs,
                                       const std::map<std::string, std::string> &labels)
    {
        // 记录总耗时
        IncrementCounter(name + "_total_ms", durationMs, labels);

        // 记录次数
        IncrementCounter(name + "_count", 1.0, labels);

        // 更新直方图（简化版：只记录平均值）
        std::string avgKey = GenerateKey(name + "_avg_ms", labels);
        std::lock_guard<std::mutex> lock(metricsMutex_);

        auto totalIt = metrics_.find(GenerateKey(name + "_total_ms", labels));
        auto countIt = metrics_.find(GenerateKey(name + "_count", labels));

        if (totalIt != metrics_.end() && countIt != metrics_.end())
        {
            double total = totalIt->second->value.load();
            double count = countIt->second->value.load();
            double avg = count > 0 ? total / count : 0.0;

            auto avgIt = metrics_.find(avgKey);
            if (avgIt == metrics_.end())
            {
                auto data = std::make_shared<MetricData>();
                data->type = MetricType::GAUGE;
                data->value.store(avg);
                data->labels = labels;
                metrics_[avgKey] = data;
            }
            else
            {
                avgIt->second->value.store(avg);
            }
        }
    }

    std::vector<MetricPoint> MetricsCollector::GetAllMetrics() const
    {
        std::lock_guard<std::mutex> lock(metricsMutex_);

        std::vector<MetricPoint> result;
        result.reserve(metrics_.size());

        auto now = std::chrono::system_clock::now();

        for (const auto &[key, data] : metrics_)
        {
            MetricPoint point;

            // 从key中提取名称和标签
            size_t bracePos = key.find('{');
            if (bracePos != std::string::npos)
            {
                point.name = key.substr(0, bracePos);
                // 简化处理：不解析标签
            }
            else
            {
                point.name = key;
            }

            point.type = data->type;
            point.value = data->value.load();
            point.timestamp = now;
            point.labels = data->labels;

            result.push_back(point);
        }

        return result;
    }

    double MetricsCollector::GetMetricValue(const std::string &name,
                                            const std::map<std::string, std::string> &labels) const
    {
        std::string key = GenerateKey(name, labels);

        std::lock_guard<std::mutex> lock(metricsMutex_);
        auto it = metrics_.find(key);
        if (it != metrics_.end())
        {
            return it->second->value.load();
        }
        return 0.0;
    }

    std::string MetricsCollector::GetMetricsSnapshot() const
    {
        auto metrics = GetAllMetrics();

        std::stringstream ss;
        ss << "{\n";
        ss << "  \"timestamp\": \"" << std::chrono::system_clock::now().time_since_epoch().count() << "\",\n";
        ss << "  \"metrics\": [\n";

        bool first = true;
        for (const auto &metric : metrics)
        {
            if (!first)
                ss << ",\n";

            ss << "    {\n";
            ss << "      \"name\": \"" << metric.name << "\",\n";
            ss << "      \"type\": \"";
            switch (metric.type)
            {
            case MetricType::COUNTER:
                ss << "counter";
                break;
            case MetricType::GAUGE:
                ss << "gauge";
                break;
            case MetricType::HISTOGRAM:
                ss << "histogram";
                break;
            case MetricType::TIMER:
                ss << "timer";
                break;
            }
            ss << "\",\n";
            ss << "      \"value\": " << std::fixed << std::setprecision(2) << metric.value << ",\n";

            ss << "      \"labels\": {";
            bool firstLabel = true;
            for (const auto &[label, value] : metric.labels)
            {
                if (!firstLabel)
                    ss << ", ";
                ss << "\"" << label << "\": \"" << value << "\"";
                firstLabel = false;
            }
            ss << "}\n";
            ss << "    }";

            first = false;
        }

        ss << "\n  ]\n";
        ss << "}";

        return ss.str();
    }

    void MetricsCollector::ResetAllMetrics()
    {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        metrics_.clear();

        // 重新初始化预定义指标
        SetGauge(METRIC_CONNECTIONS_ACTIVE, 0.0);
        SetGauge(METRIC_GAMES_ACTIVE, 0.0);
        SetGauge(METRIC_MEMORY_USAGE, 0.0);
        SetGauge(METRIC_CPU_USAGE, 0.0);
    }

    // ========== 预定义指标操作 ==========

    void MetricsCollector::RecordRequest(const std::string &endpoint, bool success, double durationMs)
    {
        std::map<std::string, std::string> labels = {{"endpoint", endpoint}, {"success", success ? "true" : "false"}};

        IncrementCounter(METRIC_REQUESTS_TOTAL, 1.0, labels);
        RecordTimer(METRIC_REQUESTS_DURATION, durationMs, labels);
    }

    void MetricsCollector::RecordConnectionOpened()
    {
        IncrementGauge(METRIC_CONNECTIONS_ACTIVE);
        IncrementCounter(METRIC_CONNECTIONS_TOTAL);
    }

    void MetricsCollector::RecordConnectionClosed()
    {
        DecrementGauge(METRIC_CONNECTIONS_ACTIVE);
    }

    void MetricsCollector::RecordGameCreated()
    {
        IncrementGauge(METRIC_GAMES_ACTIVE);
        IncrementCounter(METRIC_GAMES_TOTAL);
    }

    void MetricsCollector::RecordGameFinished()
    {
        DecrementGauge(METRIC_GAMES_ACTIVE);
    }

    void MetricsCollector::RecordMoveMade()
    {
        IncrementCounter(METRIC_MOVES_TOTAL);
    }

    void MetricsCollector::RecordDatabaseQuery(const std::string &queryType, bool success, double durationMs)
    {
        std::map<std::string, std::string> labels = {{"type", queryType}, {"success", success ? "true" : "false"}};

        IncrementCounter(METRIC_DB_QUERIES_TOTAL, 1.0, labels);
        RecordTimer(METRIC_DB_QUERY_DURATION, durationMs, labels);
    }

    // ========== 系统指标收集 ==========

    void MetricsCollector::SystemMetricsThread()
    {
        LOG_INFO("System metrics thread started");

        while (running_)
        {
            try
            {
                // 收集内存使用情况
#ifdef _WIN32
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
                {
                    SetGauge(METRIC_MEMORY_USAGE, static_cast<double>(pmc.WorkingSetSize));
                }
#else
                // Linux/MacOS
                std::ifstream statusFile("/proc/self/status");
                if (statusFile.is_open())
                {
                    std::string line;
                    while (std::getline(statusFile, line))
                    {
                        if (line.find("VmRSS:") == 0)
                        {
                            std::string value = line.substr(6);
                            value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                            double memKb = std::stod(value);
                            SetGauge(METRIC_MEMORY_USAGE, memKb * 1024); // 转换为字节
                            break;
                        }
                    }
                }
#endif

                // 简化CPU使用率计算（实际项目中需要更精确的计算）
                static auto lastTime = std::chrono::steady_clock::now();
                static auto lastIdleTime = std::chrono::steady_clock::now();

                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();

                if (elapsed > 1000)
                { // 每秒更新一次
                    // 模拟CPU使用率（实际项目中需要读取/proc/stat或使用系统API）
                    double cpuUsage = 5.0 + (std::rand() % 20); // 5-25%的随机值
                    SetGauge(METRIC_CPU_USAGE, cpuUsage);

                    lastTime = now;
                }
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("Error collecting system metrics");
            }

            // 每秒收集一次
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        LOG_INFO("System metrics thread stopped");
    }

    // ========== TimerScope 实现 ==========

    MetricsCollector::TimerScope::TimerScope(const std::string &name,
                                             const std::map<std::string, std::string> &labels)
        : name_(name), labels_(labels), start_(std::chrono::steady_clock::now()) {}

    MetricsCollector::TimerScope::~TimerScope()
    {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
        MetricsCollector::GetInstance().RecordTimer(name_, static_cast<double>(duration), labels_);
    }

} // namespace Metrics