#pragma once

#include <cstdint>
#include "log.h"

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
        LOGI("TimeTracker report:");
        if (items.empty()) {
            LOGI("  nothing to report");
            return;
        }
        long lastItemTime = items[0].time;
        for (const auto& item : items) {
            LOGI("  %s: %ld", item.name.c_str(), item.time - lastItemTime);
            lastItemTime = item.time;
        }
        LOGI("------\n");
    }
private:
    struct Item {
        std::string name;
        long time;
    };

    std::vector<Item> items;
};
