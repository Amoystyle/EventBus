#include "eventbus.hpp"
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

using namespace eventbus;

struct TradeTicket
{
    int id{};
    std::string symbol;
    std::map<std::string, double> metrics;
};

int main()
{
    EventBus bus(true);

    bool map_verified = false;
    bool tuple_verified = false;
    bool custom_verified = false;

    auto map_id = bus.subscribe("inventory.update",
        [&](const std::map<std::string, std::vector<int>>& inventory) {
            map_verified = inventory.size() == 2 &&
                inventory.at("warehouseA").size() == 3 &&
                inventory.at("warehouseB").front() == 4;
        });

    auto tuple_id = bus.subscribe("telemetry.packet",
        [&](const std::tuple<int, double, std::string>& packet) {
            auto [sequence, latency_ms, region] = packet;
            tuple_verified = sequence == 42 && latency_ms < 2.5 && region == "us-east";
        });

    auto custom_id = bus.subscribe("trade.executed",
        [&](const TradeTicket& ticket) {
            auto it = ticket.metrics.find("fee");
            custom_verified = ticket.id == 9001 &&
                ticket.symbol == "EVT" &&
                it != ticket.metrics.end() &&
                it->second == 1.25;
        });

    std::map<std::string, std::vector<int>> inventory = {
        {"warehouseA", {1, 2, 3}},
        {"warehouseB", {4, 5}}
    };
    bus.publish("inventory.update", inventory);

    auto packet = std::make_tuple(42, 1.5, std::string("us-east"));
    bus.publish("telemetry.packet", packet);

    TradeTicket ticket;
    ticket.id = 9001;
    ticket.symbol = "EVT";
    ticket.metrics["fee"] = 1.25;
    ticket.metrics["latency"] = 0.87;
    bus.publish("trade.executed", ticket);

    assert(map_verified && "Map payload was not delivered correctly");
    assert(tuple_verified && "Tuple payload was not delivered correctly");
    assert(custom_verified && "Custom payload was not delivered correctly");

    std::cout << "Complex type tests passed (map, tuple, custom)\n" << std::endl;

    bus.unsubscribe("inventory.update", map_id);
    bus.unsubscribe("telemetry.packet", tuple_id);
    bus.unsubscribe("trade.executed", custom_id);

    return 0;
}

