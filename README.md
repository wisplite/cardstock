Cardstock is a lightweight Lua-based OS for the M5Stack Cardputer.

This repository contains all the C/C++ infrastructure and bindings required to support the Lua portion of the OS.

Currently supported modules are:
- M5GFX
- M5Canvas
- Keyboard

The eventual goal is for every API to be built and passed through to Lua, with certain things like wireless being controlled globally across apps.