#include <apr.h>
#include <apr_general.h>
#include <apr_pools.h>
#include <apr_strings.h>

#include <cglm/cglm.h>

#include <GLES2/gl2.h>

#include <lauxlib.h>
#include <lualib.h>

#include <qcam.h>
#include <qwindow.h>

#include <xkbcommon/xkbcommon.h>

#include "downloader.h"
#include "lua_clay.h"
#include "qlayout_renderer.h"

struct app {
  qwindow_t *win;
  qcam_t cam;

  int width, height;
  float aspect_ratio;

  float mpos_x, mpos_y;
  int mdown;

  uint64_t lastframe;

  uint64_t last_report, frames;
  apr_file_t *err;

  bool doredraw, dorelay, needs_redraw;
  qlayout_renderer_t *rend;

  char buffer[1024];
  int nbuffer;

  lua_State *L, *co;
  lua_clay_t *lc;
};
typedef struct app app_t;

static void init_egl(struct app *app) {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  qcam_init_default(&app->cam);
  app->cam.MovementSpeed = 0.01f;
  app->cam.BinarySensitivity = 4.f;
  app->aspect_ratio = qcam_set_view(&app->cam, app->width, app->height);
}

void redraw(void *ud) {
  app_t *app = ud;
  bool lmouse = app->mdown;
  char statusbar[256];
  // qwindow_make_current(app->win);

  bool relay = /*newevents > 0 ||*/ app->dorelay || app->needs_redraw;
  bool redraw = app->doredraw || relay;

  app->dorelay = false;
  app->needs_redraw = true;

  Clay_RenderCommandArray render_commands = {0};
  if (relay) {
    // printf("relaying\n");
    // render_commands = ClayVideoDemo_CreateLayout(app, lmouse);

    Clay_BeginLayout();
    lua_clay_relay(app->lc, lmouse);
    render_commands = Clay_EndLayout();
    qwindow_set_drag(app->win, lua_clay_get_drag(app->lc));
  }

  if (redraw) {
    // uint64_t now = apr_time_now();
    if (qlayout_renderer_clay(app->rend, &render_commands)) {
      qwindow_swap(app->win);
      // app->lastframe = now;

      app->doredraw = false;
      app->needs_redraw = false;
    }
  }

  app->frames++;

  uint64_t t = apr_time_now();
  uint64_t elapsed = t - app->last_report;
  if (elapsed >= 5000000) {
    uint64_t fps = (app->frames * 1000000) / elapsed;

    apr_file_printf(app->err, "FPS: %lu\n", fps);

    app->frames = 0;
    app->last_report = t;
  }
}

void try_redraw(app_t *app) {
  if (app->needs_redraw) {
    redraw(app);
  } else {
    app->dorelay = true;
  }
}

void resize(void *ud, int width, int height, float scaling) {
  app_t *app = ud;
  app->width = width;
  app->height = height;
  app->aspect_ratio = qcam_set_view(&app->cam, width, height);

  qlayout_renderer_resize(app->rend, width, height, scaling);

  try_redraw(app);
}

void mouse_move(void *ud, float x, float y) {
  app_t *app = ud;
  app->mpos_x = x;
  app->mpos_y = y;
  Clay_SetPointerState((Clay_Vector2){.x = app->mpos_x, .y = app->mpos_y},
                       app->mdown);
  try_redraw(app);
}

void mouse_down(void *ud, int down) {
  app_t *app = ud;
  app->mdown = down;
  Clay_SetPointerState((Clay_Vector2){.x = app->mpos_x, .y = app->mpos_y},
                       app->mdown);
  try_redraw(app);
}

void key_down(void *ud, uint32_t keysym) {
  app_t *app = ud;
  // apr_file_printf(app->err, "Key pressed: %u\n", keysym);

  switch (keysym) {
  case XKB_KEY_BackSpace:
    if (app->nbuffer > 0) {
      app->nbuffer -= 1;
      app->buffer[app->nbuffer] = '\0';
    }
    break;
  case XKB_KEY_Shift_L:
  case XKB_KEY_Shift_R:
    break;
  case XKB_KEY_Control_L:
  case XKB_KEY_Control_R:
    break;
  case XKB_KEY_Caps_Lock:
    break;
  case XKB_KEY_Return:
    break;
  default:
    if (app->nbuffer < sizeof(app->buffer)) {
      app->buffer[app->nbuffer] = (char)keysym;
      app->nbuffer += 1;
    }
    break;
  }

  try_redraw(app);
}

struct ReaderState {
  data_node_t *cur;
};

const char *reader(lua_State *L, void *data, size_t *size) {
  struct ReaderState *rs = data;

  if (!rs->cur) {
    *size = 0;
    return NULL;
  }

  const char *ptr = rs->cur->data;
  *size = rs->cur->used;

  rs->cur = rs->cur->next;
  return ptr;
}

struct download {
  const char *url;
  app_t *app;
};
typedef struct download download_t;

void download_handler(data_node_t *list, void *ud) {
  app_t *app = ud;

  struct ReaderState rs = {.cur = list};

  if (lua_load(app->co, reader, &rs, "stream", NULL) != LUA_OK) {
    fprintf(stderr, "load error: %s\n", lua_tostring(app->co, -1));
    return;
  }

  int nres = 0;
  int status = lua_resume(app->co, app->L, 0, &nres);
  if (status == LUA_YIELD) {
    return;
  } else if (status == LUA_OK) {
    lc_get_refs(app->lc);
    try_redraw(app);
  } else {
    printf("Error loading app.lua: %s\n", lua_tostring(app->co, -1));
  }
}

int main(int argc, const char *const argv[]) {
  apr_app_initialize(&argc, &argv, NULL);

  if (argc != 2) {
    fprintf(stderr, "Usage: %s url|ip:port\n", argv[0]);
    return 0;
  }

  apr_pool_t *pool;
  apr_pool_create_core(&pool);

  apr_file_t *err;
  apr_file_open_stderr(&err, pool);

  apr_loop_t *loop;
  apr_event_setup(&loop, pool);

  app_t app = {
      .width = 1920,
      .height = 1080,
      .err = err,
      .dorelay = true,
      .doredraw = true,
      .lastframe = apr_time_now(),
      .L = luaL_newstate(),
  };

  // luaL_openlibs(app.L);
  luaopen_base(app.L); // TODO: Reimplement
  luaopen_coroutine(app.L);
  luaopen_debug(app.L);
  // luaopen_io(app.L);
  luaopen_math(app.L);
  // luaopen_os(app.L);
  // luaopen_package(app.L);
  luaopen_string(app.L);
  luaopen_table(app.L);
  luaopen_utf8(app.L);

  lua_clay_openlibs(&app.lc, app.L, pool);

  app.co = lua_newthread(app.L);

  download_t *app_lua = apr_pcalloc(pool, sizeof(*app_lua));
  app_lua->url = apr_pstrcat(pool, "http://", argv[1], "/app.lua", NULL);
  start_download(app_lua->url, pool, loop, download_handler, &app);

  qwindow_interface_t interface = {
      .width = app.width,
      .height = app.height,
      .err = err,
      .loop = loop,
      .ud = &app,
      .redraw = redraw,
      .resize = resize,
      .mouse_move = mouse_move,
      .mouse_down = mouse_down,
      .key_down = key_down,
  };

  qwindow_init(&app.win, pool, &interface);
  init_egl(&app);

  if (qlayout_renderer_init(&app.rend, pool, loop, app.err) < 0) {
    apr_file_printf(app.err, "Failed Clay_GLRenderInit\n");
    exit(-1);
  }

  apr_event_run(loop);

  apr_terminate();
  return 0;
}
