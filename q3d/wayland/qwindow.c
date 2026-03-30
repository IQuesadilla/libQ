#include "qwindow.h"

#include "xdg-shell-client-protocol.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <xkbcommon/xkbcommon.h>

#include <apr.h>
#include <apr_file_io.h>
#include <apr_portable.h>

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

struct qwindow {
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_surface *surface;

  struct wl_seat *seat;
  struct wl_pointer *pointer;
  struct wl_keyboard *keyboard;

  struct xdg_wm_base *wm_base;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *toplevel;

  struct xkb_context *xkb_ctx;
  struct xkb_keymap *keymap;
  struct xkb_state *xkb_state;

  struct wl_egl_window *egl_window;
  EGLDisplay egl_display;
  EGLContext egl_context;
  EGLSurface egl_surface;

  int wayland_fd;
  apr_file_t *wayland_file;
  apr_pool_t *pool;

  qwindow_interface_t i;

  bool cancel_this_frame;
  struct wl_callback *frame_cb;
  int can_render;

  bool configured;
};

/*
 * ------ PRIVATE ------
 */

static void pointer_enter(void *data, struct wl_pointer *pointer,
                          uint32_t serial, struct wl_surface *surface,
                          wl_fixed_t sx, wl_fixed_t sy) {
  qwindow_t *win = data;
  apr_file_printf(win->i.err, "Pointer entered at %.1f %.1f\n",
                  wl_fixed_to_double(sx), wl_fixed_to_double(sy));
}

static void pointer_leave(void *data, struct wl_pointer *pointer,
                          uint32_t serial, struct wl_surface *surface) {
  qwindow_t *win = data;
  apr_file_printf(win->i.err, "Pointer left\n");
}

static void pointer_motion(void *data, struct wl_pointer *pointer,
                           uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
  qwindow_t *win = data;
  // printf("Motion %.1f %.1f\n", wl_fixed_to_double(sx),
  // wl_fixed_to_double(sy));
  float x = wl_fixed_to_double(sx);
  float y = wl_fixed_to_double(sy);

  if (win->i.mouse_move)
    win->i.mouse_move(win->i.ud, x, y);
}

static void pointer_button(void *data, struct wl_pointer *pointer,
                           uint32_t serial, uint32_t time, uint32_t button,
                           uint32_t state) {
  qwindow_t *win = data;
  // apr_file_printf(win->i.err, "Button %u %s\n", button,
  // state == WL_POINTER_BUTTON_STATE_PRESSED ? "pressed"
  // : "released");
  win->i.mouse_down(win->i.ud, state == WL_POINTER_BUTTON_STATE_PRESSED);
}

static void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time,
                         uint32_t axis, wl_fixed_t value) {
  qwindow_t *win = data;
  apr_file_printf(win->i.err, "Scroll %d %.2f\n", axis,
                  wl_fixed_to_double(value));
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
};

static void keyboard_keymap(void *data, struct wl_keyboard *keyboard,
                            uint32_t format, int fd, uint32_t size) {
  qwindow_t *win = data;

  char *map = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
  win->keymap =
      xkb_keymap_new_from_string(win->xkb_ctx, map, XKB_KEYMAP_FORMAT_TEXT_V1,
                                 XKB_KEYMAP_COMPILE_NO_FLAGS);

  munmap(map, size);
  close(fd);

  win->xkb_state = xkb_state_new(win->keymap);
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard,
                           uint32_t serial, struct wl_surface *surface,
                           struct wl_array *keys) {
  qwindow_t *win = data;
  apr_file_printf(win->i.err, "Keyboard focus entered\n");
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard,
                           uint32_t serial, struct wl_surface *surface) {
  qwindow_t *win = data;
  apr_file_printf(win->i.err, "Keyboard focus left\n");
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard,
                         uint32_t serial, uint32_t time, uint32_t key,
                         uint32_t state) {
  qwindow_t *win = data;

  if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
    xkb_keysym_t sym = xkb_state_key_get_one_sym(win->xkb_state, key + 8);

    char name[64];
    xkb_keysym_get_name(sym, name, sizeof(name));
    apr_file_printf(win->i.err, "Key pressed: %s\n", name);
  }
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
                               uint32_t serial, uint32_t mods_depressed,
                               uint32_t mods_latched, uint32_t mods_locked,
                               uint32_t group) {
  qwindow_t *win = data;
  xkb_state_update_mask(win->xkb_state, mods_depressed, mods_latched,
                        mods_locked, 0, 0, group);
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
};

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
  qwindow_t *win = data;

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !win->pointer) {
    win->pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(win->pointer, &pointer_listener, win);
  }

  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !win->keyboard) {
    win->keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(win->keyboard, &keyboard_listener, win);
  }
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base,
                             uint32_t serial) {
  xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                  uint32_t serial) {
  qwindow_t *win = data;
  xdg_surface_ack_configure(xdg_surface, serial);
  win->configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                               int32_t width, int32_t height,
                               struct wl_array *states) {
  qwindow_t *win = data;

  // fprintf(stderr, "conf - w: %u, h: %u\n", width, height);
  if (width > 0 && height > 0) {
    win->i.width = width;
    win->i.height = height;
    wl_egl_window_resize(win->egl_window, width, height, 0, 0);

    if (win->i.resize)
      win->i.resize(win->i.ud, width, height);
  }
}

static void toplevel_configure_bounds(void *data, struct xdg_toplevel *toplevel,
                                      int32_t width, int32_t height) {
  qwindow_t *win = data;
  apr_file_printf(win->i.err, "conf bounds - w: %u, h: %u\n", width, height);
}
static void toplevel_close(void *data, struct xdg_toplevel *toplevel) {
  qwindow_t *win = data;
  apr_file_printf(win->i.err, "Closing\n");
  exit(0);
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure,
    .close = toplevel_close,
    .configure_bounds = toplevel_configure_bounds,
};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version) {
  qwindow_t *win = data;

  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    win->compositor =
        wl_registry_bind(registry, name, &wl_compositor_interface, 4);
  } else if (!strcmp(interface, wl_seat_interface.name)) {
    win->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);

    wl_seat_add_listener(win->seat, &seat_listener, win);

    win->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    win->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(win->wm_base, &wm_base_listener, win);
  }
}

static void registry_remove(void *data, struct wl_registry *registry,
                            uint32_t name) {
  (void)data;
  (void)registry;
  (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_remove,
};

static void frame_done(void *data, struct wl_callback *cb, uint32_t time) {
  qwindow_t *win = data;

  // fprintf(stderr, "done\n");

  wl_callback_destroy(cb);
  win->frame_cb = NULL;
  win->can_render = 1;
}

static const struct wl_callback_listener frame_listener = {
    .done = frame_done,
};

static void request_frame(qwindow_t *win) {
  // fprintf(stderr, "request\n");
  win->frame_cb = wl_surface_frame(win->surface);
  wl_callback_add_listener(win->frame_cb, &frame_listener, win);
}

static apr_status_t qwindow_event(apr_file_t *file, void *ud) {
  qwindow_t *win = ud;
  win->cancel_this_frame = false;

  wl_display_read_events(win->display);
  wl_display_dispatch_pending(win->display);

  if (win->can_render) {
    win->can_render = 0;

    request_frame(win);

    if (win->i.redraw)
      win->i.redraw(win->i.ud);
  }

  return APR_SUCCESS;
}

/*
 * ------ PUBLIC ------
 */

int qwindow_swap(qwindow_t *win) {
  eglSwapBuffers(win->egl_display, win->egl_surface);
  return 0;
}

void qwindow_make_current(qwindow_t *win) {
  eglMakeCurrent(win->egl_display, win->egl_surface, win->egl_surface,
                 win->egl_context);
}

int qwindow_init(qwindow_t **newwin, apr_pool_t *parent,
                 qwindow_interface_t *interface) {
  EGLint major, minor;
  EGLint config_attribs[] = {
      EGL_SURFACE_TYPE,
      EGL_WINDOW_BIT,
      EGL_RED_SIZE,
      8,
      EGL_GREEN_SIZE,
      8,
      EGL_BLUE_SIZE,
      8,
      EGL_DEPTH_SIZE,
      24,
      EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES2_BIT,
      EGL_NONE,
  };

  const EGLint ctx[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

  EGLConfig config;
  EGLint n;

  apr_pool_t *newpool;
  apr_pool_create(&newpool, parent);
  qwindow_t *win = apr_pcalloc(newpool, sizeof(*win));
  win->pool = newpool;
  win->i = *interface;

  win->display = wl_display_connect(NULL);
  if (!win->display) {
    apr_file_printf(win->i.err, "Failed to connect to Wayland\n");
    return 1;
  }

  win->registry = wl_display_get_registry(win->display);
  wl_registry_add_listener(win->registry, &registry_listener, win);
  wl_display_roundtrip(win->display);

  win->surface = wl_compositor_create_surface(win->compositor);

  win->xdg_surface = xdg_wm_base_get_xdg_surface(win->wm_base, win->surface);
  xdg_surface_add_listener(win->xdg_surface, &xdg_surface_listener, win);

  win->toplevel = xdg_surface_get_toplevel(win->xdg_surface);
  xdg_toplevel_add_listener(win->toplevel, &toplevel_listener, win);
  xdg_toplevel_set_title(win->toplevel, "qwindow");

  wl_surface_commit(win->surface);

  while (!win->configured)
    wl_display_dispatch(win->display);

  win->egl_display = eglGetDisplay((EGLNativeDisplayType)win->display);
  eglInitialize(win->egl_display, &major, &minor);

  eglChooseConfig(win->egl_display, config_attribs, &config, 1, &n);

  eglBindAPI(EGL_OPENGL_ES_API);

  win->egl_context =
      eglCreateContext(win->egl_display, config, EGL_NO_CONTEXT, ctx);

  win->egl_window =
      wl_egl_window_create(win->surface, win->i.width, win->i.height);

  win->egl_surface = eglCreateWindowSurface(win->egl_display, config,
                                            (uintptr_t)win->egl_window, NULL);
  qwindow_make_current(win);

  request_frame(win);
  eglSwapInterval(win->display, 0);
  glViewport(0, 0, win->i.width, win->i.height);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  qwindow_swap(win);
  eglSwapInterval(win->display, 1);

  win->wayland_fd = wl_display_get_fd(win->display);
  apr_os_file_put(&win->wayland_file, &win->wayland_fd, APR_FILE_NOCLEANUP,
                  win->pool);
  apr_event_add_file(win->i.loop, win->wayland_file, win->pool, APR_POLLIN,
                     qwindow_event, win);

  *newwin = win;

  return 0;
}

int qwindow_pre(qwindow_t *win) {
  if (win->cancel_this_frame) {
    wl_display_cancel_read(win->display);
  }

  wl_display_dispatch_pending(win->display);

  wl_display_flush(win->display);

  win->cancel_this_frame = true;

  if (wl_display_prepare_read(win->display) == -1) {
    wl_display_dispatch_pending(win->display);
    return 1;
  }

  return 0;
}
