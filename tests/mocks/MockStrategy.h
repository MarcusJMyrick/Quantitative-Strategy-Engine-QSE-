#pragma once

#include <gmock/gmock.h>
#include "IStrategy.h"

class MockStrategy : public qse::IStrategy {
public:
    MOCK_METHOD(void, on_bar, (const qse::Bar& bar), (override));
}; 