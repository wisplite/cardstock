#include "lua_gfx.h"

#include "services/GfxService.h"
#include "M5Cardputer.h"

// -------------------------------
// gfx.sprite userdata (M5Canvas)
// -------------------------------

static const char* kSpriteMT = "gfx.sprite";

struct LuaSprite {
  M5Canvas* canvas = nullptr;
};

static LuaSprite* lua_check_sprite(lua_State* L, int idx) {
  return static_cast<LuaSprite*>(luaL_checkudata(L, idx, kSpriteMT));
}

static M5Canvas* lua_sprite_require_alive(lua_State* L, LuaSprite* s) {
  if (!s || !s->canvas) luaL_error(L, "sprite is freed");
  return s->canvas;
}

static void lua_sprite_free(LuaSprite* s) {
  if (!s || !s->canvas) return;
  s->canvas->deleteSprite();
  delete s->canvas;
  s->canvas = nullptr;
}

static uint16_t lua_check_u16(lua_State* L, int idx) {
  lua_Integer v = luaL_checkinteger(L, idx);
  if (v < 0) v = 0;
  if (v > 0xFFFF) v = 0xFFFF;
  return static_cast<uint16_t>(v);
}

static int l_sprite_gc(lua_State* L) {
  LuaSprite* s = lua_check_sprite(L, 1);
  lua_sprite_free(s);
  return 0;
}

static int l_sprite_free(lua_State* L) {
  LuaSprite* s = lua_check_sprite(L, 1);
  lua_sprite_free(s);
  return 0;
}

static int l_sprite_clear(lua_State* L) {
  LuaSprite* s = lua_check_sprite(L, 1);
  M5Canvas* c = lua_sprite_require_alive(L, s);
  uint16_t color = 0x0000;
  if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) color = lua_check_u16(L, 2);
  c->fillScreen(color);
  return 0;
}

static int l_sprite_set_text_color(lua_State* L) {
  LuaSprite* s = lua_check_sprite(L, 1);
  M5Canvas* c = lua_sprite_require_alive(L, s);
  uint16_t fg = lua_check_u16(L, 2);
  if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
    uint16_t bg = lua_check_u16(L, 3);
    c->setTextColor(fg, bg);
  } else {
    c->setTextColor(fg);
  }
  return 0;
}

static int l_sprite_set_text_size(lua_State* L) {
  LuaSprite* s = lua_check_sprite(L, 1);
  M5Canvas* c = lua_sprite_require_alive(L, s);
  lua_Integer size = luaL_checkinteger(L, 2);
  if (size < 1) size = 1;
  if (size > 255) size = 255;
  c->setTextSize(static_cast<uint8_t>(size));
  return 0;
}

static int l_sprite_draw_string(lua_State* L) {
  LuaSprite* s = lua_check_sprite(L, 1);
  M5Canvas* c = lua_sprite_require_alive(L, s);
  const char* text = luaL_checkstring(L, 2);
  int32_t x = static_cast<int32_t>(luaL_checkinteger(L, 3));
  int32_t y = static_cast<int32_t>(luaL_checkinteger(L, 4));
  int32_t w = c->drawString(text, x, y);
  lua_pushinteger(L, static_cast<lua_Integer>(w));
  return 1;
}

static int l_sprite_draw_center_string(lua_State* L) {
  LuaSprite* s = lua_check_sprite(L, 1);
  M5Canvas* c = lua_sprite_require_alive(L, s);
  const char* text = luaL_checkstring(L, 2);
  int32_t x = static_cast<int32_t>(luaL_checkinteger(L, 3));
  int32_t y = static_cast<int32_t>(luaL_checkinteger(L, 4));
  int32_t w = c->drawCenterString(text, x, y);
  lua_pushinteger(L, static_cast<lua_Integer>(w));
  return 1;
}

static int l_sprite_push(lua_State* L) {
  LuaSprite* s = lua_check_sprite(L, 1);
  M5Canvas* c = lua_sprite_require_alive(L, s);
  int32_t x = static_cast<int32_t>(luaL_checkinteger(L, 2));
  int32_t y = static_cast<int32_t>(luaL_checkinteger(L, 3));
  c->pushSprite(x, y);
  return 0;
}

static const luaL_Reg kSpriteMethods[] = {
    {"clear", l_sprite_clear},
    {"setTextColor", l_sprite_set_text_color},
    {"setTextSize", l_sprite_set_text_size},
    {"drawString", l_sprite_draw_string},
    {"drawCenterString", l_sprite_draw_center_string},
    {"push", l_sprite_push},
    {"free", l_sprite_free},
    {nullptr, nullptr},
};

static int l_gfx_new_sprite(lua_State* L) {
  int32_t w = static_cast<int32_t>(luaL_checkinteger(L, 1));
  int32_t h = static_cast<int32_t>(luaL_checkinteger(L, 2));
  if (w <= 0 || h <= 0) luaL_error(L, "newSprite: width and height must be > 0");

  LuaSprite* ud = static_cast<LuaSprite*>(lua_newuserdatauv(L, sizeof(LuaSprite), 0));
  new (ud) LuaSprite();

  // Create canvas (parented to the real display).
  ud->canvas = new (std::nothrow) M5Canvas(&M5Cardputer.Display);
  if (!ud->canvas) luaL_error(L, "newSprite: OOM allocating canvas");

  bool ok = ud->canvas->createSprite(w, h);
  if (!ok) {
    lua_sprite_free(ud);
    luaL_error(L, "newSprite: createSprite failed");
  }

  luaL_setmetatable(L, kSpriteMT);
  return 1;
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

static int l_gfx_fill_rect(lua_State* L) {
  int32_t x = static_cast<int32_t>(luaL_checkinteger(L, 1));
  int32_t y = static_cast<int32_t>(luaL_checkinteger(L, 2));
  int32_t w = static_cast<int32_t>(luaL_checkinteger(L, 3));
  int32_t h = static_cast<int32_t>(luaL_checkinteger(L, 4));
  uint16_t color = lua_check_u16(L, 5);
  GfxService::fillRect(x, y, w, h, color);
  return 0;
}

static int l_gfx_draw_center_string(lua_State* L) {
  const char* s = luaL_checkstring(L, 1);
  int32_t x = static_cast<int32_t>(luaL_checkinteger(L, 2));
  int32_t y = static_cast<int32_t>(luaL_checkinteger(L, 3));
  int32_t font = -1;
  if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) font = static_cast<int32_t>(luaL_checkinteger(L, 4));
  int32_t w = GfxService::drawCenterString(s, x, y, font);
  lua_pushinteger(L, static_cast<lua_Integer>(w));
  return 1;
}

static int l_gfx_width(lua_State* L) {
  int32_t w = GfxService::width();
  lua_pushinteger(L, static_cast<lua_Integer>(w));
  return 1;
}

static int l_gfx_height(lua_State* L) {
  int32_t h = GfxService::height();
  lua_pushinteger(L, static_cast<lua_Integer>(h));
  return 1;
}

static const luaL_Reg kGfxLib[] = {
    {"newSprite", l_gfx_new_sprite},
    {"clear", l_gfx_clear},
    {"setCursor", l_gfx_set_cursor},
    {"setTextSize", l_gfx_set_text_size},
    {"setTextColor", l_gfx_set_text_color},
    {"print", l_gfx_print},
    {"println", l_gfx_println},
    {"drawString", l_gfx_draw_string},
    {"fillRect", l_gfx_fill_rect},
    {"drawCenterString", l_gfx_draw_center_string},
    {"width", l_gfx_width},
    {"height", l_gfx_height},
    {nullptr, nullptr},
};

int luaopen_gfx(lua_State* L) {
  // Create gfx.sprite metatable.
  if (luaL_newmetatable(L, kSpriteMT)) {
    // metatable.__index = methods table
    luaL_newlib(L, kSpriteMethods);
    lua_setfield(L, -2, "__index");

    // metatable.__gc = l_sprite_gc
    lua_pushcfunction(L, l_sprite_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_pop(L, 1);  // pop metatable

  luaL_newlib(L, kGfxLib);
  return 1;
}


