#pragma once

#include <gmock/gmock.h>
#include "IStrategy.h"

class MockStrategy : public IStrategy {
public:
    // Add the new on_tick method to the mock
    MOCK_METHOD(void, on_tick, (const qse::Tick& tick), (override));
    
    // The on_bar mock remains
    MOCK_METHOD(void, on_bar, (const qse::Bar& bar), (override));
};
