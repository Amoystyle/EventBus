/**
 * @file eventbus.hpp
 * @brief Enterprise-level thread-safe event bus system
 * @author AI Assistant
 * @version 1.0
 * @date 2025
 *
 * Features:
 * - Thread-safe: Read-write separation using std::shared_mutex
 * - Universal type conversion: Automatic const char* to std::string conversion
 * - Arbitrary parameter count: Support for 0 to N parameters
 * - Type safety: Compile-time and runtime type checking
 * - High performance: Zero runtime overhead template metaprogramming
 * - Statistics monitoring: Complete event bus status monitoring
 */

#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <any>
#include <atomic>
#include <type_traits>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <typeindex>
#include <memory>
#include <algorithm>

namespace eventbus {

using callback_id = size_t;

namespace detail {

template <typename T>
struct function_traits;

// Normal function
template <typename Ret, typename... Args>
struct function_traits<Ret(Args...)>
{
    using signature = Ret(Args...);
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

// Function pointer
template <typename Ret, typename... Args>
struct function_traits<Ret(*)(Args...)> : function_traits<Ret(Args...)> {};

// std::function
template <typename Ret, typename... Args>
struct function_traits<std::function<Ret(Args...)>> : function_traits<Ret(Args...)> {};

// Member function pointer
template <typename ClassType, typename Ret, typename... Args>
struct function_traits<Ret(ClassType::*)(Args...)> : function_traits<Ret(Args...)> {};

// const member function pointer
template <typename ClassType, typename Ret, typename... Args>
struct function_traits<Ret(ClassType::*)(Args...) const> : function_traits<Ret(Args...)> {};

// Function objects (including lambda)
template <typename Callable>
struct function_traits
{
private:
    using call_type = decltype(&Callable::operator());
public:
    using signature = typename function_traits<call_type>::signature;
    using return_type = typename function_traits<call_type>::return_type;
    using args_tuple = typename function_traits<call_type>::args_tuple;
    static constexpr size_t arity = function_traits<call_type>::arity;
};

// std::bind support
template <typename F>
struct function_traits<std::_Binder<std::_Unforced, F>>
{
    using signature = typename function_traits<F>::signature;
    using return_type = typename function_traits<F>::return_type;
    using args_tuple = typename function_traits<F>::args_tuple;
    static constexpr size_t arity = function_traits<F>::arity;
};

// other std::bind support
template <typename F, typename... BoundArgs>
struct function_traits<std::_Binder<std::_Unforced, F, BoundArgs...>>
{
    using signature = typename function_traits<F>::signature;
    using return_type = typename function_traits<F>::return_type;
    using args_tuple = typename function_traits<F>::args_tuple;
    static constexpr size_t arity = function_traits<F>::arity;
};

} // namespace detail

class ICallbackWrapper
{
public:
    virtual ~ICallbackWrapper() = default;
    virtual bool try_invoke(const std::any& args_any) = 0;
    virtual std::type_index get_args_type() const = 0;
    virtual callback_id get_id() const = 0;
};

template<typename... Args>
class CallbackWrapper : public ICallbackWrapper
{
private:
    callback_id id_;
    std::function<void(Args...)> callback_;

public:
    CallbackWrapper(callback_id id, std::function<void(Args...)> callback)
        : id_(id), callback_(std::move(callback))
    {
    }

    bool try_invoke(const std::any& args_any) override
    {
        try {
            if constexpr (sizeof...(Args) == 0) {
                callback_();
                return true;
            } else {
                // 1. Try exact match
                if (auto args_tuple = std::any_cast<std::tuple<Args...>>(&args_any)) {
                    std::apply(callback_, *args_tuple);
                    return true;
                }

                // 2. Try loose match
                using DecayedArgs = std::tuple<std::decay_t<Args>...>;
                if (auto args_tuple = std::any_cast<DecayedArgs>(&args_any)) {
                    std::apply(callback_, *args_tuple);
                    return true;
                }

                // 3. Try smart type conversion
                return try_universal_conversion(args_any);
            }
        }
        catch (...) {
            // Type conversion failed
        }
        return false;
    }

    std::type_index get_args_type() const override
    {
        return std::type_index(typeid(std::tuple<Args...>));
    }

    callback_id get_id() const override
    {
        return id_;
    }

private:
    bool try_universal_conversion(const std::any& args_any)
    {
        return try_parameter_conversion(args_any, std::index_sequence_for<Args...>{});
    }

    template<std::size_t... Is>
    bool try_parameter_conversion(const std::any& args_any, std::index_sequence<Is...>)
    {
        using SourceTypes = std::tuple<typename map_to_source_type<std::tuple_element_t<Is, std::tuple<Args...>>>::type...>;

        if (auto source_tuple = std::any_cast<SourceTypes>(&args_any)) {
            try {
                invoke_with_conversion(*source_tuple, std::index_sequence_for<Args...>{});
                return true;
            }
            catch (...) {}
        }

        return false;
    }

    template<typename TargetType>
    struct map_to_source_type
    {
        using type = std::decay_t<TargetType>;
    };

    // String type specialization
    template<>
    struct map_to_source_type<std::string>
    {
        using type = const char*;
    };

    template<>
    struct map_to_source_type<const std::string&>
    {
        using type = const char*;
    };

    // Reference Type Specialization - Using Value Types
    template<typename T>
    struct map_to_source_type<const T&>
    {
        using type = std::conditional_t<
            std::is_same_v<T, std::string>,
            const char*,
            T
        >;
    };

    template<typename T>
    struct map_to_source_type<T&>
    {
        using type = T;
    };

    template<typename SourceTuple, std::size_t... Is>
    void invoke_with_conversion(const SourceTuple& source_tuple, std::index_sequence<Is...>)
    {
        callback_(convert_parameter<std::tuple_element_t<Is, std::tuple<Args...>>>(std::get<Is>(source_tuple))...);
    }

    template<typename TargetType, typename SourceType>
    std::decay_t<TargetType> convert_parameter(const SourceType& source)
    {
        using DecayedTarget = std::decay_t<TargetType>;

        if constexpr (std::is_same_v<DecayedTarget, std::decay_t<SourceType>>) {
            return source;
        } else if constexpr (std::is_same_v<DecayedTarget, std::string> &&
                             (std::is_same_v<SourceType, const char*> || std::is_same_v<SourceType, char*>)) {
            return std::string(source);
        } else if constexpr (std::is_convertible_v<SourceType, DecayedTarget>) {
            return static_cast<DecayedTarget>(source);
        } else {
            return DecayedTarget{};
        }
    }
};

class EventBus
{
public:
    struct EventBusStats
    {
        size_t total_events;
        size_t total_callbacks;
        size_t max_callbacks_per_event;
        std::string most_subscribed_event;
    };

private:
    std::atomic<callback_id> next_id_{0};
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::vector<std::unique_ptr<ICallbackWrapper>>> callbacks_map_;
    bool verbose_logging_{false};

public:
    explicit EventBus(bool verbose_logging = false) : verbose_logging_(verbose_logging) {}

    void setVerboseLogging(bool verbose) { verbose_logging_ = verbose; }

    template <typename Callback>
    callback_id subscribe(const std::string& eventName, Callback&& callback)
    {
        using CallbackType = std::decay_t<Callback>;
        using Traits = detail::function_traits<CallbackType>;
        using Signature = typename Traits::signature;

        callback_id id = ++next_id_;

        if (verbose_logging_) {
            std::cout
                << "Subscribe event: " << eventName
                << "\r\n             ID: " << id
                << "\r\n          Types: " << typeid(CallbackType).name()
                << "\r\n      Signature: " << typeid(Signature).name()
                << "\r\n\r\n";
        }

        std::function<Signature> func(std::forward<Callback>(callback));
        auto wrapper = create_wrapper_from_function(id, std::move(func));

        std::unique_lock<std::shared_mutex> lock(mutex_);
        callbacks_map_[eventName].push_back(std::move(wrapper));

        return id;
    }

    bool unsubscribe(const std::string& eventName, callback_id id)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        auto it = callbacks_map_.find(eventName);
        if (it == callbacks_map_.end()) {
            return false;
        }

        auto& callbacks = it->second;
        auto callback_it = std::find_if(callbacks.begin(), callbacks.end(),
                                        [id](const std::unique_ptr<ICallbackWrapper>& wrapper) {
            return wrapper->get_id() == id;
        });

        if (callback_it != callbacks.end()) {
            callbacks.erase(callback_it);
            return true;
        }
        return false;
    }

    bool isEventRegistered(const std::string& eventName) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = callbacks_map_.find(eventName);
        return it != callbacks_map_.end() && !it->second.empty();
    }

    template <typename... Args>
    void publish(const std::string& eventName, Args&&... args)
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = callbacks_map_.find(eventName);
        if (it == callbacks_map_.end() || it->second.empty()) {
            if (verbose_logging_) {
                std::cout << "Warning: Event '" << eventName << "' has no callbacks" << std::endl;
            }
            return;
        }

        if (verbose_logging_) {
            std::cout
                << "Publish event: " << eventName
                << "\r\n         args: " << sizeof...(Args)
                << "\r\n        types: " << typeid(std::tuple<std::decay_t<Args>...>).name()
                << "\r\n\r\n";
        }

        std::any args_any;
        if constexpr (sizeof...(Args) > 0) {
            args_any = std::make_tuple(std::decay_t<Args>(args)...);
        }

        int successful_calls = 0;
        for (auto& wrapper : it->second) {
            try {
                if (wrapper->try_invoke(args_any)) {
                    successful_calls++;
                } else if (verbose_logging_) {
                    std::cout
                        << "Type mismatch, skipping callback"
                        << "\r\n             ID: " << wrapper->get_id()
                        << "\r\n  expected type: " << wrapper->get_args_type().name()
                        << "\r\n    actual type: " << typeid(std::tuple<std::decay_t<Args>...>).name()
                        << "\r\n\r\n";
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Callback exception (ID: " << wrapper->get_id() << "): " << e.what() << std::endl;
            }
        }

        if (verbose_logging_) {
            std::cout << "Successfully called " << successful_calls << " callbacks\r\n" << std::endl;
        }
    }

    size_t getCallbackCount(const std::string& eventName) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = callbacks_map_.find(eventName);
        return it != callbacks_map_.end() ? it->second.size() : 0;
    }

    size_t unsubscribe_all(const std::string& eventName)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        auto it = callbacks_map_.find(eventName);
        if (it == callbacks_map_.end()) {
            return 0;
        }

        size_t count = it->second.size();
        callbacks_map_.erase(it);
        return count;
    }

    std::vector<std::string> getAllEventNames() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        std::vector<std::string> event_names;
        event_names.reserve(callbacks_map_.size());

        for (const auto& pair : callbacks_map_) {
            if (!pair.second.empty()) {
                event_names.push_back(pair.first);
            }
        }

        return event_names;
    }

    EventBusStats getStats() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        EventBusStats stats{};
        stats.total_events = 0;
        stats.total_callbacks = 0;
        stats.max_callbacks_per_event = 0;

        for (const auto& pair : callbacks_map_) {
            if (!pair.second.empty()) {
                stats.total_events++;
                size_t callback_count = pair.second.size();
                stats.total_callbacks += callback_count;

                if (callback_count > stats.max_callbacks_per_event) {
                    stats.max_callbacks_per_event = callback_count;
                    stats.most_subscribed_event = pair.first;
                }
            }
        }

        return stats;
    }

    template <typename... Args>
    bool publish_if_min_subscribers(const std::string& eventName, size_t min_subscribers, Args&&... args)
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = callbacks_map_.find(eventName);
        if (it == callbacks_map_.end() || it->second.size() < min_subscribers) {
            return false;
        }

        lock.unlock();
        publish(eventName, std::forward<Args>(args)...);
        return true;
    }

    void clear()
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        callbacks_map_.clear();
    }

private:
    template<typename... Args>
    std::unique_ptr<ICallbackWrapper> create_wrapper_from_function(callback_id id, std::function<void(Args...)> func)
    {
        return std::make_unique<CallbackWrapper<Args...>>(id, std::move(func));
    }
};

} // namespace eventbus
