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
 * - High performance: Minimal lock hold time during event dispatch
 * - Statistics monitoring: Complete event bus status monitoring
 *
 * Performance note:
 * - Callback execution defaults to ExecutionPolicy::Sequential, which serializes
 *   the same callback across publishing threads. Use ExecutionPolicy::Concurrent
 *   only when the callback state is internally synchronized or stateless.
 * - Event parameters are copied into the synchronous dispatch payload. For large
 *   payloads, prefer std::shared_ptr<T>, std::shared_ptr<const T>, or const T*
 *   when the pointee lifetime is guaranteed by the caller.
 */

#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <any>
#include <atomic>
#include <condition_variable>
#include <type_traits>
#include <string>
#include <string_view>
#include <mutex>
#include <shared_mutex>
#include <typeindex>
#include <memory>
#include <algorithm>
#include <cstddef>
#include <tuple>

namespace eventbus {

enum class ExecutionPolicy
{
    Sequential, // 默认：自动加锁，保证同一个回调同一时刻只有一个线程在执行（绝对安全）
    Concurrent   // 并发：不加锁，回调可能同时被多个线程执行（需要用户自己保证线程安全）
};

using callback_id = std::size_t;

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

template<typename TargetType>
struct map_to_source_type
{
    using type = std::decay_t<TargetType>;
};

template<>
struct map_to_source_type<std::string>
{
    using type = const char*;
};

template<>
struct map_to_source_type<std::string_view>
{
    using type = const char*;
};

template<>
struct map_to_source_type<const std::string&>
{
    using type = const char*;
};

template<>
struct map_to_source_type<const std::string_view&>
{
    using type = const char*;
};

template<typename T>
struct map_to_source_type<const T&>
{
    using type = std::conditional_t<
        std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>,
        const char*,
        T
    >;
};

template<typename TargetType>
struct alternate_map_to_source_type
{
    using DecayedTarget = std::decay_t<TargetType>;
    using type = std::conditional_t<
        std::is_same_v<DecayedTarget, std::string>,
        std::string_view,
        std::conditional_t<
            std::is_same_v<DecayedTarget, std::string_view>,
            std::string,
            DecayedTarget
        >
    >;
};

template<typename T>
struct map_to_source_type<T&>
{
    using type = T;
};

template<typename...>
struct always_false : std::false_type {};

template<typename T>
struct is_nonconst_lvalue_reference
    : std::bool_constant<
          std::is_lvalue_reference_v<T> &&
          !std::is_const_v<std::remove_reference_t<T>>> {};

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
    static_assert((!detail::is_nonconst_lvalue_reference<Args>::value && ...),
                  "EventBus callbacks must not use non-const lvalue reference parameters");

private:
    callback_id id_;
    std::function<void(Args...)> callback_;
    ExecutionPolicy policy_;
    std::recursive_mutex invoke_mutex_;

public:
    CallbackWrapper(callback_id id, std::function<void(Args...)> callback, ExecutionPolicy policy)
        : id_(id), callback_(std::move(callback)), policy_(policy)
    {
    }

    bool try_invoke(const std::any& args_any) override
    {
        std::unique_lock<std::recursive_mutex> invoke_lock(invoke_mutex_, std::defer_lock);
        if (policy_ == ExecutionPolicy::Sequential) {
            invoke_lock.lock();
        }

        if constexpr (sizeof...(Args) == 0) {
            if (args_any.has_value()) {
                return false;
            }
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
        using SourceTypes = std::tuple<typename detail::map_to_source_type<std::tuple_element_t<Is, std::tuple<Args...>>>::type...>;

        if (auto source_tuple = std::any_cast<SourceTypes>(&args_any)) {
            if (!can_convert_tuple(*source_tuple, std::index_sequence_for<Args...>{})) {
                return false;
            }

            invoke_with_conversion(*source_tuple, std::index_sequence_for<Args...>{});
            return true;
        }

        using AlternateSourceTypes = std::tuple<typename detail::alternate_map_to_source_type<std::tuple_element_t<Is, std::tuple<Args...>>>::type...>;
        if constexpr (!std::is_same_v<SourceTypes, AlternateSourceTypes>) {
            if (auto source_tuple = std::any_cast<AlternateSourceTypes>(&args_any)) {
                if (!can_convert_tuple(*source_tuple, std::index_sequence_for<Args...>{})) {
                    return false;
                }

                invoke_with_conversion(*source_tuple, std::index_sequence_for<Args...>{});
                return true;
            }
        }

        return false;
    }

    template<typename SourceTuple, std::size_t... Is>
    bool can_convert_tuple(const SourceTuple& source_tuple, std::index_sequence<Is...>) const
    {
        return (... && can_convert_parameter<std::tuple_element_t<Is, std::tuple<Args...>>>(std::get<Is>(source_tuple)));
    }

    template<typename TargetType, typename SourceType>
    bool can_convert_parameter(const SourceType& source) const
    {
        using DecayedTarget = std::decay_t<TargetType>;
        using DecayedSource = std::decay_t<SourceType>;

        if constexpr (std::is_same_v<DecayedTarget, std::string> &&
                      (std::is_same_v<DecayedSource, const char*> || std::is_same_v<DecayedSource, char*>)) {
            return source != nullptr;
        } else if constexpr (std::is_same_v<DecayedTarget, std::string_view> &&
                             (std::is_same_v<DecayedSource, const char*> || std::is_same_v<DecayedSource, char*>)) {
            return source != nullptr;
        } else {
            return true;
        }
    }

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
                             (std::is_same_v<std::decay_t<SourceType>, const char*> ||
                              std::is_same_v<std::decay_t<SourceType>, char*>)) {
            return std::string(source);
        } else if constexpr (std::is_same_v<DecayedTarget, std::string> &&
                             std::is_same_v<std::decay_t<SourceType>, std::string_view>) {
            return std::string(source);
        } else if constexpr (std::is_same_v<DecayedTarget, std::string_view> &&
                             (std::is_same_v<std::decay_t<SourceType>, const char*> ||
                              std::is_same_v<std::decay_t<SourceType>, char*>)) {
            return std::string_view(source);
        } else if constexpr (std::is_same_v<DecayedTarget, std::string_view> &&
                             std::is_same_v<std::decay_t<SourceType>, std::string>) {
            return std::string_view(source);
        } else if constexpr (std::is_convertible_v<SourceType, DecayedTarget>) {
            return static_cast<DecayedTarget>(source);
        } else {
            static_assert(detail::always_false<TargetType, SourceType>::value,
                          "Unsupported event parameter conversion");
        }
    }
};

class EventBus
{
public:
    struct EventBusStats
    {
        std::size_t total_events;
        std::size_t total_callbacks;
        std::size_t max_callbacks_per_event;
        std::string most_subscribed_event;
    };

    struct PublishResult
    {
        std::size_t subscribers;
        std::size_t invoked;
        std::size_t failed;
        std::size_t type_mismatches;
        std::size_t skipped;
    };

private:
    using CallbackPtr = std::shared_ptr<ICallbackWrapper>;

    struct CallbackEntry
    {
        explicit CallbackEntry(CallbackPtr callback_wrapper)
            : callback(std::move(callback_wrapper))
        {
        }

        CallbackPtr callback;
        bool active{true};
        std::size_t in_flight{0};
        mutable std::mutex state_mutex;
        std::condition_variable idle_cv;
    };

    using CallbackEntryPtr = std::shared_ptr<CallbackEntry>;
    using CallbackList = std::vector<CallbackEntryPtr>;

    enum class InvokeStatus
    {
        invoked,
        type_mismatch,
        skipped
    };

    std::atomic<callback_id> next_id_{0};
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, CallbackList> callbacks_map_;
    std::atomic<bool> verbose_logging_{false};

public:
    explicit EventBus(bool verbose_logging = false) : verbose_logging_(verbose_logging) {}

    void setVerboseLogging(bool verbose) { verbose_logging_.store(verbose, std::memory_order_relaxed); }

    template <typename Callback>
    callback_id subscribe(const std::string& eventName,
                          Callback&& callback,
                          ExecutionPolicy policy = ExecutionPolicy::Sequential)
    {
        using CallbackType = std::decay_t<Callback>;
        using Traits = detail::function_traits<CallbackType>;
        using Signature = typename Traits::signature;
        static_assert(std::is_void_v<typename Traits::return_type>,
                      "EventBus callbacks must return void");

        callback_id id = next_id_.fetch_add(1, std::memory_order_relaxed) + 1;
        const bool verbose = verbose_logging_.load(std::memory_order_relaxed);

        if (verbose) {
            std::cout
                << "Subscribe event: " << eventName
                << "\r\n             ID: " << id
                << "\r\n          Types: " << typeid(CallbackType).name()
                << "\r\n      Signature: " << typeid(Signature).name()
                << "\r\n         Policy: " << (policy == ExecutionPolicy::Sequential ? "Sequential" : "Concurrent")
                << "\r\n\r\n";
        }

        std::function<Signature> func(std::forward<Callback>(callback));
        auto entry = std::make_shared<CallbackEntry>(create_wrapper_from_function(id, std::move(func), policy));

        std::unique_lock<std::shared_mutex> lock(mutex_);
        callbacks_map_[eventName].push_back(std::move(entry));

        return id;
    }

    [[nodiscard]] bool unsubscribe(const std::string& eventName, callback_id id)
    {
        CallbackEntryPtr removed_entry;

        {
            std::unique_lock<std::shared_mutex> lock(mutex_);

            auto it = callbacks_map_.find(eventName);
            if (it == callbacks_map_.end()) {
                return false;
            }

            auto& callbacks = it->second;
            auto callback_it = std::find_if(callbacks.begin(), callbacks.end(),
                                            [id](const CallbackEntryPtr& entry) {
                return entry->callback->get_id() == id;
            });

            if (callback_it == callbacks.end()) {
                return false;
            }

            removed_entry = *callback_it;
            deactivate_entry(*removed_entry);
            callbacks.erase(callback_it);
            if (callbacks.empty()) {
                callbacks_map_.erase(it);
            }
        }

        wait_for_idle(*removed_entry);
        return true;
    }

    [[nodiscard]] bool isEventRegistered(const std::string& eventName) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = callbacks_map_.find(eventName);
        return it != callbacks_map_.end() && !it->second.empty();
    }

    template <typename... Args>
    PublishResult publish(const std::string& eventName, Args&&... args)
    {
        const bool verbose = verbose_logging_.load(std::memory_order_relaxed);
        CallbackList callbacks = snapshot_callbacks(eventName);

        if (callbacks.empty()) {
            if (verbose) {
                std::cout << "Warning: Event '" << eventName << "' has no callbacks\n";
            }
            return {};
        }

        return publish_to_callbacks(eventName, callbacks, verbose, std::forward<Args>(args)...);
    }

    [[nodiscard]] std::size_t getCallbackCount(const std::string& eventName) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = callbacks_map_.find(eventName);
        return it != callbacks_map_.end() ? it->second.size() : 0;
    }

    [[nodiscard]] std::size_t unsubscribe_all(const std::string& eventName)
    {
        CallbackList removed_entries;
        std::size_t count = 0;

        {
            std::unique_lock<std::shared_mutex> lock(mutex_);

            auto it = callbacks_map_.find(eventName);
            if (it == callbacks_map_.end()) {
                return 0;
            }

            removed_entries = it->second;
            for (const auto& entry : removed_entries) {
                deactivate_entry(*entry);
            }
            count = removed_entries.size();
            callbacks_map_.erase(it);
        }

        wait_for_idle(removed_entries);
        return count;
    }

    [[nodiscard]] std::vector<std::string> getAllEventNames() const
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

    [[nodiscard]] EventBusStats getStats() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        EventBusStats stats{};
        stats.total_events = 0;
        stats.total_callbacks = 0;
        stats.max_callbacks_per_event = 0;

        for (const auto& pair : callbacks_map_) {
            if (!pair.second.empty()) {
                stats.total_events++;
                std::size_t callback_count = pair.second.size();
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
    [[nodiscard]] bool publish_if_min_subscribers(const std::string& eventName, size_t min_subscribers, Args&&... args)
    {
        const bool verbose = verbose_logging_.load(std::memory_order_relaxed);
        CallbackList callbacks;

        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = callbacks_map_.find(eventName);
            if (it == callbacks_map_.end() || it->second.size() < min_subscribers) {
                return false;
            }
            callbacks = it->second;
        }

        (void)publish_to_callbacks(eventName, callbacks, verbose, std::forward<Args>(args)...);
        return true;
    }

    void clear()
    {
        CallbackList removed_entries;

        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            for (const auto& pair : callbacks_map_) {
                for (const auto& entry : pair.second) {
                    deactivate_entry(*entry);
                    removed_entries.push_back(entry);
                }
            }
            callbacks_map_.clear();
        }

        wait_for_idle(removed_entries);
    }

private:
    static std::vector<const CallbackEntry*>& current_invocations()
    {
        thread_local std::vector<const CallbackEntry*> invocations;
        return invocations;
    }

    class InvocationGuard
    {
    public:
        explicit InvocationGuard(CallbackEntry& entry)
            : entry_(entry)
        {
            current_invocations().push_back(&entry_);
        }

        InvocationGuard(const InvocationGuard&) = delete;
        InvocationGuard& operator=(const InvocationGuard&) = delete;

        ~InvocationGuard()
        {
            current_invocations().pop_back();
            {
                std::lock_guard<std::mutex> lock(entry_.state_mutex);
                --entry_.in_flight;
            }
            entry_.idle_cv.notify_all();
        }

    private:
        CallbackEntry& entry_;
    };

    CallbackList snapshot_callbacks(const std::string& eventName) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = callbacks_map_.find(eventName);
        if (it == callbacks_map_.end()) {
            return {};
        }

        return it->second;
    }

    template <typename... Args>
    PublishResult publish_to_callbacks(const std::string& eventName, const CallbackList& callbacks, bool verbose, Args&&... args)
    {
        if (verbose) {
            std::cout
                << "Publish event: " << eventName
                << "\r\n         args: " << sizeof...(Args)
                << "\r\n        types: " << typeid(std::tuple<std::decay_t<Args>...>).name()
                << "\r\n\r\n";
        }

        std::any args_any;
        if constexpr (sizeof...(Args) > 0) {
            args_any = std::make_tuple(std::forward<Args>(args)...);
        }

        PublishResult result{};
        result.subscribers = callbacks.size();

        for (const auto& entry : callbacks) {
            try {
                const InvokeStatus status = invoke_entry(entry, args_any);
                if (status == InvokeStatus::invoked) {
                    ++result.invoked;
                } else if (status == InvokeStatus::skipped) {
                    ++result.skipped;
                } else {
                    ++result.type_mismatches;
                    if (verbose) {
                        auto& wrapper = entry->callback;
                        std::cout
                            << "Type mismatch, skipping callback"
                            << "\r\n             ID: " << wrapper->get_id()
                            << "\r\n  expected type: " << wrapper->get_args_type().name()
                            << "\r\n    actual type: " << typeid(std::tuple<std::decay_t<Args>...>).name()
                            << "\r\n\r\n";
                    }
                }
            }
            catch (const std::exception& e) {
                ++result.failed;
                std::cerr << "Callback exception (ID: " << entry->callback->get_id() << "): " << e.what() << std::endl;
            }
            catch (...) {
                ++result.failed;
                std::cerr << "Callback exception (ID: " << entry->callback->get_id() << "): unknown exception" << std::endl;
            }
        }

        if (verbose) {
            std::cout
                << "Successfully called " << result.invoked << " callbacks"
                << ", failed " << result.failed
                << ", mismatched " << result.type_mismatches
                << ", skipped " << result.skipped
                << "\r\n" << std::endl;
        }

        return result;
    }

    InvokeStatus invoke_entry(const CallbackEntryPtr& entry, const std::any& args_any)
    {
        {
            std::lock_guard<std::mutex> lock(entry->state_mutex);
            if (!entry->active) {
                return InvokeStatus::skipped;
            }
            ++entry->in_flight;
        }

        InvocationGuard invocation_guard(*entry);
        return entry->callback->try_invoke(args_any)
            ? InvokeStatus::invoked
            : InvokeStatus::type_mismatch;
    }

    void deactivate_entry(CallbackEntry& entry)
    {
        std::lock_guard<std::mutex> lock(entry.state_mutex);
        entry.active = false;
    }

    static bool is_currently_invoking(const CallbackEntry& entry)
    {
        const auto& invocations = current_invocations();
        return std::find(invocations.begin(), invocations.end(), &entry) != invocations.end();
    }

    void wait_for_idle(const CallbackList& entries)
    {
        for (const auto& entry : entries) {
            wait_for_idle(*entry);
        }
    }

    void wait_for_idle(CallbackEntry& entry)
    {
        if (is_currently_invoking(entry)) {
            return;
        }

        std::unique_lock<std::mutex> lock(entry.state_mutex);
        entry.idle_cv.wait(lock, [&entry]() {
            return entry.in_flight == 0;
        });
    }

    template<typename... Args>
    std::shared_ptr<ICallbackWrapper> create_wrapper_from_function(callback_id id,
                                                                   std::function<void(Args...)> func,
                                                                   ExecutionPolicy policy)
    {
        return std::make_shared<CallbackWrapper<Args...>>(id, std::move(func), policy);
    }
};

} // namespace eventbus
