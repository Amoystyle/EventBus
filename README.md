# EventBus

EventBus 是一个 C++17 单头文件同步事件总线，提供线程安全的订阅表、同步发布、运行期参数匹配、常见字符串转换、取消订阅等待、关闭收口和可注入诊断日志。

实现位于 `eventbus.hpp`，命名空间为 `eventbus`。只依赖 C++17 标准库。

## 特性

- 单头文件：直接包含 `eventbus.hpp` 即可使用。
- 同步分发：`publish()` 在调用线程中执行回调，返回前完成本次分发。
- 线程安全订阅表：支持多线程并发 `subscribe()`、`publish()`、`unsubscribe()`、查询和 `clear()`。
- 无回调级执行锁：同一个回调可能被多个发布线程并发调用，回调内部共享状态由业务方同步。
- 生命周期收口：`unsubscribe()`、`unsubscribe_all()`、`clear()`、`close()` 会阻止新调用进入目标回调，并等待已进入执行的回调退出。
- 类型匹配：订阅时推导回调签名，发布时按参数 tuple 做运行期匹配。
- 字符串转换：支持 `const char*` / `char*` 到 `std::string`、`std::string_view`，以及 `std::string` 到 `std::string_view`。
- 异常隔离：回调异常不会穿透 `publish()`，会计入 `PublishResult::failed`。
- 日志可注入：默认不写 `std::cout` / `std::cerr`，需要诊断时通过 `LogHandler` 注入。

## 快速开始

```cpp
#include "eventbus.hpp"

#include <iostream>
#include <string>

int main()
{
    eventbus::EventBus bus;

    const auto id = bus.subscribe("greet", [](const std::string& name) {
        std::cout << "Hello, " << name << "\n";
    });

    const auto result = bus.publish("greet", "World");
    std::cout << "invoked: " << result.invoked << "\n";

    (void)bus.unsubscribe("greet", id);
}
```

## 接口说明

### 类型

```cpp
namespace eventbus {

using callback_id = std::size_t;

enum class LogLevel
{
    Debug,
    Warning,
    Error
};

using LogHandler = std::function<void(LogLevel, const std::string&)>;

} // namespace eventbus
```

`callback_id` 由 `subscribe()` 返回，用于取消指定订阅。`LogHandler` 是可选诊断回调，默认不设置。

### 构造和关闭

```cpp
explicit EventBus(bool verbose_logging = false);
EventBus(bool verbose_logging, LogHandler log_handler);
~EventBus() noexcept;

void close();
void clear();
```

- `verbose_logging` 只控制是否生成诊断消息；没有 `LogHandler` 时不会输出。
- `close()` 会进入关闭状态，清空订阅表，并等待已进入执行的回调退出。
- `~EventBus()` 会调用 `close()`，析构函数不抛异常。
- `clear()` 只清空当前订阅，不进入关闭状态；之后仍可继续 `subscribe()`。
- `EventBus` 不可拷贝、不可移动。

关闭后的行为：

- `subscribe()` 返回 `0`。
- `publish()` 返回空的 `PublishResult`。
- `publish_if_min_subscribers()` 返回 `false`。

### 订阅和取消订阅

```cpp
template <typename Callback>
callback_id subscribe(const std::string& eventName, Callback&& callback);

[[nodiscard]] bool unsubscribe(const std::string& eventName, callback_id id);
[[nodiscard]] std::size_t unsubscribe_all(const std::string& eventName);
```

- 回调必须返回 `void`。
- 禁止非 `const` 左值引用参数，例如 `int&`。
- `unsubscribe()` 找到并移除目标订阅时返回 `true`。
- `unsubscribe_all()` 返回移除数量。
- 回调内部取消自身订阅是允许的，不会等待自己退出。

### 发布

```cpp
template <typename... Args>
PublishResult publish(const std::string& eventName, Args&&... args);

template <typename... Args>
[[nodiscard]] bool publish_if_min_subscribers(
    const std::string& eventName,
    std::size_t min_subscribers,
    Args&&... args);
```

`publish()` 先取得订阅快照，然后释放订阅表锁，再逐个同步调用快照里的回调。发布过程中新增的订阅不会进入本次快照；发布过程中取消的订阅可能在本次快照里显示为 `skipped`。

`publish_if_min_subscribers()` 只有在当前订阅数量不少于阈值时才发布，返回值表示是否执行了发布流程。

### 发布结果

```cpp
struct PublishResult
{
    std::size_t subscribers;
    std::size_t invoked;
    std::size_t failed;
    std::size_t type_mismatches;
    std::size_t skipped;
};
```

- `subscribers`：本次发布快照中的订阅数量。
- `invoked`：成功调用的回调数量。
- `failed`：回调抛出异常的数量。
- `type_mismatches`：参数不匹配而未调用的数量。
- `skipped`：快照中存在，但发布前已经被取消激活的数量。

### 查询和统计

```cpp
[[nodiscard]] bool isEventRegistered(const std::string& eventName) const;
[[nodiscard]] std::size_t getCallbackCount(const std::string& eventName) const;
[[nodiscard]] std::vector<std::string> getAllEventNames() const;

struct EventBusStats
{
    std::size_t total_events;
    std::size_t total_callbacks;
    std::size_t max_callbacks_per_event;
    std::string most_subscribed_event;
};

[[nodiscard]] EventBusStats getStats() const;
```

查询接口只观察当前订阅表状态，不等待正在执行的回调。

### 日志

```cpp
void setVerboseLogging(bool verbose);
void setLogHandler(LogHandler handler);
```

默认情况下 EventBus 不向标准输出或标准错误写任何内容。设置 `LogHandler` 后：

- `LogLevel::Debug`：订阅、发布、类型不匹配、发布结果等 verbose 诊断。
- `LogLevel::Warning`：发布到没有订阅者的事件，仅在 verbose 开启时产生。
- `LogLevel::Error`：回调抛出异常。

示例：

```cpp
eventbus::EventBus bus(true, [](eventbus::LogLevel level, const std::string& message) {
    if (level == eventbus::LogLevel::Error) {
        // route to application logger
        (void)message;
    }
});
```

## 使用示例

### 多参数事件

```cpp
eventbus::EventBus bus;

bus.subscribe("user_action",
    [](const std::string& user, const std::string& action, int priority) {
        std::cout << user << " " << action << " " << priority << "\n";
    });

auto result = bus.publish("user_action", "Alice", "login", 5);
if (result.invoked != 1) {
    std::cerr << "dispatch issue\n";
}
```

### 字符串转换

```cpp
eventbus::EventBus bus;

bus.subscribe("name", [](const std::string& value) {
    std::cout << value << "\n";
});
bus.publish("name", "Alice"); // const char* -> std::string

bus.subscribe("view", [](std::string_view value) {
    std::cout << value << "\n";
});

std::string text = "payload";
bus.publish("view", text);   // std::string -> std::string_view
bus.publish("view", "text"); // const char* -> std::string_view
```

### 成员函数回调

成员函数建议用 lambda 显式绑定对象生命周期。

```cpp
class Receiver
{
public:
    void on_value(int value)
    {
        std::cout << value << "\n";
    }
};

Receiver receiver;
eventbus::EventBus bus;

const auto id = bus.subscribe("value", [&receiver](int value) {
    receiver.on_value(value);
});

bus.publish("value", 42);
(void)bus.unsubscribe("value", id);
```

如果回调可能晚于对象销毁，不要捕获裸引用或裸 `this`。用 `std::weak_ptr` 验活：

```cpp
auto receiver = std::make_shared<Receiver>();
std::weak_ptr<Receiver> weak_receiver = receiver;

bus.subscribe("value", [weak_receiver](int value) {
    if (auto locked = weak_receiver.lock()) {
        locked->on_value(value);
    }
});
```

### 有状态回调

EventBus 不为回调加执行锁。业务状态必须由业务方保护。

```cpp
eventbus::EventBus bus;

std::mutex total_mutex;
int total = 0;

bus.subscribe("count", [&total_mutex, &total](int value) {
    std::lock_guard<std::mutex> lock(total_mutex);
    total += value;
});
```

如果回调需要继续发布事件，先释放业务锁，再调用 `publish()`：

```cpp
bus.subscribe("input", [&bus, &total_mutex, &total](int value) {
    bool should_notify = false;
    int new_total = 0;

    {
        std::lock_guard<std::mutex> lock(total_mutex);
        total += value;
        new_total = total;
        should_notify = total > 100;
    }

    if (should_notify) {
        bus.publish("threshold", new_total);
    }
});
```

### 大对象载荷

发布参数会被打包到内部 tuple 中，存在复制或移动成本。大对象建议传递明确所有权或生命周期的轻量句柄。

```cpp
auto data = std::make_shared<const std::vector<int>>(std::vector<int>{1, 2, 3});

bus.subscribe("data", [](std::shared_ptr<const std::vector<int>> payload) {
    std::cout << payload->size() << "\n";
});

bus.publish("data", data);
```

### 条件发布

```cpp
if (!bus.publish_if_min_subscribers("important", 2, "payload")) {
    // not enough subscribers
}
```

### 关闭流程

```cpp
eventbus::EventBus bus;

bus.subscribe("stop", [] {
    // cleanup
});

bus.publish("stop");
bus.close();
```

关闭后不再复用该 `EventBus` 实例。需要重新开始时创建新对象。

## 多线程安全

### EventBus 自身保证

- 订阅表的并发访问受内部锁保护。
- `publish()` 不在持有订阅表锁时执行用户回调。
- `unsubscribe()`、`unsubscribe_all()`、`clear()` 和 `close()` 会等待已进入执行的目标回调完成。
- 回调取消自身订阅不会自死锁。

### 业务方必须保证

- 同一个回调可能被多个线程并发调用；回调内部共享状态必须自行同步。
- 不要在持有业务锁时调用外部未知代码。
- 不要在持有业务锁时调用 `publish()`、`unsubscribe()`、`clear()` 或 `close()`。
- 如果回调捕获对象引用，必须保证对象活得比订阅更久，或者使用 `weak_ptr` 验活。
- `close()` 不能解决“其他线程仍持有已经析构的 EventBus 引用”的问题；对象所有权和线程 join 仍由业务方管理。

## 注意事项

- 这是同步事件总线，不提供异步队列、线程池、背压、取消令牌或跨线程投递语义。
- 当前 API 不承诺跨 DLL 稳定 ABI。不要把 `EventBus` 当作跨模块二进制接口暴露。
- 事件名接口使用 `const std::string&`。C++17 的 `std::unordered_map` 没有标准异构查找，当前未提供 `std::string_view` 事件名接口。
- 发布参数会进入 `std::any` 持有的 tuple，热路径存在类型擦除与参数复制成本。
- 非 `const` 左值引用回调参数被禁止，避免调用方误以为能修改发布方原始对象。
- 回调异常会被捕获并计入 `failed`，不会中断后续回调。
- `callback_id == 0` 表示订阅失败，当前主要出现在 `EventBus` 已关闭后。

## 构建

要求：

- C++17 或更高
- 标准库支持 `std::shared_mutex`、`std::any`、`std::string_view`

CMake：

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

单文件编译示例：

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -I. simple_test.cpp -o simple_test
```

Windows 可直接运行：

```bat
build.bat
demo.bat
```

## 测试目标

CMake 当前包含：

- `simple_test`：基础功能、类型转换、并发回调、取消订阅等待、异常结果。
- `complete_test`：完整功能、统计、条件发布、线程安全示例。
- `complex_type_test`：复杂 STL 类型和自定义类型载荷。
- `usage_example`：实际使用示例。

## 文件结构

```text
.
|-- eventbus.hpp
|-- simple_test.cpp
|-- test_full.cpp
|-- test_complex_types.cpp
|-- example_simple.cpp
|-- CMakeLists.txt
|-- build.bat
|-- demo.bat
|-- README.md
|-- QUICK_START.md
|-- PROJECT_SUMMARY.md
`-- DELIVERY_NOTES.md
```
