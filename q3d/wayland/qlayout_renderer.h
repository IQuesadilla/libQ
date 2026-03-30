#ifndef CLAY_RENDERER_OPENGL_H
#define CLAY_RENDERER_OPENGL_H

#include "clay.h"
#include <apr_file_io.h>
#include <apr_pools.h>

typedef struct qlayout_renderer qlayout_renderer_t;

int qlayout_renderer_init(qlayout_renderer_t **newrend, apr_pool_t *parent,
                          apr_file_t *err);
bool qlayout_renderer_clay(qlayout_renderer_t *rend,
                           Clay_RenderCommandArray *rcommands);
#endif
