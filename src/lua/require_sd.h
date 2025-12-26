#pragma once

// Custom SD-backed module loader ("require") for Cardstock.
//
// Installs a Lua `package.searcher` that loads modules from:
//   1) <app_root>/<module>.lua and <app_root>/<module>/init.lua
//   2) /syslib/<module>.lua and /syslib/<module>/init.lua
//
// The app root is set by the host (C++) per loaded entrypoint.

#include "lua.hpp"

// Install the Cardstock SD-backed searcher into `package.searchers`.
void lua_cardstock_install_require(lua_State* L);

// Set the current app root used by the searcher (absolute, e.g. "/apps/foo").
// Pass "" or nullptr to disable app-scoped lookup.
void lua_cardstock_set_app_root(lua_State* L, const char* app_root);


