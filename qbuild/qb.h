#ifndef LIBQ_QB_H
#define LIBQ_QB_H

#include "qbuild.h"
// #include "qtest.h"

enum qb_command {
  QB_BUILD,
  QB_MENUCONFIG,
  QB_TEST,
};
typedef enum qb_command qb_command_t;

void qbuild_init();
void qbuild_quit();

typedef void (*qbuild_log_fn_t)(const char *str, void *ud);

struct qb_disp_cmds {
  void (*qb_log_fn)(const char *str, void *ud);
  void (*qb_on_queue_fn)(const char *func, void *ud);
  void (*qb_on_trigger_fn)(int slot, int dequeue, void *ud);
  void (*qb_on_complete_fn)(int slot, int code, void *ud);
};
typedef struct qb_disp_cmds qb_disp_cmds_t;

void *root_node_create(node_t *newnode, void *parent_pool, qb_command_t cmd);

int qbuild_is_done(node_t any_node);

void qbuild_update_once(node_t any_node);

void qbuild_wait_all(node_t any_node);

void qbuild_log_stderr(node_t root_node);

void qbuild_set_disp_cmds(node_t root_node, qb_disp_cmds_t *cmds);

// Embedded testing framework
void so_run_tests(so_file_t file, void *arg);

#endif
