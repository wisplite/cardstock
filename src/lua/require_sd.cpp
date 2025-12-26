#include "require_sd.h"

#include <Arduino.h>
#include <SD.h>

#include <new>
#include <memory>

namespace {

// Registry key for current app root.
static const char* kAppRootKey = "cardstock.app_root";
static const char* kSysLibRoot = "/syslib";

static String normalize_abs_path(const String& p) {
  if (!p.length()) return "";
  String out = p;
  if (out[0] != '/') out = String("/") + out;
  // Strip trailing slashes (except for "/")
  while (out.length() > 1 && out[out.length() - 1] == '/') out.remove(out.length() - 1);
  return out;
}

static bool contains_path_traversal(const char* s) {
  if (!s) return false;
  // Disallow any ".." path segment.
  // (Accepts module names like "foo..bar"? That would be weird; still blocked.)
  return strstr(s, "..") != nullptr;
}

static String module_name_to_rel_path(const char* modname) {
  // Convert dotted module name to path: "a.b.c" -> "a/b/c"
  String out;
  if (!modname) return out;
  for (const char* p = modname; *p; ++p) {
    char c = *p;
    if (c == '.') out += '/';
    else if (c == '\\') out += '/';
    else out += c;
  }
  // Strip any leading slashes to keep it relative.
  while (out.length() && out[0] == '/') out.remove(0, 1);
  return out;
}

static bool read_entire_file(const String& path, std::unique_ptr<uint8_t[]>& out_buf, size_t& out_len, String& out_err) {
  const String normalized = normalize_abs_path(path);
  File f = SD.open(normalized.c_str(), FILE_READ);
  if (!f) {
    out_err = "open failed: " + normalized;
    return false;
  }

  size_t sz = static_cast<size_t>(f.size());
  if (sz == 0) {
    out_err = "empty file: " + normalized;
    f.close();
    return false;
  }

  std::unique_ptr<uint8_t[]> buf(new (std::nothrow) uint8_t[sz]);
  if (!buf) {
    out_err = "OOM reading: " + normalized;
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
    out_err = "short read: " + normalized;
    return false;
  }

  out_buf = std::move(buf);
  out_len = sz;
  return true;
}

static String get_app_root(lua_State* L) {
  lua_pushstring(L, kAppRootKey);
  lua_gettable(L, LUA_REGISTRYINDEX);  // registry[kAppRootKey]
  const char* s = lua_tostring(L, -1);
  String out = s ? String(s) : String("");
  lua_pop(L, 1);
  return out;
}

static void push_searcher_error(lua_State* L, const String& msg) {
  // Match Lua's style: searchers return a string starting with "\n\t..."
  String m = "\n\t";
  m += msg;
  lua_pushstring(L, m.c_str());
}

static int l_cardstock_searcher(lua_State* L) {
  const char* modname = luaL_checkstring(L, 1);
  if (contains_path_traversal(modname)) {
    push_searcher_error(L, String("invalid module name: ") + modname);
    return 1;
  }

  const String rel = module_name_to_rel_path(modname);
  if (!rel.length()) {
    push_searcher_error(L, "invalid module name");
    return 1;
  }

  const String app_root = get_app_root(L);
  bool loaded = false;

  // Build candidates.
  String candidates[4];
  int n = 0;

  if (app_root.length()) {
    candidates[n++] = app_root + "/" + rel + ".lua";
    candidates[n++] = app_root + "/" + rel + "/init.lua";
  }

  candidates[n++] = String(kSysLibRoot) + "/" + rel + ".lua";
  candidates[n++] = String(kSysLibRoot) + "/" + rel + "/init.lua";

  // Try candidates in order.
  for (int i = 0; i < n; i++) {
    const String& p = candidates[i];
    String err;
    std::unique_ptr<uint8_t[]> buf;
    size_t len = 0;

    // Load full file into chunk.
    if (!read_entire_file(p, buf, len, err)) continue;

    int rc = luaL_loadbuffer(L, reinterpret_cast<const char*>(buf.get()), len, p.c_str());
    if (rc != LUA_OK) {
      const char* emsg = lua_tostring(L, -1);
      String m = "load error in ";
      m += p;
      m += ": ";
      m += emsg ? emsg : "(unknown)";
      lua_pop(L, 1);
      push_searcher_error(L, m);
      return 1;
    }

    lua_pushstring(L, normalize_abs_path(p).c_str());
    loaded = true;
    break;
  }

  if (loaded) return 2;

  // Not found. Provide a single aggregated message.
  String msg = "module '";
  msg += modname;
  msg += "' not found (searched ";
  if (app_root.length()) {
    msg += app_root;
    msg += " and ";
  }
  msg += kSysLibRoot;
  msg += ")";
  push_searcher_error(L, msg);
  return 1;
}

static void insert_searcher(lua_State* L) {
  // package.searchers is a table (Lua 5.2+).
  lua_getglobal(L, "package");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return;
  }

  lua_getfield(L, -1, "searchers");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 2);
    return;
  }

  // Insert at index 2 (after preload searcher).
  const int insert_at = 2;
  int len = static_cast<int>(luaL_len(L, -1));
  for (int i = len + 1; i > insert_at; --i) {
    lua_rawgeti(L, -1, i - 1);
    lua_rawseti(L, -2, i);
  }

  lua_pushcfunction(L, l_cardstock_searcher);
  lua_rawseti(L, -2, insert_at);

  lua_pop(L, 2);  // pop searchers, package
}

}  // namespace

void lua_cardstock_install_require(lua_State* L) {
  if (!L) return;
  insert_searcher(L);
}

void lua_cardstock_set_app_root(lua_State* L, const char* app_root) {
  if (!L) return;
  String root = app_root ? String(app_root) : String("");
  root = normalize_abs_path(root);
  lua_pushstring(L, kAppRootKey);
  lua_pushstring(L, root.c_str());
  lua_settable(L, LUA_REGISTRYINDEX);
}


