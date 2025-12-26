#pragma once

#include <Arduino.h>

// Thin wrapper around the device display so Lua bindings don't touch hardware globals directly.
namespace GfxService {

void clear(uint16_t color = 0x0000 /* BLACK */);
void setCursor(int32_t x, int32_t y);
void setTextSize(uint8_t size);
void setTextColor(uint16_t fg, int32_t bg = -1);  // bg < 0 => don't set background
void print(const char* s);
void println(const char* s);
void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color = 0x0000 /* BLACK */);

// Returns the pixel width of the rendered string (per M5GFX/LovyanGFX convention).
int32_t drawString(const char* s, int32_t x, int32_t y, int32_t font = -1);
int32_t drawCenterString(const char* s, int32_t x, int32_t y, int32_t font = -1);

int32_t width();
int32_t height();
}  // namespace GfxService


