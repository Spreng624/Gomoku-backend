#ifndef EVENTBUS_HPP
#define EVENTBUS_HPP

#pragma once
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <shared_mutex>
#include <tuple>
#include <algorithm> // 修复: std::remove_if 需要此头文件
#include <utility>
#include "Logger.h"

// 必须在此处定义 Event，否则其他文件引用 EventBus 时找不到 Event 类型
enum class Event
{
    // 系统事件
    CloseConn,

    // 分层传递数据
    OnFrame,    // Server  -> Session
    OnPacket,   // Session -> Any
    SendPacket, // Any     -> Session
    SendFrame,  // Session -> Server

    // 游戏世界
    PlayerOperation, // Session -> Player
    ExistPlayer,     // -> ObjectManager
    CreatePlayer,    // -> ObjectManager
    DestroyPlayer,   // -> ObjectManager
    CreateUser,      // -> ObjectManager
    CreateRoom,      // -> ObjectManager

    // 游戏事件
    PlayerJoined,      // 玩家加入房间
    PlayerLeft,        // 玩家离开房间
    PiecePlaced,       // 棋子放置
    GameStarted,       // 游戏开始
    GameEnded,         // 游戏结束
    RoomStatusChanged, // 房间状态变化
    DrawRequested,     // 平局请求
    DrawAccepted,      // 平局接受
    GiveUpRequested,   // 认输
    RoomCreated,       // 房间创建完成
    UserLoggedIn,      // 用户登录完成
    RoomListUpdated,   // 房间列表已更新
    ChatMessageRecv,   // 聊天消息接收
    RoomSync,          // 房间同步
    GameSync,          // 游戏同步
    SyncSeat           // 座位同步
};

// 辅助结构：用于提取 Lambda 或函数对象的参数类型
template <typename T>
struct function_traits : function_traits<decltype(&T::operator())>
{
};

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const>
{
    using args_tuple = std::tuple<std::decay_t<Args>...>;
};

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...)>
{
    using args_tuple = std::tuple<std::decay_t<Args>...>;
};

template <typename T>
class EventBus
{
private:
    // 抽象基类
    struct HandlerBase
    {
        virtual ~HandlerBase() = default;
        virtual bool is_expired() const = 0;
        virtual void exec(void *args_ptr, std::type_index type_idx) = 0;
    };

    // 具体类型的处理器
    template <typename... Args>
    struct Handler : HandlerBase
    {
        using CallbackFunc = std::function<void(Args...)>;
        CallbackFunc callback;
        std::weak_ptr<void> token;

        Handler(CallbackFunc cb, std::shared_ptr<void> tk)
            : callback(std::move(cb)), token(std::move(tk)) {}

        bool is_expired() const override { return token.expired(); }

        void exec(void *args_ptr, std::type_index type_idx) override
        {
            if (type_idx != typeid(std::tuple<std::decay_t<Args>...>))
            {
                return;
            }
            auto *tuple_args = static_cast<std::tuple<std::decay_t<Args>...> *>(args_ptr);
            std::apply(callback, *tuple_args);
        }
    };

    using HandlerList = std::vector<std::unique_ptr<HandlerBase>>;
    std::unordered_map<T, HandlerList> subscribers_;
    mutable std::shared_mutex mutex_;

    EventBus() = default;

public:
    static EventBus<T> &GetInstance()
    {
        static EventBus<T> instance;
        return instance;
    }

    EventBus(const EventBus &) = delete;
    EventBus &operator=(const EventBus &) = delete;

    // 订阅：自动推导参数类型
    template <typename F>
    [[nodiscard]] std::shared_ptr<void> Subscribe(T type, F &&handler)
    {
        using Traits = function_traits<std::decay_t<F>>;
        return SubscribeImpl(type, std::forward<F>(handler), typename Traits::args_tuple{});
    }

    // 订阅：无参特化
    [[nodiscard]] std::shared_ptr<void> Subscribe(T type, std::function<void()> handler)
    {
        return SubscribeImpl(type, std::move(handler), std::tuple<>{});
    }

    // 发布
    template <typename... Args>
    void Publish(T type, Args &&...args)
    {
        // 构建 Tuple (移除引用和const，确保持久化存储参数)
        std::tuple<std::decay_t<Args>...> packaged_args(std::forward<Args>(args)...);

        std::shared_lock<std::shared_mutex> lock(mutex_); // 读锁

        auto it = subscribers_.find(type);
        if (it == subscribers_.end())
            return;

        bool need_cleanup = false;
        std::type_index current_type_id = typeid(packaged_args);

        for (auto &handler : it->second)
        {
            if (!handler->is_expired())
            {
                handler->exec(&packaged_args, current_type_id);
            }
            else
            {
                need_cleanup = true;
            }
        }

        if (need_cleanup)
        {
            lock.unlock();                                          // 升级锁前必须先解锁
            std::unique_lock<std::shared_mutex> write_lock(mutex_); // 写锁
            auto &list = subscribers_[type];
            // 修复: 包含了 <algorithm> 后这里才能编译通过
            list.erase(std::remove_if(list.begin(), list.end(),
                                      [](const auto &h)
                                      { return h->is_expired(); }),
                       list.end());
        }
    }

private:
    template <typename F, typename... Args>
    std::shared_ptr<void> SubscribeImpl(T type, F &&handler, std::tuple<Args...>)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        auto token = std::make_shared<int>(1);
        auto wrapper = std::make_unique<Handler<Args...>>(
            std::function<void(Args...)>(std::forward<F>(handler)),
            token);

        subscribers_[type].push_back(std::move(wrapper));
        return token;
    }
};

#endif // EVENTBUS_HPP