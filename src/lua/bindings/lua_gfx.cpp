#include "lua_gfx.h"

#include "services/GfxService.h"

static uint16_t lua_check_u16(lua_State* L, int idx) {
  lua_Integer v = luaL_checkinteger(L, idx);
  if (v < 0) v = 0;
  if (v > 0xFFFF) v = 0xFFFF;
  return static_cast<uint16_t>(v);
}

static int l_gfx_clear(lua_State* L) {
  uint16_t color = 0x0000;
  if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) color = lua_check_u16(L, 1);
  GfxService::clear(color);
  return 0;
}

static int l_gfx_set_cursor(lua_State* L) {
  int32_t x = static_cast<int32_t>(luaL_checkinteger(L, 1));
  int32_t y = static_cast<int32_t>(luaL_checkinteger(L, 2));
  GfxService::setCursor(x, y);
  return 0;
}

static int l_gfx_set_text_size(lua_State* L) {
  lua_Integer s = luaL_checkinteger(L, 1);
  if (s < 1) s = 1;
  if (s > 255) s = 255;
  GfxService::setTextSize(static_cast<uint8_t>(s));
  return 0;
}

static int l_gfx_set_text_color(lua_State* L) {
  uint16_t fg = lua_check_u16(L, 1);
  int32_t bg = -1;
  if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) bg = static_cast<int32_t>(lua_check_u16(L, 2));
  GfxService::setTextColor(fg, bg);
  return 0;
}

static int l_gfx_print(lua_State* L) {
  const char* s = luaL_checkstring(L, 1);
  GfxService::print(s);
  return 0;
}

static int l_gfx_println(lua_State* L) {
  const char* s = nullptr;
  if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) s = luaL_checkstring(L, 1);
  GfxService::println(s);
  return 0;
}

static int l_gfx_draw_string(lua_State* L) {
  const char* s = luaL_checkstring(L, 1);
  int32_t x = static_cast<int32_t>(luaL_checkinteger(L, 2));
  int32_t y = static_cast<int32_t>(luaL_checkinteger(L, 3));
  int32_t font = -1;
  if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) font = static_cast<int32_t>(luaL_checkinteger(L, 4));
  int32_t w = GfxService::drawString(s, x, y, font);
  lua_pushinteger(L, static_cast<lua_Integer>(w));
  return 1;
}

static const luaL_Reg kGfxLib[] = {
    {"clear", l_gfx_clear},
    {"setCursor", l_gfx_set_cursor},
    {"setTextSize", l_gfx_set_text_size},
    {"setTextColor", l_gfx_set_text_color},
    {"print", l_gfx_print},
    {"println", l_gfx_println},
    {"drawString", l_gfx_draw_string},
    {nullptr, nullptr},
};

int luaopen_gfx(lua_State* L) {
  luaL_newlib(L, kGfxLib);
  return 1;
}


