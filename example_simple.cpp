/**
 * @file example_simple.cpp
 * @brief EventBus simple usage example
 */

#include "eventbus.hpp"
#include <iostream>
#include <string>
#include <vector>

using namespace eventbus;

// Simulate user management system
class UserManager
{
private:
    EventBus& bus_;
    
public:
    explicit UserManager(EventBus& bus) : bus_(bus)
    {
        // Subscribe to user-related events
        bus_.subscribe("user_login", [this](const std::string& username) {
            onUserLogin(username);
        });
        
        bus_.subscribe("user_logout", [this](const std::string& username) {
            onUserLogout(username);
        });
    }
    
    void onUserLogin(const std::string& username)
    {
        std::cout << "UserManager: " << username << " logged in" << std::endl;
        // Publish user status change event
        bus_.publish("user_status_changed", username, "online");
    }
    
    void onUserLogout(const std::string& username)
    {
        std::cout << "UserManager: " << username << " logged out" << std::endl;
        bus_.publish("user_status_changed", username, "offline");
    }
};

// Simulate notification system
class NotificationService
{
private:
    EventBus& bus_;
    
public:
    explicit NotificationService(EventBus& bus) : bus_(bus)
    {
        bus_.subscribe("user_status_changed", [this](const std::string& username, const std::string& status) {
            sendNotification(username, status);
        });
        
        bus_.subscribe("system_alert", [this](const std::string& message, int priority) {
            sendAlert(message, priority);
        });
    }
    
    void sendNotification(const std::string& username, const std::string& status)
    {
        std::cout << "Notification: " << username << " status changed to " << status << std::endl;
    }
    
    void sendAlert(const std::string& message, int priority)
    {
        std::cout << "System Alert [Priority:" << priority << "] " << message << std::endl;
    }
};

// Simulate logging system
class LogService
{
private:
    EventBus& bus_;
    
public:
    explicit LogService(EventBus& bus) : bus_(bus)
    {
        // Subscribe to all user events for logging
        bus_.subscribe("user_login", [this](const std::string& username) {
            log("INFO", "User " + username + " logged in");
        });
        
        bus_.subscribe("user_logout", [this](const std::string& username) {
            log("INFO", "User " + username + " logged out");
        });
        
        bus_.subscribe("system_alert", [this](const std::string& message, int priority) {
            log(priority >= 3 ? "ERROR" : "WARN", message);
        });
    }
    
    void log(const std::string& level, const std::string& message)
    {
        std::cout << "[LOG-" << level << "] " << message << std::endl;
    }
};

int main()
{
    std::cout << "=== EventBus Practical Usage Example ===" << std::endl << std::endl;
    
    // Create event bus (disable verbose logging for clean output)
    EventBus bus(false);
    
    // Create service components
    UserManager userManager(bus);
    NotificationService notificationService(bus);
    LogService logService(bus);
    
    std::cout << "1. Simulate user login/logout flow" << std::endl;
    std::cout << "-----------------------------------" << std::endl;
    
    // Simulate user operations
    bus.publish("user_login", "Alice");
    std::cout << std::endl;
    
    bus.publish("user_login", "Bob");
    std::cout << std::endl;
    
    bus.publish("user_logout", "Alice");
    std::cout << std::endl;
    
    std::cout << "2. Simulate system alerts" << std::endl;
    std::cout << "-------------------------" << std::endl;
    
    bus.publish("system_alert", "Low disk space", 2);
    std::cout << std::endl;
    
    bus.publish("system_alert", "Database connection failed", 5);
    std::cout << std::endl;
    
    std::cout << "3. View event bus status" << std::endl;
    std::cout << "------------------------" << std::endl;
    
    auto stats = bus.getStats();
    std::cout << "Total event types: " << stats.total_events << std::endl;
    std::cout << "Total callbacks: " << stats.total_callbacks << std::endl;
    std::cout << "Most popular event: " << stats.most_subscribed_event << std::endl;
    
    auto events = bus.getAllEventNames();
    std::cout << "Registered events: ";
    for (const auto& event : events) {
        std::cout << event << "(" << bus.getCallbackCount(event) << ") ";
    }
    std::cout << std::endl << std::endl;
    
    std::cout << "4. Demonstrate type conversion feature" << std::endl;
    std::cout << "--------------------------------------" << std::endl;
    
    // Subscribe to an event that requires std::string parameter
    bus.subscribe("string_event", [](const std::string& msg) {
        std::cout << "Received string: " << msg << std::endl;
    });
    
    // Publish using const char*, will auto-convert to std::string
    bus.publish("string_event", "This is a string literal");
    
    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    std::cout << "This example demonstrates:" << std::endl;
    std::cout << "  Loose coupling between components" << std::endl;
    std::cout << "  Event-driven architecture implementation" << std::endl;
    std::cout << "  Automatic type conversion functionality" << std::endl;
    std::cout << "  System status monitoring capabilities" << std::endl;
    
    return 0;
}
