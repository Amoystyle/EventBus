# EventBus Project Summary

## 当前状态

EventBus 当前是一个 C++17 单头文件同步事件总线，实现集中在 `eventbus.hpp`。它面向进程内事件分发，不包含异步队列、线程池、跨线程投递或背压机制。

## 已实现能力

- 线程安全订阅表：使用 `std::shared_mutex` 管理事件到回调列表的映射。
- 同步发布：`publish()` 在调用线程中同步执行回调快照。
- 回调执行策略：
  - `ExecutionPolicy::Sequential`：默认策略，同一个回调跨发布线程串行执行。
  - `ExecutionPolicy::Concurrent`：显式性能策略，同一个回调允许并发执行。
- 生命周期处理：取消订阅和清空订阅会等待已进入执行的回调完成。
- 结果返回：`publish()` 返回 `PublishResult`，上报调用、失败、类型不匹配和跳过计数。
- 类型转换：支持 `const char*` / `char*` 到 `std::string`、`std::string_view`，以及 `std::string` 到 `std::string_view`。
- 类型约束：回调必须返回 `void`，禁止非 `const` 左值引用参数。
- 查询接口：事件注册状态、回调数量、事件名列表、统计信息。
- 构建集成：CMake 目标和 CTest 测试已覆盖基础、完整、复杂类型、使用示例。

## 核心接口

```cpp
namespace eventbus {

enum class ExecutionPolicy
{
    Sequential,
    Concurrent
};

using callback_id = std::size_t;

class EventBus
{
public:
    explicit EventBus(bool verbose_logging = false);

    template <typename Callback>
    callback_id subscribe(const std::string& eventName,
                          Callback&& callback,
                          ExecutionPolicy policy = ExecutionPolicy::Sequential);

    [[nodiscard]] bool unsubscribe(const std::string& eventName, callback_id id);

    template <typename... Args>
    PublishResult publish(const std::string& eventName, Args&&... args);

    [[nodiscard]] std::size_t unsubscribe_all(const std::string& eventName);
    void clear();
};

} // namespace eventbus
```

## 并发语义

订阅表本身是线程安全的。`publish()` 会先复制当前事件的回调快照，然后释放总线锁，再执行回调。这样避免了持有订阅表锁时运行用户代码。

每个回调条目有独立状态：

- `active` 控制是否允许新调用进入。
- `in_flight` 记录已经进入执行的调用数量。
- `idle_cv` 用于取消订阅时等待执行完成。

`ExecutionPolicy::Sequential` 会在 `CallbackWrapper` 内对同一个回调使用 `std::recursive_mutex`。选择递归锁是为了允许同一线程内的重入发布，不因同一回调链路重入而自死锁。

`ExecutionPolicy::Concurrent` 不加这把执行锁。该策略只应给无状态回调或内部已同步的回调使用。

## 取消订阅语义

`unsubscribe()`、`unsubscribe_all()`、`clear()` 的行为：

1. 在订阅表锁下找到目标条目。
2. 将条目标记为 inactive。
3. 从订阅表移除。
4. 释放订阅表锁。
5. 等待该条目的 `in_flight == 0`。

如果回调取消自身订阅，当前线程不会等待自己退出，避免自死锁。

## 类型系统和转换

订阅时通过 `function_traits` 推导回调签名。发布时把参数打包到 `std::any` 持有的 tuple 中，再按回调签名匹配。

转换支持范围：

- 精确 tuple 类型匹配。
- decay 后 tuple 类型匹配。
- `const char*` / `char*` 到 `std::string`。
- `const char*` / `char*` 到 `std::string_view`。
- `std::string` 到 `std::string_view`。
- 标准 `std::is_convertible_v` 可转换类型。

禁止非 `const` 左值引用参数，避免回调修改的是内部参数副本而不是发布方原始对象。

## 错误处理

回调异常不会穿透 `publish()`。发布流程会捕获 `std::exception` 和未知异常，写入 `std::cerr`，并递增 `PublishResult::failed`。

类型不匹配不会抛异常，计入 `PublishResult::type_mismatches`。

## 性能边界

当前实现为安全和通用性做了这些取舍：

- 发布参数会进入 `std::any` 和 tuple，存在类型擦除和参数复制成本。
- 默认 `Sequential` 会为同一回调加执行锁，适合安全默认，不适合极限热路径。
- `Concurrent` 可移除同一回调执行锁成本，但将回调内部 data race 责任交给调用方。
- 事件名仍使用 `std::string`。C++17 标准 `std::unordered_map` 缺少异构查找，当前未提供 `std::string_view` 事件名接口。

大对象载荷建议使用：

- `std::shared_ptr<T>`
- `std::shared_ptr<const T>`
- 调用方保证生命周期的 `const T*`

## 测试覆盖

当前 CTest 测试：

- `SimpleTest`
  - 基础订阅发布
  - `std::string` / `std::string_view` 转换
  - 零参数匹配
  - 回调自身取消订阅
  - 默认 `Sequential` 串行执行
  - 显式 `Concurrent` 并发执行
  - 取消订阅等待 in-flight
  - 回调异常计数
- `CompleteTest`
  - 多回调、多参数、统计、条件发布、线程安全示例
- `ComplexTypeTest`
  - `std::map`、`std::tuple`、自定义结构体载荷
- `UsageExample`
  - 实际业务风格示例

验证命令：

```bash
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure --timeout 30
```

GCC 严格告警验证：

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -I. -c simple_test.cpp -o build/simple_test_gcc.o
g++ -std=c++17 -Wall -Wextra -Wpedantic -I. -c test_full.cpp -o build/test_full_gcc.o
g++ -std=c++17 -Wall -Wextra -Wpedantic -I. -c test_complex_types.cpp -o build/test_complex_types_gcc.o
```

## 后续演进方向

- C++20 版本可考虑为事件名引入异构查找和 `std::string_view` 接口。
- 为极限热路径提供无 `std::any` 的强类型 channel。
- 增加可选异步 executor，但必须定义队列、停止、drain、异常和背压语义。
- 增加基准测试，区分 `Sequential` 与 `Concurrent` 策略的吞吐差异。
