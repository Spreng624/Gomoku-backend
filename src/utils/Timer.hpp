#ifndef TIMER_HPP
#define TIMER_HPP

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

// 定时器任务结构
struct TimerTask
{
    TimerID id;
    std::function<void()> callback;
    std::chrono::steady_clock::time_point execute_time;
    std::chrono::milliseconds interval; // 0表示不重复
    bool cancelled = false;

    // 用于优先级队列的比较
    bool operator>(const TimerTask &other) const
    {
        return execute_time > other.execute_time;
    }
};

class Timer
{
public:
    using Task = std::function<void()>;

private:
    // 使用shared_ptr管理任务，便于取消
    struct TaskNode
    {
        TimerTask task;
        std::shared_ptr<bool> cancelled_flag;

        TaskNode(TimerTask t) : task(std::move(t)), cancelled_flag(std::make_shared<bool>(false)) {}

        bool operator>(const TaskNode &other) const
        {
            return task > other.task;
        }
    };

    std::priority_queue<TaskNode, std::vector<TaskNode>, std::greater<TaskNode>> task_queue_;
    std::unordered_map<TimerID, std::shared_ptr<bool>> cancellation_flags_;
    mutable std::mutex queue_mutex_; // 添加mutable以便在const成员函数中使用
    std::condition_variable cv_;
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};
    std::atomic<TimerID> next_id_{1};

    // 工作线程函数
    void WorkerThread()
    {
        while (!stopped_)
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            if (task_queue_.empty())
            {
                // 队列为空，等待新任务
                cv_.wait(lock);
                continue;
            }

            auto next_task = task_queue_.top();
            auto now = std::chrono::steady_clock::now();

            if (next_task.task.execute_time > now)
            {
                // 等待直到任务执行时间
                auto wait_time = next_task.task.execute_time - now;
                cv_.wait_for(lock, wait_time);
                continue;
            }

            // 到达执行时间，取出任务
            task_queue_.pop();

            // 从映射中移除
            cancellation_flags_.erase(next_task.task.id);

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
                if (next_task.task.callback)
                {
                    next_task.task.callback();
                }
            }
            catch (...)
            {
                // 忽略任务执行中的异常
            }

            // 如果是重复任务，重新添加到队列
            if (next_task.task.interval.count() > 0)
            {
                lock.lock();
                TimerTask new_task = next_task.task;
                new_task.execute_time = std::chrono::steady_clock::now() + new_task.interval;

                TaskNode new_node(new_task);
                new_node.cancelled_flag = next_task.cancelled_flag; // 保持相同的取消标志

                task_queue_.push(new_node);
                cancellation_flags_[new_task.id] = new_node.cancelled_flag;
                lock.unlock();
                cv_.notify_one();
            }
        }
    }

public:
    Timer() = default;

    ~Timer()
    {
        Stop();
    }

    // 启动定时器
    void Start()
    {
        if (running_.exchange(true))
        {
            return; // 已经在运行
        }

        stopped_ = false;
        worker_thread_ = std::thread(&Timer::WorkerThread, this);
    }

    // 停止定时器
    void Stop()
    {
        if (!running_)
        {
            return;
        }

        stopped_ = true;
        cv_.notify_all();

        if (worker_thread_.joinable())
        {
            worker_thread_.join();
        }

        running_ = false;

        // 清空队列
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!task_queue_.empty())
        {
            task_queue_.pop();
        }
        cancellation_flags_.clear();
    }

    // 添加一次性定时任务
    TimerID AddTask(std::chrono::milliseconds delay, Task task)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        TimerID id = next_id_++;
        TimerTask timer_task;
        timer_task.id = id;
        timer_task.callback = std::move(task);
        timer_task.execute_time = std::chrono::steady_clock::now() + delay;
        timer_task.interval = std::chrono::milliseconds(0);
        timer_task.cancelled = false;

        TaskNode node(timer_task);
        task_queue_.push(node);
        cancellation_flags_[id] = node.cancelled_flag;

        cv_.notify_one();
        return id;
    }

    // 添加重复定时任务
    TimerID AddRepeatedTask(std::chrono::milliseconds interval, Task task, bool immediate = false)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        TimerID id = next_id_++;
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
        timer_task.cancelled = false;

        TaskNode node(timer_task);
        task_queue_.push(node);
        cancellation_flags_[id] = node.cancelled_flag;

        cv_.notify_one();
        return id;
    }

    // 取消定时任务
    bool CancelTask(TimerID id)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        auto it = cancellation_flags_.find(id);
        if (it != cancellation_flags_.end())
        {
            *(it->second) = true;
            cancellation_flags_.erase(it);
            return true;
        }

        return false;
    }

    // 获取待执行任务数量
    size_t GetPendingTaskCount() const
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return task_queue_.size();
    }

    // 检查定时器是否正在运行
    bool IsRunning() const
    {
        return running_;
    }
};

// 单例计时器管理器
class TimerManager
{
private:
    static TimerManager *instance_;
    static std::mutex instance_mutex_;

    Timer global_timer_;
    bool initialized_ = false;

    TimerManager() = default;
    ~TimerManager()
    {
        if (initialized_)
        {
            global_timer_.Stop();
        }
    }

    // 禁止拷贝
    TimerManager(const TimerManager &) = delete;
    TimerManager &operator=(const TimerManager &) = delete;

public:
    // 获取单例实例
    static TimerManager &GetInstance()
    {
        if (instance_ == nullptr)
        {
            std::lock_guard<std::mutex> lock(instance_mutex_);
            if (instance_ == nullptr)
            {
                instance_ = new TimerManager();
            }
        }
        return *instance_;
    }

    // 初始化全局定时器
    void Initialize()
    {
        if (!initialized_)
        {
            global_timer_.Start();
            initialized_ = true;
        }
    }

    // 关闭全局定时器
    void Shutdown()
    {
        if (initialized_)
        {
            global_timer_.Stop();
            initialized_ = false;
        }
    }

    // 获取全局定时器引用
    Timer &GetGlobalTimer()
    {
        return global_timer_;
    }

    // 便捷方法：添加一次性任务
    static TimerID AddTask(std::chrono::milliseconds delay, Timer::Task task)
    {
        auto &instance = GetInstance();
        if (!instance.initialized_)
        {
            instance.Initialize();
        }
        return instance.global_timer_.AddTask(delay, std::move(task));
    }

    // 便捷方法：添加重复任务
    static TimerID AddRepeatedTask(std::chrono::milliseconds interval, Timer::Task task, bool immediate = false)
    {
        auto &instance = GetInstance();
        if (!instance.initialized_)
        {
            instance.Initialize();
        }
        return instance.global_timer_.AddRepeatedTask(interval, std::move(task), immediate);
    }

    // 便捷方法：取消任务
    static bool CancelTask(TimerID id)
    {
        auto &instance = GetInstance();
        return instance.global_timer_.CancelTask(id);
    }

    // 释放单例实例（用于程序退出）
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
};

// 静态成员初始化
TimerManager *TimerManager::instance_ = nullptr;
std::mutex TimerManager::instance_mutex_;

#endif // TIMER_HPP