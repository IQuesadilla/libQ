#include "lua_clay.h"

#include "clay.h"

#include <apr_file_io.h>
#include <apr_strings.h>
#include <lauxlib.h>

struct lua_clay {
  apr_pool_t *pool, *rpool;
  lua_State *L;
  int do_draw_ref, window_ref;

  // char id_stack[256][256];
  apr_array_header_t *id_stack;
  int depth;
};

/*
 * ------ PRIVATE ------
 */

static int l_log(lua_State *L) {
  const char *msg = luaL_checkstring(L, 1);
  printf("[LOG] %s\n", msg);
  return 0;
}

static Clay_ElementId lc_buildid(lua_clay_t *lc, const char *name) {
  char **current_id = NULL;
  if (lc->depth >= lc->id_stack->nelts)
    current_id = &APR_ARRAY_PUSH(lc->id_stack, char *);
  else
    current_id = &APR_ARRAY_IDX(lc->id_stack, lc->depth, char *);

  if (lc->depth > 0) {
    char *parent_id = APR_ARRAY_IDX(lc->id_stack, lc->depth - 1, char *);
    *current_id = apr_pstrcat(lc->rpool, parent_id, ".", name, NULL);
  } else {
    *current_id = apr_pstrdup(lc->rpool, name);
  }

  Clay_String id = {
      .chars = *current_id,
      .length = strlen(*current_id),
      .isStaticallyAllocated = false,
  };
  return CLAY_SID(id);
}

static int l_window_item(lua_State *L) {
  lua_clay_t *lc = lua_touserdata(L, lua_upvalueindex(1));

  // argument 1 must be a table
  luaL_checktype(L, 1, LUA_TTABLE);

  Clay_ElementDeclaration el = {0};

  lua_getfield(L, 1, "layout");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "sizing");
    if (lua_istable(L, -1)) {
      lua_getfield(L, -1, "width");
      if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "type");
        el.layout.sizing.width.type = luaL_optinteger(L, -1, 0);
        lua_pop(L, 1);

        if (el.layout.sizing.width.type == CLAY__SIZING_TYPE_PERCENT) {
          lua_getfield(L, -1, "percent");
          el.layout.sizing.width.size.percent = luaL_optnumber(L, -1, 0);
          lua_pop(L, 1);
        } else {
          lua_getfield(L, -1, "min");
          el.layout.sizing.width.size.minMax.min = luaL_optnumber(L, -1, 0);
          lua_pop(L, 1);

          lua_getfield(L, -1, "max");
          el.layout.sizing.width.size.minMax.max = luaL_optnumber(L, -1, 0);
          lua_pop(L, 1);
        }
      }
      lua_pop(L, 1);

      lua_getfield(L, -1, "height");
      if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "type");
        el.layout.sizing.height.type = luaL_optinteger(L, -1, 0);
        lua_pop(L, 1);

        if (el.layout.sizing.height.type == CLAY__SIZING_TYPE_PERCENT) {
          lua_getfield(L, -1, "percent");
          el.layout.sizing.height.size.percent = luaL_optnumber(L, -1, 0);
          lua_pop(L, 1);
        } else {
          lua_getfield(L, -1, "min");
          el.layout.sizing.height.size.minMax.min = luaL_optnumber(L, -1, 0);
          lua_pop(L, 1);

          lua_getfield(L, -1, "max");
          el.layout.sizing.height.size.minMax.max = luaL_optnumber(L, -1, 0);
          lua_pop(L, 1);
        }
      }
      lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "layoutDirection");
    el.layout.layoutDirection = luaL_optnumber(L, -1, 0);
    lua_pop(L, 1);

    lua_getfield(L, -1, "padding");
    if (lua_istable(L, -1)) {
      lua_getfield(L, -1, "t");
      el.layout.padding.top = luaL_optinteger(L, -1, 0);
      lua_pop(L, 1);

      lua_getfield(L, -1, "b");
      el.layout.padding.bottom = luaL_optinteger(L, -1, 0);
      lua_pop(L, 1);

      lua_getfield(L, -1, "r");
      el.layout.padding.right = luaL_optinteger(L, -1, 0);
      lua_pop(L, 1);

      lua_getfield(L, -1, "l");
      el.layout.padding.left = luaL_optinteger(L, -1, 0);
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "cornerRadius");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "tl");
    el.cornerRadius.topLeft = luaL_optnumber(L, -1, 0);
    lua_pop(L, 1);

    lua_getfield(L, -1, "tr");
    el.cornerRadius.topRight = luaL_optnumber(L, -1, 0);
    lua_pop(L, 1);

    lua_getfield(L, -1, "bl");
    el.cornerRadius.bottomLeft = luaL_optnumber(L, -1, 0);
    lua_pop(L, 1);

    lua_getfield(L, -1, "br");
    el.cornerRadius.bottomRight = luaL_optnumber(L, -1, 0);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "backgroundColor");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "r");
    el.backgroundColor.r = luaL_optnumber(L, -1, 0);
    lua_pop(L, 1);

    lua_getfield(L, -1, "g");
    el.backgroundColor.g = luaL_optnumber(L, -1, 0);
    lua_pop(L, 1);

    lua_getfield(L, -1, "b");
    el.backgroundColor.b = luaL_optnumber(L, -1, 0);
    lua_pop(L, 1);

    lua_getfield(L, -1, "a");
    el.backgroundColor.a = luaL_optnumber(L, -1, 255);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "name");
  const char *name = luaL_checkstring(L, -1);
  lua_pop(L, 1);

  Clay_ElementId element_id = lc_buildid(lc, name);

  lc->depth += 1;

  // --- get drawchildren ---
  lua_getfield(L, 1, "drawchildren");

  CLAY(element_id, el) {
    if (lua_isfunction(L, -1)) {
      // --- call drawchildren immediately (no args, 1 return) ---
      if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "Lua error in drawchildren: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 0;
      }
    } else {
      lua_pop(L, 1);
    }
  }

  lc->depth -= 1;

  return 0; // no values returned to Lua
}

static int l_window_sizing_fixed(lua_State *L) {
  // Check that the first argument is an integer
  int param = luaL_checkinteger(L, 1);

  Clay_SizingAxis axis = CLAY_SIZING_FIXED(param);

  // Create a new table on the stack
  lua_newtable(L);

  lua_pushinteger(L, axis.type);
  lua_setfield(L, -2, "type");

  lua_pushnumber(L, axis.size.minMax.max);
  lua_setfield(L, -2, "max");

  lua_pushnumber(L, axis.size.minMax.min);
  lua_setfield(L, -2, "min");

  // The table is already on top of the stack, return 1 result
  return 1;
}

static int l_window_sizing_grow(lua_State *L) {
  // Check that the first argument is an integer
  int param = luaL_checkinteger(L, 1);

  Clay_SizingAxis axis = CLAY_SIZING_GROW(param);

  // Create a new table on the stack
  lua_newtable(L);

  lua_pushinteger(L, axis.type);
  lua_setfield(L, -2, "type");

  lua_pushnumber(L, axis.size.minMax.max);
  lua_setfield(L, -2, "max");

  lua_pushnumber(L, axis.size.minMax.min);
  lua_setfield(L, -2, "min");

  // The table is already on top of the stack, return 1 result
  return 1;
}

static int l_window_is_hovered(lua_State *L) {
  lua_clay_t *lc = lua_touserdata(L, lua_upvalueindex(1));

  const char *name = luaL_checkstring(L, 1);
  Clay_ElementId element_id = lc_buildid(lc, name);
  bool IsHovered = Clay_PointerOver(element_id);

  // lua_rawgeti(L, LUA_REGISTRYINDEX, lc->window_ref); // push window
  // lua_pushboolean(L, IsHovered);
  // lua_setfield(L, -2, "pointer_over");
  // lua_pop(L, 1);

  lua_pushboolean(L, IsHovered);

  return 1;
}

static int l_window_text(lua_State *L) {
  // Check that the first argument is an integer
  const char *text = luaL_checkstring(L, 1);

  Clay_String claytext = {
      .chars = text,
      .length = strlen(text),
      .isStaticallyAllocated = false,
  };

  CLAY_TEXT(claytext, CLAY_TEXT_CONFIG({
                          .fontId = 0,
                          .fontSize = 12,
                          .textColor = {255, 255, 255, 255},
                      }));

  // The table is already on top of the stack, return 1 result
  return 0;
}

/*
 * ------ PUBLIC ------
 */

void lua_clay_openlibs(lua_clay_t **newlc, lua_State *L, apr_pool_t *parent) {
  apr_pool_t *pool;
  apr_pool_create(&pool, parent);
  lua_clay_t *lc = apr_pcalloc(pool, sizeof(lua_clay_t));
  lc->pool = pool;

  apr_pool_create(&lc->rpool, lc->pool);

  /*
  // * Get package.preload *
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");

  // * preload["window"] = luaopen_app *
  lua_pushcfunction(L, luaopen_window);
  lua_setfield(L, -2, "window");

  // * now actually create the module *
  lua_pushcfunction(L, luaopen_window);
  lua_call(L, 0, 1); // returns module table

  lua_setglobal(L, "window");
  */

  lua_newtable(L); // window table

  // push window pointer as upvalue
  lua_pushlightuserdata(L, lc);
  lua_pushcclosure(L, l_window_item, 1);
  lua_setfield(L, -2, "item");

  lua_pushcclosure(L, l_window_sizing_fixed, 0);
  lua_setfield(L, -2, "sizing_fixed");

  lua_pushcclosure(L, l_window_sizing_grow, 0);
  lua_setfield(L, -2, "sizing_grow");

  lua_pushlightuserdata(L, lc);
  lua_pushcclosure(L, l_window_is_hovered, 1);
  lua_setfield(L, -2, "is_hovered");

  lua_pushlightuserdata(L, lc);
  lua_pushcclosure(L, l_window_text, 1);
  lua_setfield(L, -2, "text");

  lua_setglobal(L, "window");

  /* cleanup stack */
  // lua_pop(L, 2);

  apr_file_t *main_lua;
  apr_file_open(&main_lua, "./main.lua", APR_FOPEN_READ, APR_FPROT_OS_DEFAULT,
                lc->pool);

  apr_size_t nlua_cmd = 0;
  char lua_cmd[16384];
  apr_file_read_full(main_lua, lua_cmd, sizeof(lua_cmd), &nlua_cmd);
  lua_cmd[nlua_cmd] = '\0';
  apr_file_close(main_lua);

  if (luaL_dostring(L, lua_cmd) != LUA_OK) {
    fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
    return;
  }

  lua_getglobal(L, "window"); // push layout table
  lc->window_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_rawgeti(L, LUA_REGISTRYINDEX, lc->window_ref); // push window

  lua_pushboolean(L, 0);
  lua_setfield(L, -2, "mdown"); // pops bool, sets field

  lua_getfield(L, -1, "draw"); // push window.on_event

  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    fprintf(stderr, "window.draw not defined\n");
    return; // not defined
  }

  lc->do_draw_ref = luaL_ref(L, LUA_REGISTRYINDEX); // pops function
  lua_pop(L, 1);                                    // pop window table

  // cleanup: luaL_unref(L, LUA_REGISTRYINDEX, window->do_draw_ref);

  lc->L = L;
  *newlc = lc;
}

int lua_clay_relay(lua_clay_t *lc, bool mdown) {
  lua_State *L = lc->L;

  apr_pool_clear(lc->rpool);
  lc->id_stack = apr_array_make(lc->rpool, 16, sizeof(char *));

  lua_rawgeti(L, LUA_REGISTRYINDEX, lc->window_ref); // push window
  lua_pushboolean(L, mdown);
  lua_setfield(L, -2, "mdown");
  lua_pop(L, 1);

  // push function
  lua_rawgeti(L, LUA_REGISTRYINDEX, lc->do_draw_ref);

  if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
    fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    return -1;
  }

  lua_touserdata(L, lua_upvalueindex(1));

  return 0;
}
