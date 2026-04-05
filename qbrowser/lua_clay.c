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

  bool should_drag;
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

Clay_Color lc_getcolor(lua_State *L) {
  Clay_Color color;
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "r");
    color.r = luaL_optnumber(L, -1, 0);
    lua_pop(L, 1);

    lua_getfield(L, -1, "g");
    color.g = luaL_optnumber(L, -1, 0);
    lua_pop(L, 1);

    lua_getfield(L, -1, "b");
    color.b = luaL_optnumber(L, -1, 0);
    lua_pop(L, 1);

    lua_getfield(L, -1, "a");
    color.a = luaL_optnumber(L, -1, 255);
    lua_pop(L, 1);
  }
  return color;
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

    lua_getfield(L, -1, "childGap");
    el.layout.childGap = luaL_optinteger(L, -1, 0);
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
  el.backgroundColor = lc_getcolor(L);
  lua_pop(L, 1);

  lua_getfield(L, -1, "image");
  el.image.imageData = apr_pstrdup(lc->rpool, luaL_optstring(L, -1, NULL));
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
  lua_clay_t *lc = lua_touserdata(L, lua_upvalueindex(1));
  // Check that the first argument is an integer
  // const char *text = luaL_checkstring(L, 1);

  luaL_checktype(L, 1, LUA_TTABLE);
  lua_getfield(L, 1, "text");
  const char *text = apr_pstrdup(lc->rpool, luaL_optstring(L, -1, NULL));
  Clay_String claytext = {
      .chars = text,
      .length = strlen(text),
      .isStaticallyAllocated = false,
  };
  lua_pop(L, 1);

  Clay_TextElementConfig claytextcfg = {0};
  claytextcfg.fontId = 0;

  lua_getfield(L, 1, "fontSize");
  claytextcfg.fontSize = luaL_optinteger(L, -1, 12);
  lua_pop(L, 1);

  lua_getfield(L, 1, "textColor");
  claytextcfg.textColor = lc_getcolor(L);
  lua_pop(L, 1);

  CLAY_TEXT(claytext, CLAY_TEXT_CONFIG(claytextcfg));

  // The table is already on top of the stack, return 1 result
  return 0;
}

static int l_window_close(lua_State *L) {
  // Check that the first argument is an integer
  int rc = luaL_optinteger(L, 1, 0);
  exit(rc);

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
  lc->L = L;
  *newlc = lc;

  apr_pool_create(&lc->rpool, lc->pool);

  lua_newtable(lc->L); // window table
  lc->window_ref = luaL_ref(lc->L, LUA_REGISTRYINDEX);
  lua_rawgeti(lc->L, LUA_REGISTRYINDEX, lc->window_ref);
  // cleanup: luaL_unref(L, LUA_REGISTRYINDEX, window->do_draw_ref);

  lua_pushlightuserdata(lc->L, lc);
  lua_pushcclosure(lc->L, l_window_item, 1);
  lua_setfield(lc->L, -2, "item");

  lua_pushcclosure(lc->L, l_window_sizing_fixed, 0);
  lua_setfield(lc->L, -2, "sizing_fixed");

  lua_pushcclosure(lc->L, l_window_sizing_grow, 0);
  lua_setfield(lc->L, -2, "sizing_grow");

  lua_pushlightuserdata(lc->L, lc);
  lua_pushcclosure(lc->L, l_window_is_hovered, 1);
  lua_setfield(lc->L, -2, "is_hovered");

  lua_pushlightuserdata(lc->L, lc);
  lua_pushcclosure(lc->L, l_window_text, 1);
  lua_setfield(lc->L, -2, "text");

  lua_pushlightuserdata(lc->L, lc);
  lua_pushcclosure(lc->L, l_window_close, 1);
  lua_setfield(lc->L, -2, "close");

  lua_pushboolean(lc->L, 0);
  lua_setfield(lc->L, -2, "mdown"); // pops bool, sets field

  lua_setglobal(lc->L, "window");

  lc->do_draw_ref = LUA_REFNIL;
}

int lc_get_refs(lua_clay_t *lc) {
  lua_rawgeti(lc->L, LUA_REGISTRYINDEX, lc->window_ref); // push window
  lua_getfield(lc->L, -1, "draw");                       // push window.on_event

  if (!lua_isfunction(lc->L, -1)) {
    lua_pop(lc->L, 2);
    fprintf(stderr, "window.draw not defined\n");
    return -1; // not defined
  }

  lc->do_draw_ref = luaL_ref(lc->L, LUA_REGISTRYINDEX); // pops function
  lua_pop(lc->L, 1);                                    // pop window table
  return 0;
}

int lua_clay_relay(lua_clay_t *lc, bool mdown) {
  lua_State *L = lc->L;

  if (lc->do_draw_ref == LUA_REFNIL)
    return 0;

  apr_pool_clear(lc->rpool);
  lc->id_stack = apr_array_make(lc->rpool, 16, sizeof(char *));

  lc->should_drag = false;

  lua_rawgeti(L, LUA_REGISTRYINDEX, lc->window_ref); // push window
  lua_pushboolean(L, lc->should_drag);
  lua_setfield(L, -2, "drag");
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

  lua_rawgeti(L, LUA_REGISTRYINDEX, lc->window_ref); // push window
  lua_getfield(L, -1, "drag");
  lc->should_drag = lua_toboolean(L, -1);
  lua_pop(L, 1);

  return 0;
}

bool lua_clay_get_drag(lua_clay_t *lc) {
  return lc->should_drag; //
}
