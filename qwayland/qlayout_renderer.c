#define CLAY_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "qlayout_renderer.h"

#include <GLES2/gl2.h>
#include <apr.h>
#include <apr_file_io.h>
#include <apr_hash.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <cglm/cglm.h>
#include <stb_image.h>
#include <stb_truetype.h>

struct drawunit {
  apr_pool_t *pool;
  Clay_RenderCommand rcmd;
};
typedef struct drawunit drawunit_t;

struct qlayout_renderer {
  // SDL_Renderer *renderer;
  Clay_Context *clay_cxt;
  stbtt_fontinfo font;
  // TTF_TextEngine *textEngine;
  // TTF_Font **fonts;
  struct {
    struct {
      GLuint ProgID, VBO;
      GLint inPositionLoc, fragColorLoc, cornerRadiusLoc, inRectLoc, screenLoc;
    } rect;
    struct {
      GLuint ProgID;
      GLint inPositionLoc, inRectLoc, screenLoc, textImgLoc;
    } text;
  } shaders;
  float w, h, scale;
  int32_t prevcmdlen;
  // drawunit_t *drawunits;
  apr_hash_t *drawunits, *imgcache;
  apr_pool_t *pool;
  apr_file_t *err;
  apr_loop_t *loop;
  bool resize_font;
};

struct qlayout_image {
  GLuint handle;
};
typedef struct qlayout_image qlayout_image_t;

/*
 * ------ PRIVATE ------
 */

// NOTE: Corner rounding should happen on the GPU, in the fragment shader

/*
static void SDL_Clay_RenderArc(Clay_SDL3RendererData *rend,
                               const SDL_FPoint center, const float radius,
                               const float startAngle, const float endAngle,
                               const float thickness, const Clay_Color
color) { SDL_SetRenderDrawColor(rend->renderer, color.r, color.g,
color.b, color.a);

  const float radStart = startAngle * (SDL_PI_F / 180.0f);
  const float radEnd = endAngle * (SDL_PI_F / 180.0f);

  const int numCircleSegments =
      SDL_max(NUM_CIRCLE_SEGMENTS,
              (int)(radius * 1.5f)); // increase circle segments for larger
                                     // circles, 1.5 is arbitrary.

  const float angleStep = (radEnd - radStart) / (float)numCircleSegments;
  const float thicknessStep =
      0.4f; // arbitrary value to avoid overlapping lines. Changing
            // THICKNESS_STEP or numCircleSegments might cause artifacts.

  for (float t = thicknessStep; t < thickness - thicknessStep;
       t += thicknessStep) {
    SDL_FPoint points[numCircleSegments + 1];
    const float clampedRadius = SDL_max(radius - t, 1.0f);

    for (int i = 0; i <= numCircleSegments; i++) {
      const float angle = radStart + i * angleStep;
      points[i] =
          (SDL_FPoint){SDL_roundf(center.x + SDL_cosf(angle) *
clampedRadius), SDL_roundf(center.y + SDL_sinf(angle) * clampedRadius)};
    }
    SDL_RenderLines(rend->renderer, points, numCircleSegments + 1);
  }
}
*/

static bool checkCompileErrors(apr_file_t *err, GLuint shader, bool isprogram,
                               char *type) {
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

static const float quadVerts[] = {
    0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
};

static int LoadShader(apr_file_t *err, const char *vertex_source_path,
                      const char *fragment_source_path) {
  apr_file_t *vsrc_handle, *fsrc_handle;
  apr_pool_t *temp = apr_file_pool_get(err);
  apr_file_open(&vsrc_handle, vertex_source_path, APR_FOPEN_READ,
                APR_FPROT_OS_DEFAULT, temp);

  char vertex_source[16384];
  apr_size_t vertex_source_size = 0;
  apr_file_read_full(vsrc_handle, vertex_source, sizeof(vertex_source),
                     &vertex_source_size);
  vertex_source[vertex_source_size] = '\0';
  const char *vsrc = vertex_source;

  apr_file_close(vsrc_handle);

  apr_file_open(&fsrc_handle, fragment_source_path, APR_FOPEN_READ,
                APR_FPROT_OS_DEFAULT, temp);

  apr_size_t fragment_source_size = 0;
  char fragment_source[16384];
  apr_file_read_full(fsrc_handle, fragment_source, sizeof(fragment_source),
                     &fragment_source_size);
  fragment_source[fragment_source_size] = '\0';
  const char *fsrc = fragment_source;

  apr_file_close(fsrc_handle);

  bool error = false;

  if (vertex_source_size == 0 || fragment_source_size == 0) {
    apr_file_printf(err, "Tried compiling an empty shader");
    return -1;
  }

  GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vshader, 1, &vsrc, NULL);
  glCompileShader(vshader);
  bool verror = checkCompileErrors(err, vshader, false, "VERTEX");
  if (verror)
    glDeleteShader(vshader);
  error |= verror;

  GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fshader, 1, &fsrc, NULL);
  glCompileShader(fshader);
  bool ferror = checkCompileErrors(err, fshader, false, "FRAGMENT");
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
  error = checkCompileErrors(err, programID, true, "PROGRAM");

  glDeleteShader(vshader);
  glDeleteShader(fshader);

  return programID;
}

void Clay_GLRenderFillRoundedRect(qlayout_renderer_t *rend,
                                  const Clay_BoundingBox rect,
                                  const float cornerRadius,
                                  const Clay_Color color) {
  glUseProgram(rend->shaders.rect.ProgID);
  glBindBuffer(GL_ARRAY_BUFFER, rend->shaders.rect.VBO);
  // GLint aPos = glGetAttribLocation(rend->programID, "aPos");
  glEnableVertexAttribArray(rend->shaders.rect.inPositionLoc);
  glVertexAttribPointer(rend->shaders.rect.inPositionLoc, 2, GL_FLOAT, GL_FALSE,
                        0, 0);

  glUniform4f(rend->shaders.rect.fragColorLoc, color.r, color.g, color.b,
              color.a);
  glUniform4f(rend->shaders.rect.inRectLoc, rect.x, rect.y, rect.width,
              rect.height);
  glUniform2f(rend->shaders.rect.screenLoc, rend->w, rend->h);
  glUniform1f(rend->shaders.rect.cornerRadiusLoc, cornerRadius);

  glDrawArrays(GL_TRIANGLES, 0, 6);
  return;
}

void Clay_GLRenderText(qlayout_renderer_t *rend, const Clay_BoundingBox rect,
                       uint8_t *textImg) {
  glUseProgram(rend->shaders.text.ProgID);

  GLuint tex = 0;
  glGenTextures(1, &tex);
  glActiveTexture(GL_TEXTURE0 + 0);
  glBindTexture(GL_TEXTURE_2D, tex);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rect.width, rect.height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, textImg);

  glEnableVertexAttribArray(rend->shaders.rect.inPositionLoc);
  glVertexAttribPointer(rend->shaders.rect.inPositionLoc, 2, GL_FLOAT, GL_FALSE,
                        0, 0);

  glUniform4f(rend->shaders.text.inRectLoc, rect.x, rect.y, rect.width,
              rect.height);
  glUniform2f(rend->shaders.text.screenLoc, rend->w, rend->h);
  glUniform1i(rend->shaders.text.textImgLoc, 0);

  glBindBuffer(GL_ARRAY_BUFFER, rend->shaders.rect.VBO);

  // Draw texture
  glDrawArrays(GL_TRIANGLES, 0, 6);

  glDeleteTextures(1, &tex);
}

bool EqRenderCmd(Clay_RenderCommand *old, Clay_RenderCommand *new) {
  if (memcmp(old, new, sizeof(*old))) {
    *old = *new;
    return false;
  }
  return true;
}

Clay_Dimensions Clay_MeasureText(Clay_StringSlice text,
                                 Clay_TextElementConfig *config,
                                 void *userData) {
  qlayout_renderer_t *rend = userData;
  float scale = stbtt_ScaleForPixelHeight(&rend->font, config->fontSize);

  int width = 0;
  int ascent, descent, lineGap;

  stbtt_GetFontVMetrics(&rend->font, &ascent, &descent, &lineGap);

  float baseline = ascent * scale;

  for (int i = 0; text.chars[i]; i++) {
    int advance, lsb;
    stbtt_GetCodepointHMetrics(&rend->font, text.chars[i], &advance, &lsb);
    width += (int)(advance * scale);

    if (text.chars[i + 1]) {
      width +=
          (int)(scale * stbtt_GetCodepointKernAdvance(
                            &rend->font, text.chars[i], text.chars[i + 1]));
    }
  }

  int height = (int)((ascent - descent) * scale);

  printf("width: %d, height: %d, scale: %f\n", width, height, scale);

  return (Clay_Dimensions){(float)width, (float)height};
}

void HandleClayErrors(Clay_ErrorData errorData);

void ReinitClay(qlayout_renderer_t *rend) {
  uint32_t clayMemorySize = Clay_MinMemorySize();
  apr_file_printf(rend->err, "Clay Size: %u\n", clayMemorySize);
  rend->clay_cxt = Clay_Initialize(
      (Clay_Arena){
          .memory = apr_palloc(rend->pool, clayMemorySize),
          .capacity = clayMemorySize,
      },
      (Clay_Dimensions){
          .width = rend->w,
          .height = rend->h,
      },
      (Clay_ErrorHandler){
          .errorHandlerFunction = HandleClayErrors,
          .userData = rend,
      });
  Clay_SetCurrentContext(rend->clay_cxt);
  Clay_SetMeasureTextFunction(Clay_MeasureText, rend);
  Clay_ResetMeasureTextCache();
}

void HandleClayErrors(Clay_ErrorData errorData) {
  qlayout_renderer_t *rend = errorData.userData;
  if (errorData.errorType ==
      CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED) {

    apr_file_printf(rend->err, "Reset Text Cache\n");
    Clay_ResetMeasureTextCache();
    rend->resize_font = true;
  } else {
    apr_file_printf(rend->err, "%s\n", errorData.errorText.chars);
  }
}

static int qlayout_renderer_pre(void *ud) {
  qlayout_renderer_t *rend = ud;
  if (rend->resize_font) {
    int32_t wc = Clay_GetMaxMeasureTextCacheWordCount();
    Clay_SetMaxMeasureTextCacheWordCount(wc * 2);
    ReinitClay(rend);
    rend->resize_font = false;
  }
  return 0;
}

/*
 * ------ PUBLIC ------
 */

int qlayout_renderer_init(qlayout_renderer_t **newrend, apr_pool_t *parent,
                          apr_loop_t *loop, apr_file_t *err) {
  apr_pool_t *pool;
  apr_pool_create(&pool, parent);
  qlayout_renderer_t *rend = apr_pcalloc(pool, sizeof(qlayout_renderer_t));

  rend->pool = pool;
  rend->drawunits = apr_hash_make(rend->pool);
  rend->imgcache = apr_hash_make(rend->pool);
  rend->prevcmdlen = 0;
  rend->w = 1920;
  rend->h = 1080;
  rend->err = err;
  rend->loop = loop;

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Shader RECT
  int RectProgID =
      LoadShader(err, "./assets/solid.vert.glsl", "./assets/solid.frag.glsl");
  if (RectProgID < 0) {
    apr_file_printf(rend->err, "Failed to load rect shader!\n");
    return -1;
  }

  glUseProgram(RectProgID);
  rend->shaders.rect.inPositionLoc =
      glGetAttribLocation(RectProgID, "inPosition");

  rend->shaders.rect.fragColorLoc =
      glGetUniformLocation(RectProgID, "fragColor");
  rend->shaders.rect.inRectLoc = glGetUniformLocation(RectProgID, "inRect");
  rend->shaders.rect.cornerRadiusLoc =
      glGetUniformLocation(RectProgID, "cornerRadius");
  rend->shaders.rect.screenLoc = glGetUniformLocation(RectProgID, "screen");

  rend->shaders.rect.ProgID = RectProgID;

  glGenBuffers(1, &rend->shaders.rect.VBO);
  glBindBuffer(GL_ARRAY_BUFFER, rend->shaders.rect.VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

  // Shader TEXT
  int TextProgID =
      LoadShader(err, "./assets/text.vert.glsl", "./assets/text.frag.glsl");
  if (TextProgID < 0) {
    apr_file_printf(rend->err, "Failed to load text shader!\n");
    return -1;
  }

  rend->shaders.text.inPositionLoc =
      glGetAttribLocation(TextProgID, "inPosition");

  rend->shaders.text.inRectLoc = glGetUniformLocation(TextProgID, "inRect");
  rend->shaders.text.screenLoc = glGetUniformLocation(TextProgID, "screen");
  rend->shaders.text.textImgLoc = glGetUniformLocation(TextProgID, "textImg");

  rend->shaders.text.ProgID = TextProgID;

  unsigned char *ttf_buffer = NULL;
  // Roboto-Regular.ttf
  FILE *f = fopen("assets/OpenSans-VariableFont_wdth,wght.ttf", "rb");

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  fseek(f, 0, SEEK_SET);

  ttf_buffer = malloc(size);
  fread(ttf_buffer, 1, size, f);
  fclose(f);

  stbtt_InitFont(&rend->font, ttf_buffer,
                 stbtt_GetFontOffsetForIndex(ttf_buffer, 0));
  // stbtt_SetVariations(&rend->font, axes, num_axes);

  ReinitClay(rend);

  apr_event_add_prerun(rend->loop, qlayout_renderer_pre, rend);

  *newrend = rend;

  return 0;
}

bool qlayout_renderer_clay(qlayout_renderer_t *rend,
                           Clay_RenderCommandArray *rcommands) {
  bool changed = rcommands->length != rend->prevcmdlen;
  // bool validScissor = false;
  // Clay_BoundingBox currentScissor = {0};
  for (size_t i = 0; i < rcommands->length; i++) {
    Clay_RenderCommand *rcmd = Clay_RenderCommandArray_Get(rcommands, i);

    if (rcmd->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START) {
      // currentScissor = rcmd->boundingBox; // Scissor
      //  validScissor = true;
      continue;
    }

    if (rcmd->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
      // currentScissor = (Clay_BoundingBox){0.f, 0.f, 0.f, 0.f};
      // validScissor = false;
      continue;
    }

    drawunit_t *old =
        apr_hash_get(rend->drawunits, &rcmd->id, sizeof(rcmd->id));
    if (old) {
      if (!EqRenderCmd(&old->rcmd, rcmd)) {
        changed = true;
        // continue;
      }
    } else {
      apr_pool_t *pool;
      apr_pool_create(&pool, rend->pool);
      old = apr_palloc(pool, sizeof(drawunit_t));
      old->pool = pool;
      old->rcmd = *rcmd;
      // printf("new %u\n", old->rcmd.id);
      // HASH_ADD_INT(rend->drawunits, rcmd.id, old);
      apr_hash_set(rend->drawunits, &old->rcmd.id, sizeof(old->rcmd.id), old);
      changed = true;
    }
  }
  if (!changed) {
    // printf("not changed\n");
    return false;
  }

  Clay_Color oldbg = {0};
  rend->prevcmdlen = rcommands->length;
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  for (size_t i = 0; i < rcommands->length; i++) {
    Clay_RenderCommand *rcmd = Clay_RenderCommandArray_Get(rcommands, i);
    Clay_BoundingBox rect = {
        .x = rcmd->boundingBox.x * rend->scale,
        .y = rcmd->boundingBox.y * rend->scale,
        .width = rcmd->boundingBox.width * rend->scale,
        .height = rcmd->boundingBox.height * rend->scale,
    };

    switch (rcmd->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
      Clay_RectangleRenderData *config = &rcmd->renderData.rectangle;
      if (config->cornerRadius.topLeft > 0) {
        Clay_GLRenderFillRoundedRect(rend, rect,
                                     config->cornerRadius.topLeft * rend->scale,
                                     config->backgroundColor);
      } else {
        Clay_GLRenderFillRoundedRect(rend, rect, 0.f, config->backgroundColor);
      }
      oldbg = config->backgroundColor;
    } break;
    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
      Clay_TextRenderData *config = &rcmd->renderData.text;
      float scale = stbtt_ScaleForPixelHeight(&rend->font, config->fontSize) *
                    rend->scale;

      int w = rect.width;
      int h = rect.height;

      const char *text = config->stringContents.chars;
      uint8_t *bitmap = calloc(w * h, 1);

      int x = 0;

      int ascent, descent, lineGap;

      stbtt_GetFontVMetrics(&rend->font, &ascent, &descent, &lineGap);

      float baseline = ascent * scale;

      for (int i = 0; text[i]; i++) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&rend->font, text[i], &advance, &lsb);

        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&rend->font, text[i], scale, scale, &x0,
                                    &y0, &x1, &y1);

        int glyph_w = x1 - x0;
        int glyph_h = y1 - y0;

        unsigned char *glyph_bitmap = stbtt_GetCodepointBitmap(
            &rend->font, 0, scale, text[i], &glyph_w, &glyph_h, 0, 0);

        int y = (int)baseline + y0;

        for (int gy = 0; gy < glyph_h; gy++) {
          for (int gx = 0; gx < glyph_w; gx++) {
            int dst_x = x + gx + x0;
            int dst_y = y + gy;

            if (dst_x >= 0 && dst_x < w && dst_y >= 0 && dst_y < h) {
              bitmap[dst_y * w + dst_x] |= glyph_bitmap[gy * glyph_w + gx];
            }
          }
        }

        stbtt_FreeBitmap(glyph_bitmap, NULL);

        x += (int)(advance * scale);

        if (text[i + 1]) {
          x += (int)(scale * stbtt_GetCodepointKernAdvance(&rend->font, text[i],
                                                           text[i + 1]));
        }
      }

      uint8_t *pixels = malloc(w * h * sizeof(uint32_t));

      for (int i = 0; i < w * h; i++) {
        float a = bitmap[i];
        pixels[(i * 4) + 0] = config->textColor.r;
        pixels[(i * 4) + 1] = config->textColor.g;
        pixels[(i * 4) + 2] = config->textColor.b;
        pixels[(i * 4) + 3] = (config->textColor.a * a) / 255;
      }

      Clay_GLRenderText(rend, rect, (void *)pixels);
      free(pixels);
      free(bitmap);
    } break;
    case CLAY_RENDER_COMMAND_TYPE_BORDER: {
    } break;
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
      const char *imgpath = rcmd->renderData.image.imageData;
      qlayout_image_t *img =
          apr_hash_get(rend->imgcache, imgpath, APR_HASH_KEY_STRING);
      if (!img) {
        int x = 0, y = 0, nchannels;
        uint8_t *imgdata = stbi_load(imgpath, &x, &y, &nchannels, 4);
        assert(imgdata);
        printf("image %d %d %d %u %u %u %u\n", x, y, nchannels, imgdata[0],
               imgdata[1], imgdata[2], imgdata[3]);

        img = apr_palloc(rend->pool, sizeof(*img));
        apr_hash_set(rend->imgcache, apr_pstrdup(rend->pool, imgpath),
                     APR_HASH_KEY_STRING, img);

        glGenTextures(1, &img->handle);
        glBindTexture(GL_TEXTURE_2D, img->handle);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
        // GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,
        // GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLuint format, iformat;
        if (nchannels == 4) {
          format = GL_RGBA;
          iformat = GL_RGBA;
        } else {
          format = GL_RGBA;
          iformat = GL_RGBA;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, iformat, x, y, 0, format,
                     GL_UNSIGNED_BYTE, imgdata);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(imgdata);
      }

      glUseProgram(rend->shaders.text.ProgID);
      glActiveTexture(GL_TEXTURE0 + 0);
      glBindTexture(GL_TEXTURE_2D, img->handle);

      glEnableVertexAttribArray(rend->shaders.rect.inPositionLoc);
      glVertexAttribPointer(rend->shaders.rect.inPositionLoc, 2, GL_FLOAT,
                            GL_FALSE, 0, 0);

      glUniform4f(rend->shaders.text.inRectLoc, rect.x, rect.y, rect.width,
                  rect.height);
      glUniform2f(rend->shaders.text.screenLoc, rend->w, rend->h);
      glUniform1i(rend->shaders.text.textImgLoc, 0);

      glBindBuffer(GL_ARRAY_BUFFER, rend->shaders.rect.VBO);

      // Draw texture
      glDrawArrays(GL_TRIANGLES, 0, 6);

      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
      break; // TODO: Run custom callback
    }
    default:
      apr_file_printf(rend->err, "Unknown render command type: %d",
                      rcmd->commandType);
    }
  }
  return true;
}

void qlayout_renderer_resize(qlayout_renderer_t *rend, float w, float h,
                             float scaling) {
  int phys_width = round(w * scaling);
  int phys_height = round(h * scaling);

  Clay_SetLayoutDimensions((Clay_Dimensions){.width = w, .height = h});

  glViewport(0, 0, phys_width, phys_height);
  rend->w = phys_width;
  rend->h = phys_height;
  rend->scale = scaling;
}
