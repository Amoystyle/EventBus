# EventBus

EventBus 是一个 C++17 单头文件事件总线，提供线程安全的订阅、发布、取消订阅、同步分发、运行期类型匹配、常见字符串转换和基础统计能力。

当前实现位于 `eventbus.hpp`，命名空间为 `eventbus`。

## 特性

- 线程安全：订阅表使用 `std::shared_mutex` 保护，支持并发 `subscribe`、`publish`、`unsubscribe`、`clear`。
- 回调执行策略：默认 `ExecutionPolicy::Sequential`，同一个订阅回调跨线程串行执行；显式 `ExecutionPolicy::Concurrent` 时允许并发执行。
- 同步分发：`publish()` 在当前线程同步调用快照中的回调，返回前完成本次分发。
- 类型安全：回调签名在订阅时推导，运行期按参数 tuple 类型匹配。
- 智能转换：支持 `const char*` / `char*` 到 `std::string`、`std::string_view`，以及 `std::string` 到 `std::string_view`。
- 任意参数数量：支持 0 到 N 个参数。
- 结果可观测：`publish()` 返回 `PublishResult`，包含调用成功、失败、类型不匹配、跳过等计数。
- 生命周期收敛：`unsubscribe()`、`unsubscribe_all()`、`clear()` 会停止新的调用进入，并等待已进入执行的回调退出。
- 单头文件：只依赖 C++17 标准库。

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

    auto result = bus.publish("greet", "World");
    std::cout << "invoked: " << result.invoked << "\n";

    (void)bus.unsubscribe("greet", id);
}
```

## 执行策略

`subscribe()` 的第三个参数是 `ExecutionPolicy`，默认值是 `ExecutionPolicy::Sequential`。

```cpp
eventbus::EventBus bus;

// 默认策略：同一个回调不会被多个发布线程同时执行。
bus.subscribe("stateful", [](int value) {
    static int total = 0;
    total += value;
});

// 高吞吐策略：EventBus 不为该回调加执行锁。
// 使用者必须保证回调内部没有 data race。
bus.subscribe("fast", [](int value) {
    (void)value;
}, eventbus::ExecutionPolicy::Concurrent);
```

策略只作用于“同一个订阅回调”。同一事件下的不同回调仍按发布线程的同步循环逐个调用；多个线程同时 `publish()` 时，多个发布流程可以并发存在。

## 发布结果

`publish()` 返回 `EventBus::PublishResult`：

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

示例：

```cpp
auto result = bus.publish("event", 42);

if (result.failed != 0 || result.type_mismatches != 0) {
    std::cerr << "dispatch issue\n";
}
```

说明：

- `subscribers`：本次发布快照中的订阅数量。
- `invoked`：成功调用的回调数量。
- `failed`：回调抛出异常的数量。异常会被记录到 `std::cerr`，不会继续向外传播。
- `type_mismatches`：参数不匹配而未调用的数量。
- `skipped`：发布快照中存在但发布前已被取消激活的数量。

## 类型转换

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
bus.publish("view", text);    // std::string -> std::string_view
bus.publish("view", "text");  // const char* -> std::string_view
```

非 `const` 左值引用回调参数被禁止：

```cpp
// 编译失败：EventBus callbacks must not use non-const lvalue reference parameters
bus.subscribe("bad", [](int& value) {
    ++value;
});
```

原因是当前分发载荷会复制到内部 tuple 中，允许 `T&` 会让调用者误以为修改的是原始发布参数。

## 参数复制约定

当前实现会把发布参数复制或移动到同步分发载荷中：

```cpp
args_any = std::make_tuple(std::forward<Args>(args)...);
```

大对象建议传递明确所有权或生命周期的轻量句柄：

```cpp
auto data = std::make_shared<const std::vector<int>>(std::vector<int>{1, 2, 3});
bus.publish("data", data);
```

或在调用方能保证生命周期时传 `const T*`。

## 回调类型

支持普通函数、lambda、函数对象、`std::function`。

```cpp
void on_value(int value);
bus.subscribe("value", on_value);

bus.subscribe("value", [](int value) {
    std::cout << value << "\n";
});

struct Handler
{
    void operator()(int value) const
    {
        std::cout << value << "\n";
    }
};
bus.subscribe("value", Handler{});

std::function<void(int)> fn = [](int value) {
    std::cout << value << "\n";
};
bus.subscribe("value", fn);
```

成员函数建议用 lambda 显式绑定对象生命周期：

```cpp
class Receiver
{
public:
    void on_value(int value);
};

Receiver receiver;
bus.subscribe("value", [&receiver](int value) {
    receiver.on_value(value);
});
```

不要依赖 MSVC 私有 `std::_Binder` 类型。需要绑定时优先使用 lambda。

## 取消订阅语义

```cpp
auto id = bus.subscribe("event", [](int) {
    // work
});

bool removed = bus.unsubscribe("event", id);
```

取消订阅会：

- 从订阅表中移除回调。
- 阻止新的发布调用进入该回调。
- 等待已经进入执行的该回调完成。
- 如果回调在自身内部取消自身订阅，不会等待自己退出，避免自死锁。

## 统计接口

```cpp
auto stats = bus.getStats();
auto names = bus.getAllEventNames();
auto count = bus.getCallbackCount("event");
bool registered = bus.isEventRegistered("event");
```

这些查询接口标记为 `[[nodiscard]]`。

## 条件发布

```cpp
bool published = bus.publish_if_min_subscribers("important", 2, "payload");
```

当事件订阅数量不少于指定阈值时才发布。返回值表示是否执行了发布流程。

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

- `simple_test`：基础功能、类型转换、执行策略、取消订阅等待、异常结果。
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

## 当前边界

- 事件名接口使用 `const std::string&`。C++17 的 `std::unordered_map` 不提供标准异构查找，`std::string_view` 事件名接口没有在当前版本中启用。
- 发布参数当前会进入 `std::any` 持有的 tuple，热路径存在类型擦除与参数复制成本。
- 这是同步事件总线，不提供异步队列、线程池、背压或跨线程投递语义。
