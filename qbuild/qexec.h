#ifndef LIBQ_QEXEC_H
#define LIBQ_QEXEC_H

#include "qb.h"
#include "qbuild.h"

#include <apr_file_io.h>
#include <apr_hash.h>
#include <apr_tables.h>

#include <setjmp.h>
#include <stdbool.h>

typedef struct arg_map arg_map_t;
typedef struct arg_list arg_list_t;
typedef struct exec_unit exec_unit_t;

struct arg_list {
  uint8_t id;              // 0x80 | 'l'
  apr_array_header_t *arr; // char*
};

struct arg_map {
  uint8_t id;       // 0x80 | 'm'
  apr_hash_t *hash; // char*
};

struct stream_chunk {
  char *buf;
  apr_size_t n;
};
typedef struct stream_chunk stream_chunk_t;

struct qbinfo {
  int nexecunits, maxexecunits;
  // int cmdc;
  // const char **cmdv;
  qb_command_t cmd;
  exec_unit_t *head, *tail;

  void *ud;
  apr_pool_t *logpool;
  apr_file_t *logstderr, *logfile;
  // qbuild_log_fn_t *logfn;
  qb_disp_cmds_t disp;
  bool isrelease;
};
typedef struct qbinfo qbinfo_t;

struct catch {
  jmp_buf env;
  apr_pool_t *pool;
};
typedef struct catch catch_t;

struct node {
  qbinfo_t *qbinfo;
  // step_type_t type;
  apr_pool_t *pool;
  catch_t *catch;
};

typedef void (*process_fn_t)(node_t node, void *baton, int len, char *str);
typedef arg_list_t *(*build_args_fn_t)(node_t node, void *baton);

void qbinfo_logf(qbinfo_t *qbinfo, const char *fmt, ...);

void arg_list_vadd(arg_list_t *args, apr_pool_t *pool, va_list ap);
void arg_list_add(arg_list_t *args, apr_pool_t *pool, ...);

void arg_map_vadd(arg_map_t *args, apr_pool_t *pool, va_list ap);
void arg_map_add(arg_map_t *args, apr_pool_t *pool, ...);

exec_unit_t *exec_unit_init(node_t node, process_fn_t process,
                            build_args_fn_t build_args);

void exec_unit_mark_skip(exec_unit_t *unit);

void exec_unit_set_baton(exec_unit_t *unit, void *baton);

void exec_add_dep(exec_unit_t *node, exec_unit_t *dep);

void exec_unit_submit(exec_unit_t *unit);

void exec_unit_wait(exec_unit_t *unit);

#endif
