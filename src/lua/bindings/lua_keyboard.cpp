#include "lua_keyboard.h"

#include "services/KeyboardService.h"

static int l_keyboard_is_changed(lua_State* L) {
    lua_pushboolean(L, KeyboardService::isChanged());
    return 1;
}

static int l_keyboard_is_pressed(lua_State* L) {
    lua_pushinteger(L, KeyboardService::isPressed());
    return 1;
}

static int l_keyboard_is_key_pressed(lua_State* L) {
    const char* c = luaL_checkstring(L, 1);
    lua_pushboolean(L, KeyboardService::isKeyPressed(c[0]));
    return 1;
}

static int l_keyboard_get_key(lua_State* L) {
    int32_t x = static_cast<int32_t>(luaL_checkinteger(L, 1));
    int32_t y = static_cast<int32_t>(luaL_checkinteger(L, 2));
    lua_pushinteger(L, KeyboardService::getKey(x, y));
    return 1;
}

static const luaL_Reg kKeyboardLib[] = {
    {"isChanged", l_keyboard_is_changed},
    {"isPressed", l_keyboard_is_pressed},
    {"isKeyPressed", l_keyboard_is_key_pressed},
    {"getKey", l_keyboard_get_key},
    {nullptr, nullptr},
};

int luaopen_keyboard(lua_State* L) {
    luaL_newlib(L, kKeyboardLib);
    return 1;
}