#include <Arduino.h>
// In some C++ modes/toolchains, LLONG_MAX/ULLONG_MAX are not exposed unless
// <climits> is included (Lua uses LLONG_MAX as a proxy for long long support).
#include <climits>
#include <memory>
#include <new>

// Last-resort fallback: some strict modes may still not define LLONG_MAX.
// GCC/Clang typically provide __LONG_LONG_MAX__.
#if !defined(LLONG_MAX) && defined(__LONG_LONG_MAX__)
#define LLONG_MAX __LONG_LONG_MAX__
#define LLONG_MIN (-LLONG_MAX - 1LL)
#endif
#if !defined(ULLONG_MAX) && defined(LLONG_MAX)
#define ULLONG_MAX (static_cast<unsigned long long>(LLONG_MAX) * 2ULL + 1ULL)
#endif

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <SPI.h>
#include <SD.h>

#include "M5Cardputer.h"
#include "lua/bindings/lua_gfx.h"

// -------------------------------
// Build-time configuration knobs
// -------------------------------
// Default Lua entrypoint on SD. Override with: -DCARDSTOCK_LUA_ENTRY=\"/my.lua\"
#ifndef CARDSTOCK_LUA_ENTRY
#define CARDSTOCK_LUA_ENTRY "/app.lua"
#endif

// SD init pins. If you don't define these, the code falls back to SD.begin(CS).
// Override with (example):
//   -DCARDSTOCK_SD_CS=4 -DCARDSTOCK_SD_SCK=40 -DCARDSTOCK_SD_MISO=39 -DCARDSTOCK_SD_MOSI=41
#ifndef CARDSTOCK_SD_CS
#define CARDSTOCK_SD_CS SS
#endif

// SPI frequency for SD (Hz). Many cards are happy at 20-40MHz; drop if unstable.
#ifndef CARDSTOCK_SD_FREQ_HZ
#define CARDSTOCK_SD_FREQ_HZ 20000000u
#endif

// -------------------------------
// Tiny Lua host runtime
// -------------------------------

struct LuaHost {
  lua_State* L = nullptr;
  String current_path;
  String pending_path;
  bool reload_requested = false;
  uint32_t last_ms = 0;
};

static LuaHost g_host;

static void* host_from_lua(lua_State* L) {
  // Store LuaHost* in the Lua "extra space" (Lua 5.4 feature).
  void** p = reinterpret_cast<void**>(lua_getextraspace(L));
  return p ? *p : nullptr;
}

static void host_set_for_lua(lua_State* L, LuaHost* host) {
  void** p = reinterpret_cast<void**>(lua_getextraspace(L));
  if (p) *p = host;
}

static void ui_status(const String& line1, const String& line2 = "") {
  // Simple status overlay for early bring-up.
  M5Cardputer.Display.fillScreen(BLACK);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(0, 0);
  M5Cardputer.Display.setTextColor(WHITE, BLACK);
  M5Cardputer.Display.println(line1);
  if (line2.length()) M5Cardputer.Display.println(line2);
}

static void log_line(const String& s) {
  Serial.println(s);
}

static bool init_sd_card() {
#if defined(CARDSTOCK_SD_SCK) && defined(CARDSTOCK_SD_MISO) && defined(CARDSTOCK_SD_MOSI)
  SPI.begin(CARDSTOCK_SD_SCK, CARDSTOCK_SD_MISO, CARDSTOCK_SD_MOSI, CARDSTOCK_SD_CS);
  if (!SD.begin(CARDSTOCK_SD_CS, SPI, CARDSTOCK_SD_FREQ_HZ)) {
    return false;
  }
  return true;
#else
  // Fall back to default SPI pins for the selected board definition.
  return SD.begin(CARDSTOCK_SD_CS);
#endif
}

static bool read_entire_file_from_sd(const String& path, std::unique_ptr<uint8_t[]>& out_buf, size_t& out_len, String& out_err) {
  File f = SD.open(path.c_str(), FILE_READ);
  if (!f) {
    out_err = "SD.open failed: " + path;
    return false;
  }

  size_t sz = static_cast<size_t>(f.size());
  if (sz == 0) {
    out_err = "Empty file: " + path;
    f.close();
    return false;
  }

  std::unique_ptr<uint8_t[]> buf(new (std::nothrow) uint8_t[sz]);
  if (!buf) {
    out_err = "OOM reading: " + path;
    f.close();
    return false;
  }

  size_t read_total = 0;
  while (read_total < sz) {
    int n = f.read(buf.get() + read_total, sz - read_total);
    if (n <= 0) break;
    read_total += static_cast<size_t>(n);
  }
  f.close();

  if (read_total != sz) {
    out_err = "Short read: " + path;
    return false;
  }

  out_buf = std::move(buf);
  out_len = sz;
  return true;
}

static int l_print_serial(lua_State* L) {
  int n = lua_gettop(L);
  String line;
  for (int i = 1; i <= n; i++) {
    size_t len = 0;
    const char* s = luaL_tolstring(L, i, &len);  // pushes string
    if (i > 1) line += "\t";
    if (s && len) line += String(s);
    lua_pop(L, 1);  // pop tostring result
  }
  Serial.println(line);
  return 0;
}

static int l_switch_app(lua_State* L) {
  const char* p = luaL_checkstring(L, 1);
  LuaHost* host = reinterpret_cast<LuaHost*>(host_from_lua(L));
  if (!host) return 0;

  host->pending_path = String(p);
  host->reload_requested = true;
  return 0;
}

static void lua_report_top_error(lua_State* L, const char* prefix) {
  const char* msg = lua_tostring(L, -1);
  String s = prefix;
  s += msg ? msg : "(unknown lua error)";
  log_line(s);
  ui_status("Lua error", s);
  lua_pop(L, 1);
}

static bool lua_call_optional(lua_State* L, const char* fn_name, int nargs, int nresults) {
  // Stack on entry: ... [args already pushed if nargs>0]
  // We'll move them to after the function if needed.
  lua_getglobal(L, fn_name);
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);  // not a function
    if (nargs) lua_pop(L, nargs);  // drop args if caller pushed them
    return true;
  }

  if (nargs > 0) {
    // Function is on top; args are below it. Rotate so stack is ... func arg1..argN
    lua_insert(L, -(nargs + 1));
  }

  if (lua_pcall(L, nargs, nresults, 0) != LUA_OK) {
    String prefix = "pcall ";
    prefix += fn_name;
    prefix += ": ";
    lua_report_top_error(L, prefix.c_str());
    return false;
  }

  return true;
}

static void lua_close_state(LuaHost& host) {
  if (host.L) {
    lua_close(host.L);
    host.L = nullptr;
  }
}

static bool lua_boot_and_load(LuaHost& host, const String& script_path) {
  lua_close_state(host);

  host.L = luaL_newstate();
  if (!host.L) {
    ui_status("Lua", "luaL_newstate failed");
    log_line("luaL_newstate failed");
    return false;
  }

  host_set_for_lua(host.L, &host);
  luaL_openlibs(host.L);

  // Register built-in modules (Lua: local gfx = require("gfx")).
  luaL_requiref(host.L, "gfx", luaopen_gfx, 1);
  lua_pop(host.L, 1);  // pop returned module table

  // Override print() to go to Serial (handy on embedded).
  lua_pushcfunction(host.L, l_print_serial);
  lua_setglobal(host.L, "print");

  // Expose application switching API.
  lua_pushcfunction(host.L, l_switch_app);
  lua_setglobal(host.L, "switch_app");

  std::unique_ptr<uint8_t[]> buf;
  size_t len = 0;
  String err;
  if (!read_entire_file_from_sd(script_path, buf, len, err)) {
    ui_status("SD read failed", err);
    log_line(err);
    return false;
  }

  int rc = luaL_loadbuffer(host.L, reinterpret_cast<const char*>(buf.get()), len, script_path.c_str());
  if (rc != LUA_OK) {
    lua_report_top_error(host.L, "load: ");
    return false;
  }

  if (lua_pcall(host.L, 0, 0, 0) != LUA_OK) {
    lua_report_top_error(host.L, "run: ");
    return false;
  }

  host.current_path = script_path;
  host.pending_path = "";
  host.reload_requested = false;

  ui_status("Lua loaded", host.current_path);

  // Call init() once if present.
  if (!lua_call_optional(host.L, "init", 0, 0)) return false;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(50);

  auto cfg = M5.config();
  M5Cardputer.begin(cfg);
  M5Cardputer.Display.setRotation(1);
  ui_status("cardstock", "booting...");

  if (!init_sd_card()) {
    ui_status("SD init failed",
              "Define CARDSTOCK_SD_* pins or check wiring");
    log_line("SD init failed. If you're on Cardputer, define CARDSTOCK_SD_CS/SCK/MISO/MOSI in build_flags.");
    return;
  }

  ui_status("SD OK", String("Loading ") + CARDSTOCK_LUA_ENTRY);
  g_host.last_ms = millis();
  lua_boot_and_load(g_host, String(CARDSTOCK_LUA_ENTRY));
}

void loop() {
  M5Cardputer.update();

  // If SD failed in setup, nothing to do.
  if (!g_host.L) {
    delay(250);
    return;
  }

  uint32_t now = millis();
  float dt = (now - g_host.last_ms) / 1000.0f;
  g_host.last_ms = now;

  // tick(dt)
  lua_pushnumber(g_host.L, dt);
  if (!lua_call_optional(g_host.L, "tick", 1, 0)) {
    // On error, keep Lua alive so user can see the message. (They can switch apps via reset.)
    delay(250);
    return;
  }

  // draw()
  if (!lua_call_optional(g_host.L, "draw", 0, 0)) {
    delay(250);
    return;
  }

  // If Lua requested a new script, reload cleanly between frames.
  if (g_host.reload_requested) {
    String next = g_host.pending_path;
    if (!next.length()) {
      // If someone passed "", just ignore.
      g_host.reload_requested = false;
    } else {
      ui_status("Switching to", next);
      log_line(String("switch_app -> ") + next);
      lua_boot_and_load(g_host, next);
      // Note: lua_boot_and_load updates g_host fields.
    }
  }

  // Keep loop responsive; adjust later if you add a fixed framerate.
  delay(1);
}


