/**
 * @file simple_test.cpp
 * @brief Simple test for EventBus
 */

#include "eventbus.hpp"
#include <cassert>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <string>
#include <string_view>

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
    (void)id1;
    (void)id2;
    (void)id3;
    (void)id4;
    
    std::cout << "\n=== Publishing Events ===" << std::endl;
    
    // Test basic events
    auto add_result = bus.publish("add", 5, 3);
    assert(add_result.invoked == 1);
    assert(add_result.failed == 0);
    bus.publish("greet", "World");  // const char* -> std::string conversion
    bus.publish("save", "/tmp/file.dat", std::vector<int>{1, 2, 3, 4, 5});
    bus.publish("lambda", 42);

    int string_view_calls = 0;
    std::string observed_view;
    bus.subscribe("string_view", [&string_view_calls, &observed_view](std::string_view message) {
        ++string_view_calls;
        observed_view.assign(message.data(), message.size());
    });
    std::string owned_message = "Owned message";
    auto string_view_from_string = bus.publish("string_view", owned_message);
    assert(string_view_from_string.invoked == 1);
    assert(observed_view == "Owned message");
    auto string_view_from_cstr = bus.publish("string_view", "Literal message");
    assert(string_view_from_cstr.invoked == 1);
    assert(observed_view == "Literal message");
    assert(string_view_calls == 2);

    int zero_arg_calls = 0;
    bus.subscribe("zero_arg", [&zero_arg_calls]() {
        ++zero_arg_calls;
    });
    auto zero_arg_mismatch = bus.publish("zero_arg", 1);
    assert(zero_arg_calls == 0);
    assert(zero_arg_mismatch.type_mismatches == 1);
    bus.publish("zero_arg");
    assert(zero_arg_calls == 1);

    bool reentrant_called = false;
    callback_id reentrant_id = 0;
    reentrant_id = bus.subscribe("reentrant", [&bus, &reentrant_called, &reentrant_id](int value) {
        reentrant_called = value == 7;
        bus.subscribe("reentrant_child", [](int) {});
        (void)bus.unsubscribe("reentrant", reentrant_id);
    });
    bus.publish("reentrant", 7);
    assert(reentrant_called);
    assert(bus.getCallbackCount("reentrant") == 0);
    assert(bus.getCallbackCount("reentrant_child") == 1);

    std::atomic<int> active_callbacks{0};
    std::atomic<int> max_active_callbacks{0};
    std::atomic<bool> start_serialized_publishers{false};
    bus.subscribe("serialized", [&active_callbacks, &max_active_callbacks](int) {
        const int active = active_callbacks.fetch_add(1) + 1;
        int observed = max_active_callbacks.load();
        while (active > observed &&
               !max_active_callbacks.compare_exchange_weak(observed, active)) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        active_callbacks.fetch_sub(1);
    });

    std::vector<std::thread> serialized_publishers;
    for (int i = 0; i < 4; ++i) {
        serialized_publishers.emplace_back([&bus, &start_serialized_publishers]() {
            while (!start_serialized_publishers.load()) {
                std::this_thread::yield();
            }
            for (int j = 0; j < 20; ++j) {
                bus.publish("serialized", j);
            }
        });
    }
    start_serialized_publishers.store(true);
    for (auto& thread : serialized_publishers) {
        thread.join();
    }
    assert(max_active_callbacks.load() == 1);

    std::atomic<int> concurrent_active_callbacks{0};
    std::atomic<int> concurrent_max_active_callbacks{0};
    std::atomic<bool> start_concurrent_publishers{false};
    bus.subscribe("concurrent", [&concurrent_active_callbacks, &concurrent_max_active_callbacks](int) {
        const int active = concurrent_active_callbacks.fetch_add(1) + 1;
        int observed = concurrent_max_active_callbacks.load();
        while (active > observed &&
               !concurrent_max_active_callbacks.compare_exchange_weak(observed, active)) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        concurrent_active_callbacks.fetch_sub(1);
    }, ExecutionPolicy::Concurrent);

    std::vector<std::thread> concurrent_publishers;
    for (int i = 0; i < 4; ++i) {
        concurrent_publishers.emplace_back([&bus, &start_concurrent_publishers]() {
            while (!start_concurrent_publishers.load()) {
                std::this_thread::yield();
            }
            for (int j = 0; j < 20; ++j) {
                bus.publish("concurrent", j);
            }
        });
    }
    start_concurrent_publishers.store(true);
    for (auto& thread : concurrent_publishers) {
        thread.join();
    }
    assert(concurrent_max_active_callbacks.load() > 1);

    std::atomic<bool> callback_started{false};
    std::atomic<bool> callback_finished{false};
    auto slow_id = bus.subscribe("unsubscribe_waits", [&callback_started, &callback_finished]() {
        callback_started.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        callback_finished.store(true);
    });

    std::thread publisher([&bus]() {
        bus.publish("unsubscribe_waits");
    });
    while (!callback_started.load()) {
        std::this_thread::yield();
    }
    assert(bus.unsubscribe("unsubscribe_waits", slow_id));
    assert(callback_finished.load());
    publisher.join();

    bus.subscribe("throws", []() {
        throw std::runtime_error("expected test exception");
    });
    auto throw_result = bus.publish("throws");
    assert(throw_result.failed == 1);
    assert(throw_result.invoked == 0);
    
    std::cout << "\n=== Statistics ===" << std::endl;
    auto stats = bus.getStats();
    std::cout << "Total events: " << stats.total_events << std::endl;
    std::cout << "Total callbacks: " << stats.total_callbacks << std::endl;
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    return 0;
}
