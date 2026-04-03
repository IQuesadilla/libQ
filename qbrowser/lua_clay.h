#ifndef LIBQ_LUA_CLAY_H
#define LIBQ_LUA_CLAY_H

#include <apr_pools.h>
#include <lua.h>
#include <stdbool.h>

typedef struct lua_clay lua_clay_t;

void lua_clay_openlibs(lua_clay_t **newlc, lua_State *L, apr_pool_t *parent);
int lc_get_refs(lua_clay_t *lc);

int lua_clay_relay(lua_clay_t *lc, bool mdown);

#endif
