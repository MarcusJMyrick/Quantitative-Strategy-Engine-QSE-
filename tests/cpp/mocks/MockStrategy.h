#pragma once

#include <gmock/gmock.h>
#include "qse/data/Data.h"           // For qse::Tick and qse::Bar
#include "qse/strategy/IStrategy.h"

class MockStrategy : public qse::IStrategy {
public:
    // Add the new on_tick method to the mock
    MOCK_METHOD(void, on_tick, (const qse::Tick& tick), (override));
    
    // The on_bar mock remains
    MOCK_METHOD(void, on_bar, (const qse::Bar& bar), (override));
};
