#pragma once

#include <cstdint>

extern uint32_t g_mock_millis;

inline uint32_t millis() { return g_mock_millis; }
inline void delay(uint32_t ms) { g_mock_millis += ms; }
