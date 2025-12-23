#ifndef EVENTBUS_HPP
#define EVENTBUS_HPP

#pragma once
#include <vector>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <any>
#include <typeindex>
#include <tuple>
#include <type_traits>
#include <utility>
#include <cassert>

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

    // 游戏事件（用于服务器推送）
    PlayerJoined,      // 玩家加入房间
    PlayerLeft,        // 玩家离开房间
    OpponentMoved,     // 对手落子
    GameEnded,         // 游戏结束
    RoomStatusChanged, // 房间状态变化
    DrawRequested,     // 平局请求
    DrawAccepted,      // 平局接受
    GiveUpRequested,   // 认输
    RoomCreated        // 房间创建完成
};

template <typename T>
struct function_traits : function_traits<decltype(&T::operator())>
{
};

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const>
{
    using std_function_type = std::function<void(Args...)>;
};

// 增加对非 const operator() 的支持
template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...)>
{
    using std_function_type = std::function<void(Args...)>;
};

template <typename T>
class EventBus
{
private:
    // 内部回调包装器基类，使用类型擦除
    struct CallbackWrapperBase
    {
        virtual ~CallbackWrapperBase() = default;
        virtual void call(const std::any &args) = 0;
        virtual std::type_index get_type_index() const = 0;
        virtual bool is_expired() const = 0;
    };

    // 可变参数模板的回调包装器
    template <typename... Args>
    struct CallbackWrapper : CallbackWrapperBase
    {
        std::function<void(Args...)> callback;
        std::weak_ptr<void> wToken;

        CallbackWrapper(std::function<void(Args...)> cb, std::shared_ptr<void> token)
            : callback(std::move(cb)), wToken(token) {}

        void call(const std::any &args) override
        {
            if (args.type() == typeid(std::tuple<Args...>))
            {
                // 解包元组并调用回调
                std::apply(callback, std::any_cast<const std::tuple<Args...> &>(args));
            }
            // 如果类型不匹配，静默忽略
        }

        std::type_index get_type_index() const override
        {
            return typeid(std::tuple<Args...>);
        }

        bool is_expired() const override
        {
            return wToken.expired();
        }
    };

    struct Entry
    {
        std::unique_ptr<CallbackWrapperBase> wrapper;
        std::type_index argType;
    };

    std::unordered_map<T, std::vector<Entry>> subscribers;

    // 清理过期订阅者的辅助函数
    void cleanup_expired(T type)
    {
        auto it = subscribers.find(type);
        if (it == subscribers.end())
            return;

        auto &list = it->second;
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [](Entry &e)
                                  { return e.wrapper->is_expired(); }),
                   list.end());
    }

public:
    // 订阅事件，支持任意数量和类型的参数
    template <typename... Args>
    [[nodiscard]] std::shared_ptr<void> Subscribe(T type, std::function<void(Args...)> handler)
    {
        auto token = std::make_shared<char>(0);
        auto wrapper = std::make_unique<CallbackWrapper<Args...>>(std::move(handler), token);
        std::type_index argType = typeid(std::tuple<Args...>);

        subscribers[type].push_back({std::move(wrapper), argType});
        return token;
    }

    // 重载：支持无参数事件
    [[nodiscard]] std::shared_ptr<void> Subscribe(T type, std::function<void()> handler)
    {
        return Subscribe(type, std::function<void()>(std::move(handler)));
    }

    // // 重载：接受任意可调用对象（lambda、函数指针等）
    // template <typename Callable>
    // [[nodiscard]] std::shared_ptr<void> Subscribe(T type, Callable &&handler)
    // {
    //     // 使用 SFINAE 检测 Callable 的签名
    //     using traits = detail::function_traits<std::decay_t<Callable>>;
    //     return SubscribeImpl(type, std::forward<Callable>(handler), std::make_index_sequence<traits::arity>{});
    // }

    template <typename F>
    auto Subscribe(T type, F &&handler)
    {
        // 利用辅助工具类提取 Lambda 的函数签名
        using traits = function_traits<std::decay_t<F>>;
        return Subscribe(type, static_cast<typename traits::std_function_type>(std::forward<F>(handler)));
    }

    // 发布事件，支持任意数量和类型的参数
    template <typename... Args>
    void Publish(T type, Args &&...args)
    {
        auto it = subscribers.find(type);
        if (it == subscribers.end())
            return;

        auto &list = it->second;
        std::type_index expectedType = typeid(std::tuple<Args...>);
        bool hasDead = false;

        // 首先执行所有有效的回调
        for (auto &entry : list)
        {
            // 检查类型是否匹配
            if (entry.argType != expectedType)
                continue;

            if (!entry.wrapper->is_expired())
            {
                std::tuple<Args...> tupleArgs(std::forward<Args>(args)...);
                entry.wrapper->call(tupleArgs);
            }
            else
            {
                hasDead = true;
            }
        }

        // 如果有过期令牌，立即清理
        if (hasDead)
        {
            cleanup_expired(type);
        }
    }

    // 重载：发布无参数事件
    void Publish(T type)
    {
        Publish(type);
    }

    // 手动清理所有过期订阅者
    void Cleanup()
    {
        for (auto &pair : subscribers)
        {
            cleanup_expired(pair.first);
        }
    }

    // 检查是否有订阅者
    bool HasSubscribers(T type) const
    {
        auto it = subscribers.find(type);
        return it != subscribers.end() && !it->second.empty();
    }

    // 获取特定类型的订阅者数量
    size_t GetSubscriberCount(T type) const
    {
        auto it = subscribers.find(type);
        return it == subscribers.end() ? 0 : it->second.size();
    }
};

#endif // EVENTBUS_HPP
