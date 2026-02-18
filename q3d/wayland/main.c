#define _POSIX_C_SOURCE 200112L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#include <GLES2/gl2.h>

#include <apr.h>
#include <apr_general.h>
#include <apr_pools.h>

#include <cglm/cglm.h>

#include <qcam.h>
#include <qwindow.h>

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

static void init_egl(struct app *app) {
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

static void render(struct app *app) {
  glViewport(0, 0, app->width, app->height);
  glClearColor(0.1f, 0.2f, 0.6f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  // glEnable(GL_CULL_FACE);

  qcam_input_update(&app->cam, 16.66f);

  glUseProgram(app->shaders.cube.ProgID);
  glBindBuffer(GL_ARRAY_BUFFER, app->shaders.cube.PosVBO);
  glEnableVertexAttribArray(app->shaders.cube.PosLoc);
  glVertexAttribPointer(app->shaders.cube.PosLoc, 3, GL_FLOAT, GL_FALSE, 0, 0);

  glBindBuffer(GL_ARRAY_BUFFER, app->shaders.cube.ColorVBO);
  glEnableVertexAttribArray(app->shaders.cube.ColorLoc);
  glVertexAttribPointer(app->shaders.cube.ColorLoc, 3, GL_FLOAT, GL_FALSE, 0,
                        0);

  static float angle = 0.f;
  float AspectRatio = ((float)app->width) / ((float)app->height);
  mat4 model, view;
  glm_mat4_identity(model);
  glm_translate(model, (vec3){0.f, 0.f, -50.f});
  glm_scale(model, (vec3){10.f, 10.f, 10.f});
  glm_rotate(model, angle, (vec3){0.f, 1.f, 0.f});
  angle += 0.003f;
  qcam_get_view(&app->cam, view);
  qcam_get_proj(&app->cam, AspectRatio, 0.1f, 1000.f);

  static bool dump = true;
  if (dump) {
    glm_mat4_print(model, stderr);
    glm_mat4_print(view, stderr);
    glm_mat4_print(app->cam.ProjectionMatrix, stderr);
    fprintf(stderr, "%d %d\n", app->shaders.cube.PosLoc,
            app->shaders.cube.ColorLoc);
    fprintf(stderr, "%d %d %d\n", app->shaders.cube.modelLoc,
            app->shaders.cube.viewLoc, app->shaders.cube.projLoc);
    dump = false;
  }

  glUniformMatrix4fv(app->shaders.cube.modelLoc, 1, GL_FALSE, (float *)model);
  glUniformMatrix4fv(app->shaders.cube.viewLoc, 1, GL_FALSE, (float *)view);
  glUniformMatrix4fv(app->shaders.cube.projLoc, 1, GL_FALSE,
                     (float *)app->cam.ProjectionMatrix);

  GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR)
    printf("GL ERR %x\n", err);

  glDrawArrays(GL_TRIANGLES, 0, 12 * 3);

  glDisable(GL_DEPTH_TEST);
  // glDisable(GL_CULL_FACE);

  glUseProgram(app->shaders.overlay.ProgID);
  glBindBuffer(GL_ARRAY_BUFFER, app->shaders.overlay.VBO);
  // GLint aPos = glGetAttribLocation(rend->programID, "aPos");
  glEnableVertexAttribArray(app->shaders.overlay.inPositionLoc);
  glVertexAttribPointer(app->shaders.overlay.inPositionLoc, 2, GL_FLOAT,
                        GL_FALSE, 0, 0);

  float mpos_x, mpos_y;
  qwindow_get_pointer(app->win, &mpos_x, &mpos_y);
  glUniform4f(app->shaders.overlay.fragColorLoc, 10.f, 10.f, 10.f, 255.f);
  glUniform4f(app->shaders.overlay.inRectLoc, mpos_x - 20.f, mpos_y - 20.f,
              40.f, 40.f);
  glUniform2f(app->shaders.overlay.screenLoc, app->width, app->height);
  glUniform1f(app->shaders.overlay.cornerRadiusLoc, 10.f);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  qwindow_swap(app->win);
}

int main(int argc, const char *const argv[]) {
  apr_app_initialize(&argc, &argv, NULL);

  apr_pool_t *pool;
  apr_pool_create_core(&pool);

  struct app app = {
      .width = 640,
      .height = 480,
  };

  qwindow_init(&app.win, pool);
  init_egl(&app);

  struct pollfd pfd = {
      .fd =,
      .events = POLLIN,
  };

  while (1) {
    wl_display_dispatch_pending(app.win->display);

    wl_display_flush(app.win->display);

    if (wl_display_prepare_read(app.win->display) == -1) {
      wl_display_dispatch_pending(app.win->display);
      continue;
    }

    int ret = poll(&pfd, 1, 16); // ~60fps timeout

    if (ret > 0 && (pfd.revents & POLLIN)) {
      wl_display_read_events(app.win->display);
      wl_display_dispatch_pending(app.win->display);
    } else {
      wl_display_cancel_read(app.win->display);
    }

    render(&app);
  }

  apr_terminate();
  return 0;
}
