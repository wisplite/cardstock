#pragma once

#include <Arduino.h>

namespace KeyboardService {
    bool isChanged();
    uint8_t isPressed(); // returns number of pressed keys
    bool isKeyPressed(char c); // returns true if the key is pressed
    uint8_t getKey(int32_t x, int32_t y); // returns the key code of the pressed key at (x,y)
}