/**
 * @file simple_test.cpp
 * @brief Simple test for EventBus
 */

#include "eventbus.hpp"
#include <iostream>
#include <vector>
#include <string>

using namespace eventbus;

void handle_add(int a, int b)
{
    std::cout << "Add: " << a << " + " << b << " = " << a + b << std::endl;
}

void handle_greet(const std::string& name)
{
    std::cout << "Hello, " << name << "!" << std::endl;
}

void handle_save(const std::string& path, const std::vector<int>& data)
{
    std::cout << "Save to: " << path << ", size: " << data.size() << std::endl;
}

int main()
{
    std::cout << "=== EventBus Clean Test ===" << std::endl;
    
    EventBus bus(true); // Enable verbose logging
    
    // Subscribe events
    auto id1 = bus.subscribe("add", handle_add);
    auto id2 = bus.subscribe("greet", handle_greet);
    auto id3 = bus.subscribe("save", handle_save);
    
    // Test lambda
    auto id4 = bus.subscribe("lambda", [](int x) {
        std::cout << "Lambda: " << x << std::endl;
    });
    
    std::cout << "\n=== Publishing Events ===" << std::endl;
    
    // Test basic events
    bus.publish("add", 5, 3);
    bus.publish("greet", "World");  // const char* -> std::string conversion
    bus.publish("save", "/tmp/file.dat", std::vector<int>{1, 2, 3, 4, 5});
    bus.publish("lambda", 42);
    
    std::cout << "\n=== Statistics ===" << std::endl;
    auto stats = bus.getStats();
    std::cout << "Total events: " << stats.total_events << std::endl;
    std::cout << "Total callbacks: " << stats.total_callbacks << std::endl;
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    return 0;
}
