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

static const char lua_cmd[] = "\
layout.log(\"hello\")\
print(layout.add(2, 3))\
";

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
  qwindow_t *win;
  qcam_t cam;

  struct {
    struct {
      GLuint ProgID, VBO;
      GLint inPositionLoc, fragColorLoc, cornerRadiusLoc, inRectLoc, screenLoc;
    } overlay;
    struct {
      GLuint ProgID, PosVBO, ColorVBO;
      GLint PosLoc, ColorLoc, modelLoc, viewLoc, projLoc;
    } cube;
  } shaders;

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
};
typedef struct app app_t;

bool checkCompileErrors(GLuint shader, bool isprogram, char *type,
                        apr_file_t *err) {
  GLint success;
  GLchar infoLog[1024];
  if (!isprogram) {
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(shader, 1024, NULL, infoLog);
      apr_file_printf(err, "SHADER_COMPILATION_ERROR of type: %s -> %s", type,
                      infoLog);
    }
  } else {
    glGetProgramiv(shader, GL_LINK_STATUS, &success);
    if (!success) {
      glGetProgramInfoLog(shader, 1024, NULL, infoLog);
      apr_file_printf(err, "PROGRAM_LINKING_ERROR of type: %s -> %s", type,
                      infoLog);
    }
  }
  return !success;
}

int LoadShader(const char *vertex_source, const char *fragment_source,
               apr_file_t *err) {
  // const char *vertex_source = solid_vert_glsl;
  // const char *fragment_source = solid_frag_glsl;
  bool error = false;

  GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vshader, 1, &vertex_source, NULL);
  glCompileShader(vshader);
  bool verror = checkCompileErrors(vshader, false, "VERTEX", err);
  if (verror)
    glDeleteShader(vshader);
  error |= verror;

  GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fshader, 1, &fragment_source, NULL);
  glCompileShader(fshader);
  bool ferror = checkCompileErrors(fshader, false, "FRAGMENT", err);
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
  error = checkCompileErrors(programID, true, "PROGRAM", err);

  glDeleteShader(vshader);
  glDeleteShader(fshader);

  return programID;
}

static void init_egl(struct app *app) {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  qcam_init_default(&app->cam);
  app->cam.MovementSpeed = 0.01f;
  app->cam.BinarySensitivity = 4.f;
  app->aspect_ratio = qcam_set_view(&app->cam, app->width, app->height);

  // Shader RECT
  int RectProgID = LoadShader(solid_vert_glsl, solid_frag_glsl, app->err);
  if (RectProgID < 0) {
    apr_file_printf(app->err, "Failed to load rect shader!\n");
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
  int CubeProgID = LoadShader(basic_colored_vert, basic_colored_frag, app->err);
  if (CubeProgID < 0) {
    apr_file_printf(app->err, "Failed to load rect shader!\n");
    exit(0);
  }

  glUseProgram(CubeProgID);
  app->shaders.cube.PosLoc = glGetAttribLocation(CubeProgID, "aPos");
  app->shaders.cube.ColorLoc = glGetAttribLocation(CubeProgID, "aColor");

  app->shaders.cube.modelLoc = glGetUniformLocation(CubeProgID, "model");
  app->shaders.cube.viewLoc = glGetUniformLocation(CubeProgID, "view");
  app->shaders.cube.projLoc = glGetUniformLocation(CubeProgID, "projection");

  glGenBuffers(1, &app->shaders.cube.PosVBO);
  glBindBuffer(GL_ARRAY_BUFFER, app->shaders.cube.PosVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertData), vertData, GL_STATIC_DRAW);

  glGenBuffers(1, &app->shaders.cube.ColorVBO);
  glBindBuffer(GL_ARRAY_BUFFER, app->shaders.cube.ColorVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(colorData), colorData, GL_STATIC_DRAW);

  app->shaders.cube.ProgID = CubeProgID;
}

static void render(app_t *app, uint64_t dT,
                   Clay_RenderCommandArray *render_commands) {
  glViewport(0, 0, app->width, app->height);
  // glClearColor(0.1f, 0.2f, 0.6f, 1.0f);
  // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (qlayout_renderer_clay(app->rend, render_commands)) {
    /*
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    // glEnable(GL_CULL_FACE);

    qcam_input_update(&app->cam, ((float)dT) / 1e6);

    glUseProgram(app->shaders.cube.ProgID);
    glBindBuffer(GL_ARRAY_BUFFER, app->shaders.cube.PosVBO);
    glEnableVertexAttribArray(app->shaders.cube.PosLoc);
    glVertexAttribPointer(app->shaders.cube.PosLoc, 3, GL_FLOAT, GL_FALSE, 0,
                          0);

    glBindBuffer(GL_ARRAY_BUFFER, app->shaders.cube.ColorVBO);
    glEnableVertexAttribArray(app->shaders.cube.ColorLoc);
    glVertexAttribPointer(app->shaders.cube.ColorLoc, 3, GL_FLOAT, GL_FALSE, 0,
                          0);

    static float angle = 0.f;
    mat4 model, view;
    glm_mat4_identity(model);
    glm_translate(model, (vec3){0.f, 0.f, -50.f});
    glm_scale(model, (vec3){10.f, 10.f, 10.f});
    glm_rotate(model, angle, (vec3){0.f, 1.f, 0.f});
    angle += 0.0000006 * ((double)dT);
    qcam_get_view(&app->cam, view);
    qcam_get_proj(&app->cam, app->aspect_ratio, 0.1f, 1000.f);

    static bool dump = true;
    if (dump) {
      // glm_mat4_print(model, stderr);
      // glm_mat4_print(view, stderr);
      // glm_mat4_print(app->cam.ProjectionMatrix, stderr);
      apr_file_printf(app->err, "%d %d\n", app->shaders.cube.PosLoc,
                      app->shaders.cube.ColorLoc);
      apr_file_printf(app->err, "%d %d %d\n", app->shaders.cube.modelLoc,
                      app->shaders.cube.viewLoc, app->shaders.cube.projLoc);
      dump = false;
    }

    glUniformMatrix4fv(app->shaders.cube.modelLoc, 1, GL_FALSE, (float *)model);
    glUniformMatrix4fv(app->shaders.cube.viewLoc, 1, GL_FALSE, (float *)view);
    glUniformMatrix4fv(app->shaders.cube.projLoc, 1, GL_FALSE,
                       (float *)app->cam.ProjectionMatrix);

    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
      apr_file_printf(app->err, "GL ERR %x\n", err);

    glDrawArrays(GL_TRIANGLES, 0, 12 * 3);

glDisable(GL_DEPTH_TEST);
// glDisable(GL_CULL_FACE);

glUseProgram(app->shaders.overlay.ProgID);
glBindBuffer(GL_ARRAY_BUFFER, app->shaders.overlay.VBO);
// GLint aPos = glGetAttribLocation(rend->programID, "aPos");
glEnableVertexAttribArray(app->shaders.overlay.inPositionLoc);
glVertexAttribPointer(app->shaders.overlay.inPositionLoc, 2, GL_FLOAT,
                      GL_FALSE, 0, 0);

float mpos_x = app->mpos_x, mpos_y = app->mpos_y;
glUniform4f(app->shaders.overlay.fragColorLoc, 10.f, 10.f, 10.f, 255.f);
glUniform4f(app->shaders.overlay.inRectLoc, mpos_x - 20.f, mpos_y - 20.f,
            40.f, 40.f);
glUniform2f(app->shaders.overlay.screenLoc, app->width, app->height);
glUniform1f(app->shaders.overlay.cornerRadiusLoc, 10.f);

glDrawArrays(GL_TRIANGLES, 0, 6);
*/

    qwindow_swap(app->win);
  }
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

    // char id[] = ;
    // Clay_String sid = {.chars = id, .length = strlen(id)};
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
        static bool MenuVisible = false;
        static bool ButtonHover = false;
        ButtonHover = Clay_PointerOver(Button_el);
        Clay_Color ButtonColor = ButtonHover ? (Clay_Color){255, 100, 100, 255}
                                             : (Clay_Color){100, 100, 100, 255};
        CLAY(Button_el, {
                            .layout =
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
            luaL_dostring(app->L, lua_cmd);
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
           }) {}

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

    render_commands = Clay_EndLayout();
    app->dorelay = false;
  }

  if (redraw) {
    uint64_t now = apr_time_now();
    render(app, now - app->lastframe, &render_commands);
    app->lastframe = now;

    app->doredraw = false;
    app->needs_redraw = false;
  } else {
    app->needs_redraw = true;
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
  lua_clay_openlibs(app.L);

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
