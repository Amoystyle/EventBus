# EventBus Quick Start

## 1. 构建和测试

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Windows 脚本：

```bat
build.bat
demo.bat
```

## 2. 最小示例

```cpp
#include "eventbus.hpp"

#include <iostream>
#include <string>

int main()
{
    eventbus::EventBus bus;

    auto id = bus.subscribe("hello", [](const std::string& name) {
        std::cout << "Hello, " << name << "\n";
    });

    auto result = bus.publish("hello", "World");
    std::cout << "called: " << result.invoked << "\n";

    (void)bus.unsubscribe("hello", id);
}
```

## 3. 默认安全执行

默认订阅策略是 `ExecutionPolicy::Sequential`。同一个回调不会被多个发布线程同时执行。

```cpp
bus.subscribe("counter", [](int value) {
    static int total = 0;
    total += value;
});
```

这适合有内部状态、会访问成员变量或全局变量的回调。

## 4. 显式并发执行

当回调无状态，或回调内部已经自己同步共享状态时，可以使用 `ExecutionPolicy::Concurrent`。

```cpp
bus.subscribe("metrics", [](int value) {
    (void)value;
}, eventbus::ExecutionPolicy::Concurrent);
```

使用 `Concurrent` 后，EventBus 不会为同一个回调加执行锁。调用方必须保证没有 data race。

## 5. 字符串转换

```cpp
bus.subscribe("message", [](const std::string& msg) {
    std::cout << msg << "\n";
});
bus.publish("message", "const char payload");

bus.subscribe("view", [](std::string_view msg) {
    std::cout << msg << "\n";
});
std::string text = "owned payload";
bus.publish("view", text);
bus.publish("view", "literal payload");
```

## 6. 多参数事件

```cpp
bus.subscribe("user_action",
    [](const std::string& user, const std::string& action, int priority) {
        std::cout << user << " " << action << " " << priority << "\n";
    });

bus.publish("user_action", "Alice", "login", 5);
```

## 7. 查看发布结果

```cpp
auto result = bus.publish("user_action", "Alice", "login", 5);

if (result.invoked == 0 || result.failed != 0 || result.type_mismatches != 0) {
    std::cerr << "dispatch failed or mismatched\n";
}
```

## 8. 文件说明

- `eventbus.hpp`：核心单头文件。
- `simple_test.cpp`：基础功能、执行策略和异常结果测试。
- `test_full.cpp`：完整功能、统计、条件发布和线程安全示例。
- `test_complex_types.cpp`：复杂 STL 类型和自定义类型载荷测试。
- `example_simple.cpp`：实际使用示例。
- `README.md`：完整 API 和语义说明。
- `PROJECT_SUMMARY.md`：当前实现总结。
