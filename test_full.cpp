/**
 * @file test_full.cpp
 * @brief EventBus complete functionality test program
 */

#include "eventbus.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

using namespace eventbus;

// Test callback functions

void handle_add(int a, int b)
{
    std::cout << "Add: " << a << " + " << b << " = " << a + b << std::endl;
}

void handle_add_multi(int a, int b, int c, int d, int e, int f)
{
    std::cout << "Multi-add: " << a << "+" << b << "+" << c << "+" << d << "+" << e << "+" << f 
              << " = " << a + b + c + d + e + f << std::endl;
}

void handle_task(const std::string& task, const std::string& user, int priority)
{
    std::cout << "Task: " << task << ", User: " << user << ", Priority: " << priority << std::endl;
}

void handle_simple()
{
    std::cout << "Simple event (no parameters)" << std::endl;
}

void handle_complex(const std::string& name, int age, double salary, bool active)
{
    std::cout << "Complex event: Name=" << name << ", Age=" << age 
              << ", Salary=" << salary << ", Active=" << (active ? "yes" : "no") << std::endl;
}

// Function object
struct GreetHandler
{
    void operator()(const std::string &name) const
    {
        std::cout << "Greet: Hello, " << name << "!" << std::endl;
    }
};

// Stateful Lambda
auto create_save_handler()
{
    int count = 0;
    return [count](const std::string &path, const std::vector<int> &data) mutable {
        count++;
        std::cout << "Save (" << count << " times): Path=" << path << ", Size=" << data.size() << std::endl;
    };
}

// Member function example
class Logger
{
public:
    void log_message(const std::string &level, const std::string &msg) const
    {
        std::cout << "[" << level << "] " << msg << std::endl;
    }
    
    void log_with_timestamp(const std::string &msg) const
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::cout << "[TIMESTAMP] " << msg << std::endl;
    }
};

// Performance test
void performance_test(EventBus& bus)
{
    std::cout << "\n=== Performance Test ===" << std::endl;
    
    // Subscribe multiple events
    for (int i = 0; i < 1000; ++i) {
        bus.subscribe("perf_test", [i](int value) {
            volatile int result = value + i;
            (void)result;
        });
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Publish 1000 events
    for (int i = 0; i < 1000; ++i) {
        bus.publish("perf_test", i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Published 1000 events (1000 callbacks each) in: " << duration.count() << " microseconds" << std::endl;
    std::cout << "Average per publish: " << duration.count() / 1000.0 << " microseconds" << std::endl;
    
    // Cleanup
    bus.unsubscribe_all("perf_test");
}

// Thread safety test
void thread_safety_test(EventBus& bus)
{
    std::cout << "\n=== Thread Safety Test ===" << std::endl;
    
    std::atomic<int> counter{0};
    
    // Subscribe counter event
    bus.subscribe("counter", [&counter](int value) {
        counter += value;
    });
    
    const int num_threads = 4;
    const int events_per_thread = 250;
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Start multiple threads publishing events
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&bus, events_per_thread, t]() {
            for (int i = 0; i < events_per_thread; ++i) {
                bus.publish("counter", 1);
                
                // Occasionally subscribe and unsubscribe
                if (i % 50 == 0) {
                    auto id = bus.subscribe("temp_event", [](int x) {});
                    bus.unsubscribe("temp_event", id);
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Multi-thread test completed in: " << duration.count() << " ms" << std::endl;
    std::cout << "Expected counter: " << num_threads * events_per_thread << std::endl;
    std::cout << "Actual counter: " << counter.load() << std::endl;
    std::cout << "Thread safety: " << (counter.load() == num_threads * events_per_thread ? "PASS" : "FAIL") << std::endl;
}

int main()
{
    // Create EventBus with verbose logging
    EventBus bus(true);
    Logger logger;

    std::cout << "=== EventBus Enterprise Functionality Test ===" << std::endl << std::endl;

    // 1. Basic functionality test
    std::cout << "1. Basic Functionality Test" << std::endl;
    auto id1 = bus.subscribe("add", handle_add);
    auto id2 = bus.subscribe("add_multi", handle_add_multi);
    auto id3 = bus.subscribe("greet", GreetHandler{});
    auto id4 = bus.subscribe("save", create_save_handler());
    auto id5 = bus.subscribe("task", handle_task);
    auto id6 = bus.subscribe("log", std::bind(&Logger::log_message, &logger, std::placeholders::_1, std::placeholders::_2));
    auto id7 = bus.subscribe("simple", handle_simple);
    auto id8 = bus.subscribe("complex", handle_complex);
    
    // Add multiple callbacks for same event
    auto id9 = bus.subscribe("add", [](int a, int b) {
        std::cout << "Lambda Add: " << a << " + " << b << " = " << a + b << std::endl;
    });

    std::cout << "\n2. Event Publishing Test (Smart Type Conversion)" << std::endl;
    try {
        bus.publish("add", 5, 3);
        bus.publish("add_multi", 1, 2, 3, 4, 5, 6);
        bus.publish("greet", "Alice");  // const char* -> std::string auto conversion
        bus.publish("save", "/temp", std::vector<int>{4, 5, 6});  // string + container
        bus.publish("task", "Write docs", "Bob", 5);  // multiple strings + int
        bus.publish("log", "INFO", "System started");  // dual strings
        bus.publish("simple");  // no parameters
        bus.publish("complex", "Charlie", 30, 5000.0, true);  // 4 parameters

        std::cout << "\n3. Parameter Mismatch Test" << std::endl;
        bus.publish("add", 1);  // insufficient parameters
        bus.publish("add", 1, 2, 3);  // too many parameters

        std::cout << "\n4. Event Statistics Test" << std::endl;
        std::cout << "Event 'add' callback count: " << bus.getCallbackCount("add") << std::endl;
        std::cout << "Event 'nonexistent' callback count: " << bus.getCallbackCount("nonexistent") << std::endl;

        std::cout << "\n5. Unsubscription Test" << std::endl;
        std::cout << "Unsubscribe callback ID " << id9 << ": " << (bus.unsubscribe("add", id9) ? "success" : "failed") << std::endl;
        std::cout << "Publish 'add' event again:" << std::endl;
        bus.publish("add", 10, 20);

        std::cout << "\n6. Non-existent Event Test" << std::endl;
        bus.publish("nonexistent", "test");

        std::cout << "\n7. Same Event Multiple Parameter Types Test" << std::endl;
        bus.subscribe("mixed", [](int x) { std::cout << "Handle int: " << x << std::endl; });
        bus.subscribe("mixed", [](const std::string& s) { std::cout << "Handle string: " << s << std::endl; });
        
        bus.publish("mixed", 42);
        bus.publish("mixed", "Hello World");

        std::cout << "\n8. Advanced Features Test" << std::endl;
        
        // Get system statistics
        auto stats = bus.getStats();
        std::cout << "EventBus Statistics:" << std::endl;
        std::cout << "- Total events: " << stats.total_events << std::endl;
        std::cout << "- Total callbacks: " << stats.total_callbacks << std::endl;
        std::cout << "- Max callbacks/event: " << stats.max_callbacks_per_event << std::endl;
        std::cout << "- Most popular event: " << stats.most_subscribed_event << std::endl;
        
        // Get all event names
        auto event_names = bus.getAllEventNames();
        std::cout << "All registered events (" << event_names.size() << "): ";
        for (const auto& name : event_names) {
            std::cout << name << " ";
        }
        std::cout << std::endl;
        
        // Conditional publishing test
        std::cout << "\nConditional Publishing Test:" << std::endl;
        bool published = bus.publish_if_min_subscribers("add", 2, 100, 200);
        std::cout << "Publish 'add' with min 2 subscribers: " << (published ? "success" : "failed") << std::endl;
        
        published = bus.publish_if_min_subscribers("add", 1, 100, 200);
        std::cout << "Publish 'add' with min 1 subscriber: " << (published ? "success" : "failed") << std::endl;
        
        // Batch unsubscription test
        size_t removed = bus.unsubscribe_all("mixed");
        std::cout << "Batch unsubscribe 'mixed' event, removed " << removed << " callbacks" << std::endl;

    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    // Performance test
    performance_test(bus);
    
    // Thread safety test
    thread_safety_test(bus);

    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "EventBus demonstrates these enterprise features:" << std::endl;
    std::cout << "  Thread-safe concurrent access" << std::endl;
    std::cout << "  Smart type conversion (const char* -> std::string)" << std::endl;
    std::cout << "  Support for arbitrary parameter count" << std::endl;
    std::cout << "  Complete statistics and monitoring" << std::endl;
    std::cout << "  Exception safety and error handling" << std::endl;
    std::cout << "  High-performance event dispatch" << std::endl;
    std::cout << "  Flexible subscription management" << std::endl;
    
    return 0;
}
