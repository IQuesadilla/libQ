#ifndef QLIB_QWINDOW_H
#define QLIB_QWINDOW_H

#include <apr.h>
#include <apr_pools.h>

#include "apr_events.h"

typedef void (*redraw_fn_t)(void *ud, uint64_t dT); // dT in usec

struct qwindow_events {
  apr_loop_t *loop;
  redraw_fn_t redraw;
  void *ud;
};
typedef struct qwindow_events qwindow_events_t;

typedef struct qwindow qwindow_t;

int qwindow_swap(qwindow_t *win);
int qwindow_get_pointer(qwindow_t *win, float *w, float *h);
void qwindow_make_current(qwindow_t *win);

int qwindow_init(qwindow_t **newwin, apr_pool_t *pool, qwindow_events_t *ev);
int qwindow_pre(qwindow_t *win);

#endif
