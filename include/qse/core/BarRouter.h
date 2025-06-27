#pragma once

#include "qse/data/Data.h"
#include "qse/strategy/IStrategy.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>

namespace qse {

/**
 * @brief Simple dispatcher that routes bars to strategies that have
 *        registered interest in a given symbol. The implementation is
 *        intentionally lightweight and header-only so we can avoid touching
 *        the build system / CMakeLists.txt.
 */
class BarRouter {
public:
    // Register the given strategy for a particular symbol. Duplicate
    // registrations are ignored.
    void register_strategy(const std::string& symbol, IStrategy* strategy) {
        if (!strategy) return;
        auto& vec = routes_[symbol];
        if (std::find(vec.begin(), vec.end(), strategy) == vec.end()) {
            vec.push_back(strategy);
        }
    }

    // Dispatch a bar to all interested strategies.
    void route_bar(const Bar& bar) const {
        auto it = routes_.find(bar.symbol);
        if (it == routes_.end()) return;
        for (auto* strat : it->second) {
            if (strat) {
                strat->on_bar(bar);
            }
        }
    }

private:
    std::unordered_map<std::string, std::vector<IStrategy*>> routes_;
};

} // namespace qse 