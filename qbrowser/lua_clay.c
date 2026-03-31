#include "lua_clay.h"

#include <lauxlib.h>

/*
 * ------ PRIVATE ------
 */

static int l_log(lua_State *L) {
  const char *msg = luaL_checkstring(L, 1);
  printf("[LOG] %s\n", msg);
  return 0;
}

static int l_add(lua_State *L) {
  int a = luaL_checkinteger(L, 1);
  int b = luaL_checkinteger(L, 2);
  lua_pushinteger(L, a + b);
  return 1;
}

static const luaL_Reg layout_funcs[] = {
    {"log", l_log},
    {"add", l_add},
    {NULL, NULL},
};

static int luaopen_layout(lua_State *L) {
  luaL_newlib(L, layout_funcs);
  return 1;
}

/*
 * ------ PUBLIC ------
 */

void lua_clay_openlibs(lua_State *L) {
  /* Get package.preload */
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");

  /* preload["layout"] = luaopen_app */
  lua_pushcfunction(L, luaopen_layout);
  lua_setfield(L, -2, "layout");

  /* now actually create the module */
  lua_pushcfunction(L, luaopen_layout);
  lua_call(L, 0, 1); // returns module table

  lua_setglobal(L, "layout");

  /* cleanup stack */
  lua_pop(L, 2);
}
