#ifndef LIBQ_QB_H
#define LIBQ_QB_H

#include "qbuild.h"

void qbuild_init();
void qbuild_quit();

typedef void *(qbuild_log_fn_t)(const char *str, void *ud);

void *root_node_create(node_t **newnode, void *parent_pool, int cmdc,
                       const char *cmdv[]);

int qbuild_is_done(node_t *any_node);

void qbuild_update_once(node_t *any_node);

void qbuild_wait_all(node_t *any_node);

void qbuild_log_stderr(node_t *root_node);

#endif
