#ifndef CLAY_RENDERER_OPENGL_H
#define CLAY_RENDERER_OPENGL_H

#include "clay.h"
#include <apr_events.h>
#include <apr_file_io.h>
#include <apr_pools.h>

typedef struct qlayout_renderer qlayout_renderer_t;

int qlayout_renderer_init(qlayout_renderer_t **newrend, apr_pool_t *parent,
                          apr_loop_t *loop, apr_file_t *err);
bool qlayout_renderer_clay(qlayout_renderer_t *rend,
                           Clay_RenderCommandArray *rcommands);
void qlayout_renderer_resize(qlayout_renderer_t *rend, float w, float h,
                             float scaling);
#endif
