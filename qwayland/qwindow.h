#ifndef QLIB_QWINDOW_H
#define QLIB_QWINDOW_H

#include <apr.h>
#include <apr_pools.h>

#include "apr_events.h"

typedef void (*redraw_fn_t)(void *ud); // dT in usec
typedef void (*resize_fn_t)(void *ud, int width, int height);
typedef void (*mouse_move_fn_t)(void *ud, float x, float y);
typedef void (*mouse_down_fn_t)(void *ud, int down);
typedef void (*key_down_fn_t)(void *ud, uint32_t keysym);

struct qwindow_interface {
  int width, height;
  apr_file_t *err;

  apr_loop_t *loop;
  void *ud;
  redraw_fn_t redraw;
  resize_fn_t resize;
  mouse_move_fn_t mouse_move;
  mouse_down_fn_t mouse_down;
  key_down_fn_t key_down;
};
typedef struct qwindow_interface qwindow_interface_t;

typedef struct qwindow qwindow_t;

int qwindow_swap(qwindow_t *win);
void qwindow_make_current(qwindow_t *win);

int qwindow_init(qwindow_t **newwin, apr_pool_t *parent,
                 qwindow_interface_t *ev);

#endif
