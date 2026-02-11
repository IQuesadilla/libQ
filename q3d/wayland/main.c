#define _POSIX_C_SOURCE 200112L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <cglm/cglm.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <xkbcommon/xkbcommon.h>

#include <qcam.h>

#include "xdg-shell-client-protocol.h"

static const char basic_colored_vert[] = "\
attribute vec3 aPos; \n\
attribute vec3 aColor; \n\
\
uniform mat4 model; \n\
uniform mat4 view; \n\
uniform mat4 projection; \n\
\
varying vec3 fragColor; \n\
\
void main() { \n\
  gl_Position = projection * view * model * vec4(aPos, 1.0); \n\
  fragColor = aColor; \n\
} \n\
";

static const char basic_colored_frag[] = "\
precision mediump float; \n\
\
varying vec3 fragColor; \n\
\
void main() { \n\
  gl_FragColor = vec4(fragColor, 1.0); \n\
} \n\
";

static const char solid_vert_glsl[] = "\
attribute vec2 inPosition; \n\
\
uniform mediump vec4 inRect; \n\
uniform vec2 screen; \n\
\
varying vec2 fragPos; \n\
\
void main() { \n\
    float x = inRect.x + inRect.p * inPosition.x; \n\
    float y = inRect.y + inRect.q * inPosition.y; \n\
    fragPos = vec2(x, y); \n\
    float glx = x / screen.x; \n\
    float gly = y / screen.y; \n\
    gl_Position = vec4((glx * 2.0) - 1.0, ((gly * 2.0) - 1.0) * -1.0, 0.0, 1.0); \n\
}\n";

static const char solid_frag_glsl[] = "\
precision mediump float; \n\
\
uniform vec4 inRect; \n\
uniform vec4 fragColor; \n\
uniform float cornerRadius; \n\
\
varying vec2 fragPos; \n\
\
void main() { \n\
    vec2 p = fragPos - (inRect.xy + inRect.zw * 0.5);  // center the rectangle \n\
    vec2 halfSize = inRect.zw * 0.5; \n\
\
    // Distance to box with rounded corners \n\
    vec2 d = abs(p) - (halfSize - vec2(cornerRadius)); \n\
    float dist = length(max(d, 0.0)); \n\
\
    if (dist > cornerRadius) \n\
        discard; \n\
\
    gl_FragColor = fragColor / 255.0; \n\
}\n";

static const float quadVerts[] = {
    0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
};

#define I 1.f

static const float vertData[] = {
    -I, I,  -I, //
    -I, I,  I,  //
    I,  I,  -I, //
    I,  I,  -I, //
    -I, I,  I,  //
    I,  I,  I,  //

    I,  I,  -I, //
    I,  -I, -I, //
    -I, I,  -I, //
    -I, I,  -I, //
    I,  -I, -I, //
    -I, -I, -I, //

    -I, I,  -I, //
    -I, -I, -I, //
    -I, I,  I,  //
    -I, I,  I,  //
    -I, -I, -I, //
    -I, -I, I,  //

    -I, I,  I, //
    -I, -I, I, //
    I,  I,  I, //
    I,  I,  I, //
    -I, -I, I, //
    I,  -I, I, //

    I,  I,  I,  //
    I,  -I, I,  //
    I,  I,  -I, //
    I,  I,  -I, //
    I,  -I, I,  //
    I,  -I, -I, //

    I,  -I, -I, //
    I,  -I, I,  //
    -I, -I, -I, //
    -I, -I, -I, //
    I,  -I, I,  //
    -I, -I, I,  //
};

static const float colorData[] = {
    0.583f, 0.771f, 0.014f, //
    0.609f, 0.115f, 0.436f, //
    0.327f, 0.483f, 0.844f, //
    0.822f, 0.569f, 0.201f, //
    0.435f, 0.602f, 0.223f, //
    0.310f, 0.747f, 0.185f, //
    0.597f, 0.770f, 0.761f, //
    0.559f, 0.436f, 0.730f, //
    0.359f, 0.583f, 0.152f, //
    0.483f, 0.596f, 0.789f, //
    0.559f, 0.861f, 0.639f, //
    0.195f, 0.548f, 0.859f, //
    0.014f, 0.184f, 0.576f, //
    0.771f, 0.328f, 0.970f, //
    0.406f, 0.615f, 0.116f, //
    0.676f, 0.977f, 0.133f, //
    0.971f, 0.572f, 0.833f, //
    0.140f, 0.616f, 0.489f, //
    0.997f, 0.513f, 0.064f, //
    0.945f, 0.719f, 0.592f, //
    0.543f, 0.021f, 0.978f, //
    0.279f, 0.317f, 0.505f, //
    0.167f, 0.620f, 0.077f, //
    0.347f, 0.857f, 0.137f, //
    0.055f, 0.953f, 0.042f, //
    0.714f, 0.505f, 0.345f, //
    0.783f, 0.290f, 0.734f, //
    0.722f, 0.645f, 0.174f, //
    0.302f, 0.455f, 0.848f, //
    0.225f, 0.587f, 0.040f, //
    0.517f, 0.713f, 0.338f, //
    0.053f, 0.959f, 0.120f, //
    0.393f, 0.621f, 0.362f, //
    0.673f, 0.211f, 0.457f, //
    0.820f, 0.883f, 0.371f, //
    0.982f, 0.099f, 0.879f  //
};

struct app {
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

  qcam_t cam;

  struct {
    struct {
      GLuint ProgID, VBO;
      GLint inPositionLoc, fragColorLoc, cornerRadiusLoc, inRectLoc, screenLoc;
    } overlay;
    struct {
      GLuint ProgID, PosVBO, ColorVBO;
      GLint modelLoc, viewLoc, projLoc;
    } cube;
  } shaders;

  int width, height;
  float mpos_x, mpos_y;
  bool configured;
};

bool checkCompileErrors(GLuint shader, bool isprogram, char *type) {
  GLint success;
  GLchar infoLog[1024];
  if (!isprogram) {
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(shader, 1024, NULL, infoLog);
      fprintf(stderr, "SHADER_COMPILATION_ERROR of type: %s -> %s", type,
              infoLog);
    }
  } else {
    glGetProgramiv(shader, GL_LINK_STATUS, &success);
    if (!success) {
      glGetProgramInfoLog(shader, 1024, NULL, infoLog);
      fprintf(stderr, "PROGRAM_LINKING_ERROR of type: %s -> %s", type, infoLog);
    }
  }
  return !success;
}

int LoadShader(const char *vertex_source, const char *fragment_source) {
  // const char *vertex_source = solid_vert_glsl;
  // const char *fragment_source = solid_frag_glsl;
  bool error = false;

  GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vshader, 1, &vertex_source, NULL);
  glCompileShader(vshader);
  bool verror = checkCompileErrors(vshader, false, "VERTEX");
  if (verror)
    glDeleteShader(vshader);
  error |= verror;

  GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fshader, 1, &fragment_source, NULL);
  glCompileShader(fshader);
  bool ferror = checkCompileErrors(fshader, false, "FRAGMENT");
  if (ferror)
    glDeleteShader(fshader);
  error |= ferror;

  if (error) {
    return -1;
  }

  GLuint programID = glCreateProgram();
  glAttachShader(programID, vshader);
  glAttachShader(programID, fshader);
  glLinkProgram(programID);
  error = checkCompileErrors(programID, true, "PROGRAM");

  glDeleteShader(vshader);
  glDeleteShader(fshader);

  return programID;
}

static void pointer_enter(void *data, struct wl_pointer *pointer,
                          uint32_t serial, struct wl_surface *surface,
                          wl_fixed_t sx, wl_fixed_t sy) {
  (void)pointer;
  (void)serial;
  (void)surface;
  printf("Pointer entered at %.1f %.1f\n", wl_fixed_to_double(sx),
         wl_fixed_to_double(sy));
}

static void pointer_leave(void *data, struct wl_pointer *pointer,
                          uint32_t serial, struct wl_surface *surface) {
  printf("Pointer left\n");
}

static void pointer_motion(void *data, struct wl_pointer *pointer,
                           uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
  struct app *app = data;
  // printf("Motion %.1f %.1f\n", wl_fixed_to_double(sx),
  // wl_fixed_to_double(sy));
  app->mpos_x = wl_fixed_to_double(sx);
  app->mpos_y = wl_fixed_to_double(sy);
}

static void pointer_button(void *data, struct wl_pointer *pointer,
                           uint32_t serial, uint32_t time, uint32_t button,
                           uint32_t state) {
  printf("Button %u %s\n", button,
         state == WL_POINTER_BUTTON_STATE_PRESSED ? "pressed" : "released");
}

static void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time,
                         uint32_t axis, wl_fixed_t value) {
  printf("Scroll %.2f\n", wl_fixed_to_double(value));
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
  struct app *app = data;

  char *map = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
  app->keymap =
      xkb_keymap_new_from_string(app->xkb_ctx, map, XKB_KEYMAP_FORMAT_TEXT_V1,
                                 XKB_KEYMAP_COMPILE_NO_FLAGS);

  munmap(map, size);
  close(fd);

  app->xkb_state = xkb_state_new(app->keymap);
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard,
                           uint32_t serial, struct wl_surface *surface,
                           struct wl_array *keys) {
  (void)data;
  (void)keyboard;
  (void)serial;
  (void)surface;
  (void)keys;
  printf("Keyboard focus entered\n");
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard,
                           uint32_t serial, struct wl_surface *surface) {
  (void)data;
  (void)keyboard;
  (void)serial;
  (void)surface;
  printf("Keyboard focus left\n");
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard,
                         uint32_t serial, uint32_t time, uint32_t key,
                         uint32_t state) {
  struct app *app = data;

  if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
    xkb_keysym_t sym = xkb_state_key_get_one_sym(app->xkb_state, key + 8);

    char name[64];
    xkb_keysym_get_name(sym, name, sizeof(name));
    printf("Key pressed: %s\n", name);
  }
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
                               uint32_t serial, uint32_t mods_depressed,
                               uint32_t mods_latched, uint32_t mods_locked,
                               uint32_t group) {
  struct app *app = data;
  xkb_state_update_mask(app->xkb_state, mods_depressed, mods_latched,
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
  struct app *app = data;

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !app->pointer) {
    app->pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(app->pointer, &pointer_listener, app);
  }

  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !app->keyboard) {
    app->keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(app->keyboard, &keyboard_listener, app);
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
  struct app *app = data;
  xdg_surface_ack_configure(xdg_surface, serial);
  app->configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                               int32_t width, int32_t height,
                               struct wl_array *states) {
  struct app *app = data;

  if (width > 0 && height > 0) {
    app->width = width;
    app->height = height;
    wl_egl_window_resize(app->egl_window, width, height, 0, 0);
  }
}

static void toplevel_close(void *data, struct xdg_toplevel *toplevel) {
  exit(0);
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure,
    .close = toplevel_close,
};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version) {
  struct app *app = data;

  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    app->compositor =
        wl_registry_bind(registry, name, &wl_compositor_interface, 4);
  } else if (!strcmp(interface, wl_seat_interface.name)) {
    app->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);

    wl_seat_add_listener(app->seat, &seat_listener, app);

    app->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    app->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(app->wm_base, &wm_base_listener, app);
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

static void init_egl(struct app *app) {
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
      EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES2_BIT,
      EGL_NONE,
  };

  const EGLint ctx[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

  EGLConfig config;
  EGLint n;

  app->egl_display = eglGetDisplay((EGLNativeDisplayType)app->display);
  eglInitialize(app->egl_display, &major, &minor);

  eglChooseConfig(app->egl_display, config_attribs, &config, 1, &n);

  eglBindAPI(EGL_OPENGL_ES_API);

  app->egl_context =
      eglCreateContext(app->egl_display, config, EGL_NO_CONTEXT, ctx);

  app->egl_window = wl_egl_window_create(app->surface, app->width, app->height);

  app->egl_surface = eglCreateWindowSurface(app->egl_display, config,
                                            (uintptr_t)app->egl_window, NULL);

  eglMakeCurrent(app->egl_display, app->egl_surface, app->egl_surface,
                 app->egl_context);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  qcam_init_default(&app->cam);
  app->cam.MovementSpeed = 0.01f;
  app->cam.BinarySensitivity = 4.f;

  // Shader RECT
  int RectProgID = LoadShader(solid_vert_glsl, solid_frag_glsl);
  if (RectProgID < 0) {
    fprintf(stderr, "Failed to load rect shader!\n");
    exit(0);
  }

  glUseProgram(RectProgID);
  app->shaders.overlay.inPositionLoc =
      glGetAttribLocation(RectProgID, "inPosition");

  app->shaders.overlay.fragColorLoc =
      glGetUniformLocation(RectProgID, "fragColor");
  app->shaders.overlay.inRectLoc = glGetUniformLocation(RectProgID, "inRect");
  app->shaders.overlay.cornerRadiusLoc =
      glGetUniformLocation(RectProgID, "cornerRadius");
  app->shaders.overlay.screenLoc = glGetUniformLocation(RectProgID, "screen");

  app->shaders.overlay.ProgID = RectProgID;

  glGenBuffers(1, &app->shaders.overlay.VBO);
  glBindBuffer(GL_ARRAY_BUFFER, app->shaders.overlay.VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

  // Shader CUBE
  int CubeProgID = LoadShader(basic_colored_vert, basic_colored_frag);
  if (CubeProgID < 0) {
    fprintf(stderr, "Failed to load rect shader!\n");
    exit(0);
  }

  glUseProgram(CubeProgID);
  app->shaders.cube.modelLoc = glGetAttribLocation(CubeProgID, "model");
  app->shaders.cube.viewLoc = glGetAttribLocation(CubeProgID, "view");
  app->shaders.cube.projLoc = glGetAttribLocation(CubeProgID, "projection");

  glGenBuffers(1, &app->shaders.cube.PosVBO);
  glBindBuffer(GL_ARRAY_BUFFER, app->shaders.cube.PosVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertData), vertData, GL_STATIC_DRAW);

  glGenBuffers(1, &app->shaders.cube.ColorVBO);
  glBindBuffer(GL_ARRAY_BUFFER, app->shaders.cube.ColorVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(colorData), colorData, GL_STATIC_DRAW);

  app->shaders.cube.ProgID = CubeProgID;
}

static void render(struct app *app) {
  glViewport(0, 0, app->width, app->height);
  glClearColor(0.1f, 0.2f, 0.6f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glEnable(GL_CULL_FACE);

  qcam_input_update(&app->cam, 16.66f);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  glUseProgram(app->shaders.overlay.ProgID);
  glBindBuffer(GL_ARRAY_BUFFER, app->shaders.overlay.VBO);
  // GLint aPos = glGetAttribLocation(rend->programID, "aPos");
  glEnableVertexAttribArray(app->shaders.overlay.inPositionLoc);
  glVertexAttribPointer(app->shaders.overlay.inPositionLoc, 2, GL_FLOAT,
                        GL_FALSE, 0, 0);

  glUniform4f(app->shaders.overlay.fragColorLoc, 10.f, 10.f, 10.f, 255.f);
  glUniform4f(app->shaders.overlay.inRectLoc, app->mpos_x - 20.f,
              app->mpos_y - 20.f, 40.f, 40.f);
  glUniform2f(app->shaders.overlay.screenLoc, app->width, app->height);
  glUniform1f(app->shaders.overlay.cornerRadiusLoc, 10.f);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  eglSwapBuffers(app->egl_display, app->egl_surface);
}

int main(void) {
  struct app app = {
      .width = 640,
      .height = 480,
  };

  app.display = wl_display_connect(NULL);
  if (!app.display) {
    fprintf(stderr, "Failed to connect to Wayland\n");
    return 1;
  }

  app.registry = wl_display_get_registry(app.display);
  wl_registry_add_listener(app.registry, &registry_listener, &app);
  wl_display_roundtrip(app.display);

  app.surface = wl_compositor_create_surface(app.compositor);

  app.xdg_surface = xdg_wm_base_get_xdg_surface(app.wm_base, app.surface);
  xdg_surface_add_listener(app.xdg_surface, &xdg_surface_listener, &app);

  app.toplevel = xdg_surface_get_toplevel(app.xdg_surface);
  xdg_toplevel_add_listener(app.toplevel, &toplevel_listener, &app);
  xdg_toplevel_set_title(app.toplevel, "Wayland OpenGL Demo");

  wl_surface_commit(app.surface);

  while (!app.configured)
    wl_display_dispatch(app.display);

  init_egl(&app);

  while (wl_display_dispatch(app.display) != -1) {
    render(&app);
  }

  return 0;
}
