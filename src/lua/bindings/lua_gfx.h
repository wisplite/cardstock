#pragma once

// C++ convenience header that wraps the Lua C headers in 'extern "C"'.
#include "lua.hpp"

// Lua module entrypoint: local gfx = require("gfx")
int luaopen_gfx(lua_State* L);


