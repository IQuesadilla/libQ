#include <apr.h>
#include <apr_general.h>
#include <apr_getopt.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <apr_thread_proc.h>

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

  apr_loop_t *loop;

  int width, height;
  float aspect_ratio;

  float mpos_x, mpos_y;
  int mdown;

  uint64_t lastframe;

  uint64_t last_report, frames;
  apr_file_t *err;

  qlayout_renderer_t *rend;

  char buffer[1024];
  int nbuffer;

  lua_State *L, *co;
  lua_clay_t *lc;
};
typedef struct app app_t;

static void init_egl(struct app *app) {
  qcam_init_default(&app->cam);
  app->cam.MovementSpeed = 0.01f;
  app->cam.BinarySensitivity = 4.f;
  app->aspect_ratio = qcam_set_view(&app->cam, app->width, app->height);
}

void redraw(void *ud) {
  app_t *app = ud;
  bool lmouse = app->mdown;
  char statusbar[256];

  Clay_BeginLayout();
  lua_clay_relay(app->lc, lmouse);
  Clay_RenderCommandArray render_commands = Clay_EndLayout();

  qwindow_set_drag(app->win, lua_clay_get_drag(app->lc));

  if (qlayout_renderer_clay(app->rend, &render_commands)) {
    qwindow_swap(app->win);

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
}

void resize(void *ud, int width, int height, float scaling) {
  app_t *app = ud;
  app->width = width;
  app->height = height;
  app->aspect_ratio = qcam_set_view(&app->cam, width, height);
  printf("resize\n");

  qlayout_renderer_resize(app->rend, width, height, scaling);

  qwindow_queue_redraw(app->win);
}

void mouse_move(void *ud, float x, float y) {
  app_t *app = ud;
  app->mpos_x = x;
  app->mpos_y = y;
  Clay_SetPointerState((Clay_Vector2){.x = app->mpos_x, .y = app->mpos_y},
                       app->mdown);

  qwindow_queue_redraw(app->win);
}

void mouse_down(void *ud, int down) {
  app_t *app = ud;
  app->mdown = down;
  Clay_SetPointerState((Clay_Vector2){.x = app->mpos_x, .y = app->mpos_y},
                       app->mdown);

  qwindow_queue_redraw(app->win);
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

  qwindow_queue_redraw(app->win);
}

const char *data_node_reader(lua_State *L, void *data, size_t *size) {
  data_node_t **rs = data;
  data_node_t *cur = *rs;

  if (!cur) {
    *size = 0;
    return NULL;
  }

  const char *ptr = cur->data;
  *size = cur->used;

  *rs = cur->next;
  return ptr;
}

struct download {
  const char *url;
  app_t *app;
};
typedef struct download download_t;

void download_handler(data_node_t *list, void *ud) {
  app_t *app = ud;

  if (lua_load(app->co, data_node_reader, &list, "stream", NULL) != LUA_OK) {
    fprintf(stderr, "load error: %s\n", lua_tostring(app->co, -1));
    return;
  }

  int nres = 0;
  int status = lua_resume(app->co, app->L, 0, &nres);
  if (status == LUA_YIELD) {
    return;
  } else if (status == LUA_OK) {
    lc_get_refs(app->lc);
    qwindow_queue_redraw(app->win);
  } else {
    printf("Error loading app.lua: %s\n", lua_tostring(app->co, -1));
  }
}

typedef struct {
  apr_pool_t *pool;
  const char *filename;
  apr_file_t *pipe_write;
  data_node_t *head, *tail;
  apr_loop_t *loop;
  downloader_fn_t on_complete;
  void *ud;
} file_reader_t;

int file_reader_poll(const apr_pollfd_t *fd, void *ud) {
  file_reader_t *fl = ud;

  if (fl->tail->used >= CHUNK_SIZE) {
    data_node_t *new_node = apr_palloc(fd->p, sizeof(data_node_t));
    new_node->used = 0;
    new_node->next = NULL;
    fl->tail->next = new_node;
    fl->tail = new_node;
  }

  apr_size_t size = CHUNK_SIZE - fl->tail->used;
  apr_file_read(fd->desc.f, &fl->tail->data[fl->tail->used], &size);
  fl->tail->used += size;

  if (apr_file_eof(fd->desc.f)) {
    fl->on_complete(fl->head, fl->ud);
    apr_event_remove_pollfd(fl->loop, fd);
  }
  return 0;
}

static void *APR_THREAD_FUNC file_reader_thread(apr_thread_t *thd, void *data) {
  file_reader_t *ctx = (file_reader_t *)data;

  apr_file_t *file;
  apr_status_t rv;

  rv = apr_file_open(&file, ctx->filename, APR_READ, APR_OS_DEFAULT, ctx->pool);
  if (rv != APR_SUCCESS) {
    apr_file_close(ctx->pipe_write);
    return NULL;
  }

  char buf[CHUNK_SIZE];
  apr_size_t n;

  while (1) {
    n = sizeof(buf);
    rv = apr_file_read(file, buf, &n);

    if (rv == APR_EOF) {
      break;
    }

    if (rv != APR_SUCCESS) {
      break;
    }

    // write chunk into pipe
    apr_size_t written = n;
    apr_file_write(ctx->pipe_write, buf, &written);
  }

  apr_file_close(file);

  // optional: close write end to signal EOF to reader
  apr_file_close(ctx->pipe_write);

  return NULL;
}

void file_reader_create(file_reader_t **newfl, const char *path,
                        apr_loop_t *loop, apr_pool_t *pool,
                        downloader_fn_t on_complete, void *ud) {
  file_reader_t *fl = apr_pcalloc(pool, sizeof(*fl));
  fl->on_complete = on_complete;
  fl->ud = ud;
  fl->head = fl->tail = apr_palloc(pool, sizeof(data_node_t));
  fl->tail->used = 0;
  fl->tail->next = NULL;
  fl->pool = pool;
  fl->loop = loop;
  fl->filename = apr_pstrdup(pool, path);

  apr_file_t *pipe_read;
  apr_file_pipe_create(&pipe_read, &fl->pipe_write, pool);

  apr_thread_t *t;
  apr_thread_create(&t, NULL, file_reader_thread, fl, pool);

  apr_event_add_file(loop, pipe_read, pool, APR_POLLIN, file_reader_poll, fl);

  *newfl = fl;
}

static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize; /* not used */
  if (nsize == 0) {
    free(ptr);
    return NULL;
  } else
    return realloc(ptr, nsize);
}

static const apr_getopt_option_t options[] = {
    {"help", 'h', 0, "Display this help"},
    {NULL, 'V', 0, "Display version and exit"},
    {"file", 'f', 1, "<path> Load file from disk"},
    {"url", 'u', 1, "<url> Load file from the web"},
    {NULL, 0},
};

static void print_usage(apr_file_t *err) {
  int i = 0;

  apr_file_printf(err, "qbrowser [options]\n");
  apr_file_printf(err, "Options:");

  while (options[i].optch > 0) {
    const apr_getopt_option_t *o = &options[i];

    if (o->optch <= 255) {
      apr_file_printf(err, " -%c", o->optch);
      if (o->name)
        apr_file_printf(err, ", ");
    } else {
      apr_file_printf(err, "     ");
    }

    apr_file_printf(err, "%s%s\t%s\n", o->name ? "--" : "\t",
                    o->name ? o->name : "", o->description);

    i++;
  }
}

int main(int argc, const char *const argv[]) {
  apr_app_initialize(&argc, &argv, NULL);

  apr_status_t s = APR_SUCCESS;

  apr_pool_t *pool;
  apr_pool_create_core(&pool);

  app_t app = {
      .width = 700,
      .height = 500,
  };

  apr_file_open_stderr(&app.err, pool);

  apr_event_setup(&app.loop, pool);

  bool loaded = false;
  int opt_c;
  const char *opt_arg;
  apr_getopt_t *opt;
  apr_getopt_init(&opt, pool, argc, argv);
  while ((s = apr_getopt_long(opt, options, &opt_c, &opt_arg)) == APR_SUCCESS) {
    switch (opt_c) {
    case 'h':
      print_usage(app.err);
      exit(0);
      break;
    case 'f': {
      file_reader_t *fl;
      file_reader_create(&fl, opt_arg, app.loop, pool, download_handler, &app);

      loaded = true;
    } break;
    case 'u': {
      download_t *app_lua = apr_pcalloc(pool, sizeof(*app_lua));
      app_lua->url = apr_pstrcat(pool, "http://", opt_arg, "/app.lua", NULL);
      start_download(app_lua->url, pool, app.loop, download_handler, &app);
      loaded = true;
    } break;
    }
  }

  if (!loaded) {
    print_usage(app.err);
    return 0;
  }

  app.lastframe = apr_time_now();
  app.L = lua_newstate(l_alloc, NULL);

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

  qwindow_interface_t interface = {
      .width = app.width,
      .height = app.height,
      .err = app.err,
      .loop = app.loop,
      .ud = &app,
      .redraw = redraw,
      .resize = resize,
      .mouse_move = mouse_move,
      .mouse_down = mouse_down,
      .key_down = key_down,
  };

  qwindow_init(&app.win, pool, &interface);
  init_egl(&app);

  if (qlayout_renderer_init(&app.rend, pool, app.loop, app.err) < 0) {
    apr_file_printf(app.err, "Failed Clay_GLRenderInit\n");
    exit(-1);
  }

  apr_event_run(app.loop);

  apr_terminate();
  return 0;
}
