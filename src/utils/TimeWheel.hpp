#ifndef TIMEWHEEL_HPP
#define TIMEWHEEL_HPP

#include <vector>
#include <chrono>
#include <thread>
#include <functional>

inline uint64_t GetTimeMS()
{
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

class TimeWheel
{
    using Task = std::function<void()>;

private:
    std::vector<std::vector<Task>> wheel;
    std::chrono::milliseconds interval;
    int current_slot = 0;

public:
    // 构造函数实现在类内部即为 inline
    TimeWheel(int slots, std::chrono::milliseconds interval)
        : wheel(slots), interval(interval) {}

    void AddTask(int delay_slots, Task task)
    {
        int pos = (current_slot + delay_slots) % wheel.size();
        wheel[pos].push_back(std::move(task)); // 使用 move 稍微优化
    }

    void Run()
    {
        std::thread([this]()
                    {
            while (true) {
                std::this_thread::sleep_for(interval);
                
                auto& tasks = wheel[current_slot];
                for (auto& task : tasks) {
                    if (task) task();
                }
                tasks.clear();

                current_slot = (current_slot + 1) % wheel.size();
            } })
            .detach();
    }
};

#endif