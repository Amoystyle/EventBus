# EventBus Delivery Notes

## 交付内容

核心文件：

- `eventbus.hpp`：单头文件 EventBus 实现。
- `simple_test.cpp`：基础功能和关键语义测试。
- `test_full.cpp`：完整功能、统计、条件发布、线程安全示例。
- `test_complex_types.cpp`：复杂类型载荷测试。
- `example_simple.cpp`：业务风格使用示例。

构建和运行：

- `CMakeLists.txt`：CMake 目标和 CTest 测试。
- `build.bat`：Windows 构建脚本。
- `demo.bat`：演示脚本。

文档：

- `README.md`：完整使用和 API 说明。
- `QUICK_START.md`：快速上手。
- `PROJECT_SUMMARY.md`：当前技术实现总结。
- `DELIVERY_NOTES.md`：本交付说明。

## 当前 API 重点

```cpp
eventbus::EventBus bus;

auto id = bus.subscribe("event", [](const std::string& value) {
    std::cout << value << "\n";
});

auto result = bus.publish("event", "payload");

(void)bus.unsubscribe("event", id);
```

EventBus 不为订阅回调加执行锁，回调内部共享状态同步责任由业务方承担。

无状态或内部已同步的回调可以直接订阅：

```cpp
bus.subscribe("fast", [](int value) {
    (void)value;
});
```

同一个回调可能被多个发布线程并发调用。

## 已验证能力

- 多线程安全订阅、发布、取消订阅。
- 默认并发回调执行，不持有回调级执行锁。
- `publish()` 返回 `PublishResult`。
- 回调异常计入失败结果。
- 类型不匹配计数。
- `const char*` 到 `std::string` / `std::string_view`。
- `std::string` 到 `std::string_view`。
- 复杂 STL 类型和自定义类型载荷。
- 取消订阅等待已进入执行的回调完成。

## 构建验证

推荐验证命令：

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure --timeout 30
```

已使用的额外编译验证：

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -I. -c simple_test.cpp -o build/simple_test_gcc.o
g++ -std=c++17 -Wall -Wextra -Wpedantic -I. -c test_full.cpp -o build/test_full_gcc.o
g++ -std=c++17 -Wall -Wextra -Wpedantic -I. -c test_complex_types.cpp -o build/test_complex_types_gcc.o
```

## 集成注意事项

- 只需要包含 `eventbus.hpp`。
- 回调必须返回 `void`。
- 回调不能使用非 `const` 左值引用参数。
- `publish()` 是同步调用，不是异步投递。
- 大对象载荷会被复制或移动进分发 tuple；建议用 `std::shared_ptr<const T>` 或生命周期明确的 `const T*`。
- 成员函数回调建议使用 lambda 显式绑定对象，并保证对象生命周期覆盖订阅期。
- `unsubscribe()`、`unsubscribe_all()`、`clear()` 可能等待正在执行的回调完成，不应在对延迟极敏感的线程中无评估调用。

## 当前限制

- 事件名接口仍是 `const std::string&`。
- 热路径仍有 `std::any` 类型擦除和参数打包成本。
- 当前版本不提供异步 executor、任务队列或背压。
- 未提供真实基准测试报告；性能应以目标平台上的专门 benchmark 为准。
