#include <apr.h>
#include <apr_general.h>
#include <apr_pools.h>

#include <cglm/cglm.h>

#include <GLES2/gl2.h>

#include <lauxlib.h>
#include <lualib.h>

#include <qcam.h>
#include <qwindow.h>

#include <xkbcommon/xkbcommon.h>

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

  lua_State *L;
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

  bool redraw = app->doredraw;
  bool relay = /*newevents > 0 ||*/ app->dorelay || app->needs_redraw;

  Clay_RenderCommandArray render_commands = {0};
  if (relay) {
    // printf("relaying\n");
    // render_commands = ClayVideoDemo_CreateLayout(app, lmouse);
    redraw = true;

    Clay_BeginLayout();

    Clay_Sizing layoutExpand = {.width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_GROW(0)};
    Clay_Color contentBackgroundColor = {90, 90, 90, 255};

    lua_clay_relay(app->lc, lmouse);

    // char id[] = ;
    // Clay_String sid = {.chars = id, .length = strlen(id)};
    /*
    CLAY(CLAY_ID("OuterContainer"),
        {
            .backgroundColor = {30, 30, 30, 255},
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = layoutExpand,
                    .padding = CLAY_PADDING_ALL(0),
                    .childGap = 0,
                },
        }) {
        CLAY(CLAY_ID("HeaderBar"),
            {
                .layout =
                    {
                        .sizing =
                            {
                                .height = CLAY_SIZING_FIXED(30),
                                .width = CLAY_SIZING_GROW(0),
                            },
                        .padding = {4, 4, 0, 0},
                        .childGap = 16,
                        .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                    },
                .backgroundColor = contentBackgroundColor,
            }) {
        Clay_ElementId Button_el = CLAY_ID("FileButton");
        bool ButtonHover = Clay_PointerOver(Button_el);
        Clay_Color ButtonColor = ButtonHover ? (Clay_Color){255, 100, 100, 255}
                                                : (Clay_Color){100, 100, 100,
    255}; CLAY(Button_el, { .layout =
                                {
                                    .padding = {6, 6, 3, 3},
                                },
                            .backgroundColor = ButtonColor,
                            .cornerRadius = CLAY_CORNER_RADIUS(4),
                        }) {
            CLAY_TEXT(CLAY_STRING("Go"), CLAY_TEXT_CONFIG({
                                            .fontId = 0,
                                            .fontSize = 12,
                                            .textColor = {255, 255, 255, 255},
                                        }));
            if (ButtonHover && lmouse)
            fprintf(stderr, "here\n");
        }

        CLAY_TEXT(((Clay_String){
                        .chars = app->buffer,
                        .length = app->nbuffer,
                        .isStaticallyAllocated = true,
                    }),
                    CLAY_TEXT_CONFIG({
                        .fontId = 0,
                        .fontSize = 12,
                        .textColor = {255, 255, 255, 255},
                    }));
        }

        CLAY(CLAY_ID("TextBox"),
            {
                .layout =
                    {
                        .sizing =
                            {
                                .height = CLAY_SIZING_GROW(0),
                                .width = CLAY_SIZING_GROW(0),
                            },
                    },
            }) {
        lua_clay_relay(app->lc);
        }

        CLAY(CLAY_ID("BottomBar"),
            {
                .layout =
                    {
                        .sizing =
                            {
                                .height = CLAY_SIZING_FIXED(14),
                                .width = CLAY_SIZING_GROW(0),
                            },
                    },
                .backgroundColor = {70, 70, 70, 255},
            }) {
        int nstatusbar =
            snprintf(statusbar, sizeof(statusbar), "nchars: %d", app->nbuffer);
        CLAY_TEXT(((Clay_String){
                        .chars = statusbar,
                        .length = nstatusbar,
                        .isStaticallyAllocated = true,
                    }),
                    CLAY_TEXT_CONFIG({
                        .fontId = 0,
                        .fontSize = 12,
                        .textColor = {255, 255, 255, 255},
                    }));
        }
    }
    */

    render_commands = Clay_EndLayout();
    app->dorelay = false;
  }

  app->needs_redraw = true;
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

void resize(void *ud, int width, int height) {
  app_t *app = ud;
  app->width = width;
  app->height = height;
  app->aspect_ratio = qcam_set_view(&app->cam, width, height);

  Clay_SetLayoutDimensions((Clay_Dimensions){.width = width, .height = height});
  qlayout_renderer_resize(app->rend, width, height);

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

int main(int argc, const char *const argv[]) {
  apr_app_initialize(&argc, &argv, NULL);

  apr_pool_t *pool;
  apr_pool_create_core(&pool);

  apr_file_t *err;
  apr_file_open_stderr(&err, pool);

  apr_loop_t *loop;
  apr_event_setup(&loop, pool);

  app_t app = {
      .width = 640,
      .height = 480,
      .err = err,
      .dorelay = true,
      .doredraw = true,
      .lastframe = apr_time_now(),
      .L = luaL_newstate(),
  };

  luaL_openlibs(app.L);
  lua_clay_openlibs(&app.lc, app.L, pool);

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

  if (qlayout_renderer_init(&app.rend, pool, app.err) < 0) {
    apr_file_printf(app.err, "Failed Clay_GLRenderInit\n");
    exit(-1);
  }

  while (1) {
    qlayout_renderer_pre(app.rend);

    if (qwindow_pre(app.win))
      continue;

    apr_event_run(loop);
  }

  apr_terminate();
  return 0;
}
