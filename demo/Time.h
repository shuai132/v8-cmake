#pragma once

#include <cstdint>
using namespace std::chrono;

class TimeTracker {
public:
    TimeTracker() {
        items.reserve(1024);
        track("init");
    }

    void track(std::string name) {
        items.push_back({
            std::move(name),
            duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count(),
        });
    }

    void report() {
        printf("TimeTracker report:\n");
        if (items.empty()) {
            printf("  nothing to report\n");
            return;
        }
        long lastItemTime = items[0].time;
        for (const auto& item : items) {
            printf("  %s: elapsed: %ld\n", item.name.c_str(), item.time - lastItemTime);
            lastItemTime = item.time;
        }
        printf("------\n");
    }
private:
    struct Item {
        std::string name;
        long time;
    };

    std::vector<Item> items;
};
