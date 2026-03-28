#ifndef CLAY_RENDERER_OPENGL_H
#define CLAY_RENDERER_OPENGL_H

#include "clay.h"
#include <apr_pools.h>

typedef struct Clay_GLRenderData Clay_GLRenderData_t;

int Clay_GLRenderInit(Clay_GLRenderData_t **newrend, apr_pool_t *parent);
bool SDL_Clay_RenderClayCommands(Clay_GLRenderData_t *rend,
                                 Clay_RenderCommandArray *rcommands);

Clay_Dimensions SDL_MeasureText(Clay_StringSlice text,
                                Clay_TextElementConfig *config, void *userData);
void HandleClayErrors(Clay_ErrorData errorData);

#endif
