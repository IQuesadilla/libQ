#ifndef QLIB_QWINDOW_H
#define QLIB_QWINDOW_H

#include <apr.h>
#include <apr_pools.h>

typedef struct qwindow qwindow_t;

int qwindow_swap(qwindow_t *win);
int qwindow_get_pointer(qwindow_t *win, float *w, float *h);

int qwindow_add_events(qwindow_t *win);

int qwindow_init(qwindow_t **newwin, apr_pool_t *pool);

#endif
