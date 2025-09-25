# EventBus 快速开始指南

## 🚀 立即体验

### 1. 一键构建和测试
```bash
# Windows
build.bat

# Linux/macOS  
make test

# 跨平台 CMake
mkdir build && cd build && cmake .. && make
```

### 2. 运行演示
```bash
demo.bat          # 完整演示程序
simple_test.exe   # 基础功能测试
usage_example.exe # 实际使用示例
```

## 💡 5分钟上手

### 基本使用
```cpp
#include "eventbus.hpp"
using namespace eventbus;

int main() {
    EventBus bus;
    
    // 订阅事件
    bus.subscribe("hello", [](const std::string& name) {
        std::cout << "Hello, " << name << "!" << std::endl;
    });
    
    // 发布事件 - 自动类型转换！
    bus.publish("hello", "World");
    
    return 0;
}
```

### 智能类型转换
```cpp
// 订阅需要 std::string 的事件
bus.subscribe("message", [](const std::string& msg) {
    std::cout << "Message: " << msg << std::endl;
});

// 发布时使用 const char* - 自动转换！
bus.publish("message", "This is a C string");
```

### 多参数支持
```cpp
// 复杂事件处理
bus.subscribe("user_action", [](const std::string& user, const std::string& action, int priority) {
    std::cout << "User " << user << " performed " << action 
              << " with priority " << priority << std::endl;
});

bus.publish("user_action", "Alice", "login", 5);
```

## 📁 文件说明

- **`eventbus.hpp`** - 核心头文件，包含即用
- **`simple_test.cpp`** - 基础功能演示
- **`example_simple.cpp`** - 实际应用示例
- **`test_full.cpp`** - 完整功能测试（性能+线程安全）

## 🎯 核心特性

✅ **线程安全** - 支持多线程并发访问  
✅ **智能转换** - 自动 `const char*` → `std::string`  
✅ **零开销** - 编译时优化，运行时高性能  
✅ **类型安全** - 编译时和运行时双重保障  
✅ **易集成** - 单头文件，直接包含使用  

## 📖 更多信息

- **README.md** - 完整文档和API参考
- **PROJECT_SUMMARY.md** - 技术实现详解

**开始您的EventBus之旅吧！** 🎊
