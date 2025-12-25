#include "GfxService.h"

#include "M5Cardputer.h"

namespace GfxService {

void clear(uint16_t color) {
  M5Cardputer.Display.fillScreen(color);
}

void setCursor(int32_t x, int32_t y) {
  M5Cardputer.Display.setCursor(x, y);
}

void setTextSize(uint8_t size) {
  M5Cardputer.Display.setTextSize(size);
}

void setTextColor(uint16_t fg, int32_t bg) {
  if (bg < 0) {
    M5Cardputer.Display.setTextColor(fg);
  } else {
    M5Cardputer.Display.setTextColor(fg, static_cast<uint16_t>(bg));
  }
}

void print(const char* s) {
  if (!s) return;
  M5Cardputer.Display.print(s);
}

void println(const char* s) {
  if (!s) {
    M5Cardputer.Display.println();
    return;
  }
  M5Cardputer.Display.println(s);
}

int32_t drawString(const char* s, int32_t x, int32_t y, int32_t font) {
  if (!s) return 0;
  if (font < 0) {
    return M5Cardputer.Display.drawString(s, x, y);
  }
  return M5Cardputer.Display.drawString(s, x, y, font);
}

}  // namespace GfxService


