#include "KeyboardService.h"

#include "M5Cardputer.h"

namespace KeyboardService {
    bool isChanged() {
        return M5Cardputer.Keyboard.isChange();
    }
    uint8_t isPressed() {
        return M5Cardputer.Keyboard.isPressed();
    }
    bool isKeyPressed(char c) {
        return M5Cardputer.Keyboard.isKeyPressed(c);
    }
    uint8_t getKey(int32_t x, int32_t y) {
        Point2D_t keyCoord;
        keyCoord.x = static_cast<int>(x);
        keyCoord.y = static_cast<int>(y);
        return M5Cardputer.Keyboard.getKey(keyCoord);
    }
}