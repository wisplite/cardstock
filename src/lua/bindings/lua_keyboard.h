#pragma once

#include "lua.hpp"

// Lua module entrypoint: local keyboard = require("keyboard")
int luaopen_keyboard(lua_State* L);