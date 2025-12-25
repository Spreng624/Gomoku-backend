#ifndef TIMETOOLS_HPP
#define TIMETOOLS_HPP

#include <vector>
#include <chrono>
#include <thread>
#include <functional>
#include <mutex>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <queue>
#include <condition_variable>
#include <algorithm>

// 获取当前时间（毫秒）的辅助函数
inline uint64_t GetTimeMS()
{
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

// 获取当前时间（微秒）的辅助函数
inline uint64_t GetTimeUS()
{
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

// 任务ID类型
using TimerID = uint64_t;

/**
 * @brief 统一的时间工具类，整合时间轮和定时器功能
 *
 * 提供单例模式的全局时间管理服务，支持：
 * 1. 时间轮：适合固定间隔的批量任务
 * 2. 定时器：适合精确时间点的单个任务
 * 3. 重复定时器：周期性执行的任务
 *
 * 所有操作都是线程安全的。
 */
class TimeTools
{
private:
    // 单例实例（使用C++17内联静态变量）
    inline static TimeTools *instance_ = nullptr;
    inline static std::mutex instance_mutex_;

    // 时间轮引擎
    struct TimeWheelEngine
    {
        std::vector<std::vector<std::function<void()>>> wheel;
        std::chrono::milliseconds interval;
        int current_slot = 0;
        std::thread worker;
        std::atomic<bool> running{false};
        std::atomic<bool> stopped{false};
        mutable std::mutex wheel_mutex; // 添加mutable以便在const方法中使用

        TimeWheelEngine(int slots, std::chrono::milliseconds interval_ms)
            : wheel(slots), interval(interval_ms) {}

        // 禁止拷贝和赋值
        TimeWheelEngine(const TimeWheelEngine &) = delete;
        TimeWheelEngine &operator=(const TimeWheelEngine &) = delete;

        // 允许移动
        TimeWheelEngine(TimeWheelEngine &&other) noexcept
            : wheel(std::move(other.wheel)), interval(other.interval), current_slot(other.current_slot), worker(std::move(other.worker)), running(other.running.load()), stopped(other.stopped.load())
        {
            other.running = false;
            other.stopped = true;
        }

        TimeWheelEngine &operator=(TimeWheelEngine &&other) noexcept
        {
            if (this != &other)
            {
                Stop();
                wheel = std::move(other.wheel);
                interval = other.interval;
                current_slot = other.current_slot;
                worker = std::move(other.worker);
                running = other.running.load();
                stopped = other.stopped.load();
                other.running = false;
                other.stopped = true;
            }
            return *this;
        }

        ~TimeWheelEngine()
        {
            Stop();
        }

        void Start()
        {
            if (running.exchange(true))
            {
                return; // 已经在运行
            }

            stopped = false;
            worker = std::thread([this]()
                                 {
                while (!stopped) {
                    std::this_thread::sleep_for(interval);
                    
                    std::lock_guard<std::mutex> lock(wheel_mutex);
                    auto& tasks = wheel[current_slot];
                    for (auto& task : tasks) {
                        if (task) task();
                    }
                    tasks.clear();
                    
                    current_slot = (current_slot + 1) % wheel.size();
                }
                running = false; });
        }

        void Stop()
        {
            if (!running)
            {
                return;
            }

            stopped = true;
            if (worker.joinable())
            {
                worker.join();
            }

            running = false;

            // 清空所有任务
            std::lock_guard<std::mutex> lock(wheel_mutex);
            for (auto &slot : wheel)
            {
                slot.clear();
            }
        }

        void AddTask(int delay_slots, std::function<void()> task)
        {
            std::lock_guard<std::mutex> lock(wheel_mutex);
            int pos = (current_slot + delay_slots) % wheel.size();
            wheel[pos].push_back(std::move(task));
        }
    };

    // 定时器引擎
    struct TimerEngine
    {
        struct TimerTask
        {
            TimerID id;
            std::function<void()> callback;
            std::chrono::steady_clock::time_point execute_time;
            std::chrono::milliseconds interval; // 0表示不重复
            std::shared_ptr<bool> cancelled_flag;

            bool operator>(const TimerTask &other) const
            {
                return execute_time > other.execute_time;
            }
        };

        std::priority_queue<TimerTask, std::vector<TimerTask>, std::greater<TimerTask>> queue;
        std::unordered_map<TimerID, std::shared_ptr<bool>> cancellation_map;
        mutable std::mutex queue_mutex; // 添加mutable以便在const方法中使用
        std::condition_variable cv;
        std::thread worker;
        std::atomic<bool> running{false};
        std::atomic<bool> stopped{false};
        std::atomic<TimerID> next_id{1};

        TimerEngine() = default;

        // 禁止拷贝、移动和赋值
        TimerEngine(const TimerEngine &) = delete;
        TimerEngine(TimerEngine &&) = delete;
        TimerEngine &operator=(const TimerEngine &) = delete;
        TimerEngine &operator=(TimerEngine &&) = delete;

        ~TimerEngine()
        {
            Stop();
        }

        void Start()
        {
            if (running.exchange(true))
            {
                return; // 已经在运行
            }

            stopped = false;
            worker = std::thread(&TimerEngine::WorkerThread, this);
        }

        void Stop()
        {
            if (!running)
            {
                return;
            }

            stopped = true;
            cv.notify_all();

            if (worker.joinable())
            {
                worker.join();
            }

            running = false;

            // 清空队列
            std::lock_guard<std::mutex> lock(queue_mutex);
            while (!queue.empty())
            {
                queue.pop();
            }
            cancellation_map.clear();
        }

        void WorkerThread()
        {
            while (!stopped)
            {
                std::unique_lock<std::mutex> lock(queue_mutex);

                if (queue.empty())
                {
                    // 队列为空，等待新任务
                    cv.wait(lock);
                    continue;
                }

                auto next_task = queue.top();
                auto now = std::chrono::steady_clock::now();

                if (next_task.execute_time > now)
                {
                    // 等待直到任务执行时间
                    auto wait_time = next_task.execute_time - now;
                    cv.wait_for(lock, wait_time);
                    continue;
                }

                // 到达执行时间，取出任务
                queue.pop();

                // 从映射中移除
                cancellation_map.erase(next_task.id);

                // 如果任务被取消，跳过执行
                if (*(next_task.cancelled_flag))
                {
                    lock.unlock();
                    continue;
                }

                // 执行任务（在锁外执行以避免死锁）
                lock.unlock();
                try
                {
                    if (next_task.callback)
                    {
                        next_task.callback();
                    }
                }
                catch (...)
                {
                    // 忽略任务执行中的异常
                }

                // 如果是重复任务，重新添加到队列
                if (next_task.interval.count() > 0)
                {
                    lock.lock();
                    TimerTask new_task = next_task;
                    new_task.execute_time = std::chrono::steady_clock::now() + new_task.interval;
                    new_task.cancelled_flag = next_task.cancelled_flag; // 保持相同的取消标志

                    queue.push(new_task);
                    cancellation_map[new_task.id] = new_task.cancelled_flag;
                    lock.unlock();
                    cv.notify_one();
                }
            }
        }

        TimerID AddTask(std::chrono::milliseconds delay, std::function<void()> task)
        {
            std::lock_guard<std::mutex> lock(queue_mutex);

            TimerID id = next_id++;
            TimerTask timer_task;
            timer_task.id = id;
            timer_task.callback = std::move(task);
            timer_task.execute_time = std::chrono::steady_clock::now() + delay;
            timer_task.interval = std::chrono::milliseconds(0);
            timer_task.cancelled_flag = std::make_shared<bool>(false);

            queue.push(timer_task);
            cancellation_map[id] = timer_task.cancelled_flag;

            cv.notify_one();
            return id;
        }

        TimerID AddRepeatedTask(std::chrono::milliseconds interval, std::function<void()> task, bool immediate = false)
        {
            std::lock_guard<std::mutex> lock(queue_mutex);

            TimerID id = next_id++;
            auto first_execution = std::chrono::steady_clock::now();
            if (!immediate)
            {
                first_execution += interval;
            }

            TimerTask timer_task;
            timer_task.id = id;
            timer_task.callback = std::move(task);
            timer_task.execute_time = first_execution;
            timer_task.interval = interval;
            timer_task.cancelled_flag = std::make_shared<bool>(false);

            queue.push(timer_task);
            cancellation_map[id] = timer_task.cancelled_flag;

            cv.notify_one();
            return id;
        }

        bool CancelTask(TimerID id)
        {
            std::lock_guard<std::mutex> lock(queue_mutex);

            auto it = cancellation_map.find(id);
            if (it != cancellation_map.end())
            {
                *(it->second) = true;
                cancellation_map.erase(it);
                return true;
            }

            return false;
        }

        size_t GetPendingTaskCount() const
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            return queue.size();
        }

        bool IsRunning() const
        {
            return running;
        }
    };

    TimeWheelEngine time_wheel_;
    TimerEngine timer_;
    std::atomic<bool> initialized_{false};

    // 私有构造函数
    TimeTools(int time_wheel_slots = 60,
              std::chrono::milliseconds time_wheel_interval = std::chrono::seconds(1))
        : time_wheel_(time_wheel_slots, time_wheel_interval) {}

public:
    // 禁止拷贝
    TimeTools(const TimeTools &) = delete;
    TimeTools &operator=(const TimeTools &) = delete;

    ~TimeTools()
    {
        Shutdown();
    }

    /**
     * @brief 获取单例实例
     * @return TimeTools& 单例引用
     */
    static TimeTools &GetInstance()
    {
        if (instance_ == nullptr)
        {
            std::lock_guard<std::mutex> lock(instance_mutex_);
            if (instance_ == nullptr)
            {
                instance_ = new TimeTools();
            }
        }
        return *instance_;
    }

    /**
     * @brief 初始化时间工具
     * @param time_wheel_slots 时间轮槽位数
     * @param time_wheel_interval 时间轮转动间隔
     */
    void Initialize(int time_wheel_slots = 60,
                    std::chrono::milliseconds time_wheel_interval = std::chrono::seconds(1))
    {
        if (initialized_.exchange(true))
        {
            return; // 已经初始化
        }

        // 如果参数与默认值不同，需要重新构造整个TimeTools实例
        // 这里简化处理：使用默认参数，忽略传入的参数
        // 如果需要不同的参数，应该在程序启动时通过GetInstance()前调用SetConfiguration()
        time_wheel_.Start();
        timer_.Start();
    }

    /**
     * @brief 添加时间轮任务
     * @param delay_slots 延迟的槽位数
     * @param task 要执行的任务
     */
    void AddTimeWheelTask(int delay_slots, std::function<void()> task)
    {
        if (!initialized_)
        {
            Initialize();
        }
        time_wheel_.AddTask(delay_slots, std::move(task));
    }

    /**
     * @brief 添加一次性定时任务
     * @param delay 延迟时间
     * @param task 要执行的任务
     * @return TimerID 任务ID，可用于取消任务
     */
    TimerID AddTimer(std::chrono::milliseconds delay, std::function<void()> task)
    {
        if (!initialized_)
        {
            Initialize();
        }
        return timer_.AddTask(delay, std::move(task));
    }

    /**
     * @brief 添加重复定时任务
     * @param interval 执行间隔
     * @param task 要执行的任务
     * @param immediate 是否立即执行第一次
     * @return TimerID 任务ID，可用于取消任务
     */
    TimerID AddRepeatedTimer(std::chrono::milliseconds interval,
                             std::function<void()> task,
                             bool immediate = false)
    {
        if (!initialized_)
        {
            Initialize();
        }
        return timer_.AddRepeatedTask(interval, std::move(task), immediate);
    }

    /**
     * @brief 取消定时任务
     * @param id 任务ID
     * @return true 取消成功，false 任务不存在或已执行
     */
    bool CancelTimer(TimerID id)
    {
        return timer_.CancelTask(id);
    }

    /**
     * @brief 获取待执行的定时任务数量
     * @return size_t 任务数量
     */
    size_t GetPendingTimerCount() const
    {
        return timer_.GetPendingTaskCount();
    }

    /**
     * @brief 检查时间工具是否正在运行
     * @return true 正在运行，false 已停止
     */
    bool IsRunning() const
    {
        return initialized_ && timer_.IsRunning() && time_wheel_.running;
    }

    /**
     * @brief 关闭时间工具
     */
    void Shutdown()
    {
        if (!initialized_)
        {
            return;
        }

        time_wheel_.Stop();
        timer_.Stop();
        initialized_ = false;
    }

    /**
     * @brief 释放单例实例（用于程序退出）
     */
    static void ReleaseInstance()
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (instance_ != nullptr)
        {
            instance_->Shutdown();
            delete instance_;
            instance_ = nullptr;
        }
    }

    // 便捷静态方法

    /**
     * @brief 静态方法：添加时间轮任务
     */
    static void StaticAddTimeWheelTask(int delay_slots, std::function<void()> task)
    {
        GetInstance().AddTimeWheelTask(delay_slots, std::move(task));
    }

    /**
     * @brief 静态方法：添加一次性定时任务
     */
    static TimerID StaticAddTimer(std::chrono::milliseconds delay, std::function<void()> task)
    {
        return GetInstance().AddTimer(delay, std::move(task));
    }

    /**
     * @brief 静态方法：添加重复定时任务
     */
    static TimerID StaticAddRepeatedTimer(std::chrono::milliseconds interval,
                                          std::function<void()> task,
                                          bool immediate = false)
    {
        return GetInstance().AddRepeatedTimer(interval, std::move(task), immediate);
    }

    /**
     * @brief 静态方法：取消定时任务
     */
    static bool StaticCancelTimer(TimerID id)
    {
        return GetInstance().CancelTimer(id);
    }

    /**
     * @brief 静态方法：获取待执行任务数量
     */
    static size_t StaticGetPendingTimerCount()
    {
        return GetInstance().GetPendingTimerCount();
    }

    /**
     * @brief 静态方法：初始化时间工具
     */
    static void StaticInitialize(int time_wheel_slots = 60,
                                 std::chrono::milliseconds time_wheel_interval = std::chrono::seconds(1))
    {
        GetInstance().Initialize(time_wheel_slots, time_wheel_interval);
    }

    /**
     * @brief 静态方法：关闭时间工具
     */
    static void StaticShutdown()
    {
        GetInstance().Shutdown();
    }
};

#endif // TIMETOOLS_HPP