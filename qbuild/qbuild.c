#include "qbuild.h"
#include "qb.h"
#include "qtest.h"

#include <apr.h>
#include <apr_dso.h>
#include <apr_file_io.h>
#include <apr_hash.h>
#include <apr_strings.h>
#include <apr_tables.h>
#include <apr_thread_proc.h>

#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>

enum step_type {
  STEP_PCDEP,
  STEP_SRC,
  STEP_OBJFILE,
  STEP_EXE,
  STEP_SO,
  STEP_SOFILE,
  STEP_ARFILE,
};
typedef enum step_type step_type_t;

struct arg_list {
  uint8_t id;              // 0x80 | 'l'
  apr_array_header_t *arr; // char*
};
typedef struct arg_list arg_list_t;

struct arg_map {
  uint8_t id;       // 0x80 | 'm'
  apr_hash_t *hash; // char*
};
typedef struct arg_map arg_map_t;

typedef void (*process_fn_t)(node_t node, void *baton);
typedef arg_list_t *(*build_args_fn_t)(node_t node, void *baton);

typedef struct exec_unit exec_unit_t;
struct exec_unit {
  apr_pool_t *pool;
  apr_proc_t proc;
  // qbinfo_t *qbinfo;
  node_t parent;
  apr_array_header_t *stdout_buffer, *stderr_buffer;
  apr_size_t stdout_total, stderr_total;
  char *cmdline;
  enum {
    EXEC_UNIT_RUNNING,
    EXEC_UNIT_SUCCESS,
    EXEC_UNIT_FAILURE,
    EXEC_UNIT_CRASH,
    EXEC_UNIT_SKIPPED,
  } status;
  void *baton;
  process_fn_t process;
  build_args_fn_t build_args;
  bool processed, queued;
  // process is called when all deps have completed
  apr_array_header_t *deps, *rdeps;
  exec_unit_t *next;
};

struct qbinfo {
  int nexecunits, maxexecunits;
  // int cmdc;
  // const char **cmdv;
  qb_command_t cmd;
  exec_unit_t *head, *tail;

  void *ud;
  apr_pool_t *logpool;
  apr_file_t *logstderr, *logfile;
  qbuild_log_fn_t *logfn;
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
  step_type_t type;
  apr_pool_t *pool;
  catch_t *catch;
};

#define STREAM_CHUNK_NBYTES 1024
struct stream_chunk {
  char *buf;
  apr_size_t n;
};
typedef struct stream_chunk stream_chunk_t;

/*
struct step_core {
  step_type_t type;
  node_t *node;
  apr_pool_t *pool;
};
typedef struct step_core step_core_t;
*/

struct pcdep_baton {
  arg_list_t args_in;
  arg_list_t args_out;
  exec_unit_t *exec_unit;
};
typedef struct pcdep_baton pcdep_baton_t;

struct pcdep {
  struct node node;
  // apr_array_header_t *cflags_args, *libs_args;
  pcdep_baton_t cflags, libs;
  char *depname;
};

struct src_step {
  struct node node;
  arg_list_t args;
  // apr_hash_t *cflags_args, *libs_args;
  apr_array_header_t *deps; // pcdep_t[]
  char *c_path, *compiled_path;
  exec_unit_t *exec_unit;
  apr_time_t mtime;
};

struct linkable {
  struct node node;
  arg_map_t libs_args;
  exec_unit_t *exec_unit;
  apr_array_header_t **deps; // pcdep_t[]
  apr_time_t mtime;
  bool PIC;
  char *path;
};

/*
struct obj_file {
  step_core_t core;
  apr_hash_t *libs_args;
  exec_unit_t *exec_unit;
  char *path;
};

struct ar_file {
  step_core_t core;
  char *arname;
};
*/

struct exe_step {
  struct node node;
  arg_list_t args;
  apr_array_header_t *linkables;
  arg_map_t libs_args;
  exec_unit_t *exec_unit;
  char *exename;
  apr_time_t mtime;
};

struct so_step {
  struct node node;
  arg_list_t args;
  apr_array_header_t *linkables;
  arg_map_t libs_args;
  exec_unit_t *exec_unit;
  char *soname;
  apr_time_t mtime;
  bool skip_test;
};

struct so_file {
  struct node node;
  apr_dso_handle_t *dso;
  exec_unit_t *exec_unit;
  char *soname;
  apr_time_t mtime;
};

/*
union step {
  node_t node;
  pcdep_t pcdep;
  src_step_t src_step;
  linkable_t linkable;
  exe_step_t exe_step;
  so_step_t so_step;
  so_file_t so_file;
};
typedef union step step_t;
*/

void internal_throw(catch_t *catch, int rc) {
  longjmp(catch->env, rc); //
}

void qbuild_throw(node_t node, int rc) {
  internal_throw(node->catch, rc); //
}

void _qbuild_assert(node_t node, const char *file, const int line) {
  qbuild_logf(node, "Build failed in %s at line %d\n", file, line);
  qbuild_throw(node, -1);
}

/*
void print_args(const char *pre, const char *const *argv) {
  fputs(pre, stdout);
  for (int i = 0; argv[i]; i++) {
    if (i > 0)
      putchar(' ');

    fputs(argv[i], stdout);
  }

  putchar('\n');
}
*/

int arg_map_combine_do_callback_fn_t(void *rec, const void *key,
                                     apr_ssize_t klen, const void *value) {
  arg_map_t *dst = rec;
  if (key)
    apr_hash_set(dst->hash, key, klen, (void *)1);
  return 1;
}

int arg_list_append_map_do_callback_fn_t(void *rec, const void *key,
                                         apr_ssize_t klen, const void *value) {
  arg_list_t *dst = rec;
  if (key)
    APR_ARRAY_PUSH(dst->arr, char *) = (char *)key;
  return 1;
}

void arg_list_vadd(arg_list_t *args, apr_pool_t *pool, va_list ap) {
  if (args->arr == NULL) {
    args->id = 0x80 | 'l';
    args->arr = apr_array_make(pool, 8, sizeof(char *));
  }

  const char *s;
  while ((s = va_arg(ap, const char *)) != NULL) {
    if (s == NULL)
      continue;

    uint8_t id = s[0];
    switch (id) {
    case 0:
      // fprintf(stderr, "tried adding empty string to arg list\n");
      break;
    case (0x80 | 'l'): {
      arg_list_t *in = (arg_list_t *)s;
      if (in->arr) {
        for (int e = 0; e < in->arr->nelts; ++e) {
          const char *newarg = APR_ARRAY_IDX(in->arr, e, const char *);
          APR_ARRAY_PUSH(args->arr, const char *) = apr_pstrdup(pool, newarg);
        }
      }
    } break;
    case (0x80 | 'm'): {
      arg_map_t *in = (arg_map_t *)s;
      apr_hash_do(arg_list_append_map_do_callback_fn_t, args, in->hash);
    } break;
    default: {
      APR_ARRAY_PUSH(args->arr, const char *) = apr_pstrdup(pool, s);
    } break;
    }
  }
}

void arg_list_add(arg_list_t *args, apr_pool_t *pool, ...) {
  va_list ap;
  va_start(ap, pool);
  arg_list_vadd(args, pool, ap);
  va_end(ap);
}

void arg_map_vadd(arg_map_t *args, apr_pool_t *pool, va_list ap) {
  if (args->hash == NULL) {
    args->id = 0x80 | 'm';
    args->hash = apr_hash_make(pool);
  }

  const char *s;
  while ((s = va_arg(ap, const char *)) != NULL) {
    if (s == NULL)
      continue;

    uint8_t id = s[0];
    switch (id) {
    case 0:
      // fprintf(stderr, "tried adding empty string to arg map\n");
      break;
    case (0x80 | 'l'): {
      arg_list_t *in = (arg_list_t *)s;
      if (in->arr) {
        for (int e = 0; e < in->arr->nelts; e++) {
          const char *newarg = APR_ARRAY_IDX(in->arr, e, const char *);
          apr_hash_set(args->hash, apr_pstrdup(pool, newarg),
                       APR_HASH_KEY_STRING, (void *)1);
        }
      }
    } break;
    case (0x80 | 'm'): {
      arg_map_t *in = (arg_map_t *)s;
      apr_hash_do(arg_map_combine_do_callback_fn_t, args, in->hash);
    } break;
    default:
      apr_hash_set(args->hash, apr_pstrdup(pool, s), APR_HASH_KEY_STRING,
                   (void *)1);
      break;
    }
  }
}

void arg_map_add(arg_map_t *args, apr_pool_t *pool, ...) {
  va_list ap;
  va_start(ap, pool);
  arg_map_vadd(args, pool, ap);
  va_end(ap);
}

apr_time_t get_mtime(const char *path, apr_pool_t *pool) {
  apr_finfo_t finfo;
  apr_status_t rc = apr_stat(&finfo, path, APR_FINFO_MTIME, pool);
  if (rc != APR_SUCCESS)
    return 0;
  return finfo.mtime;
}

void qbuild_log_stderr(node_t node) {
  apr_file_open_stderr(&node->qbinfo->logstderr, node->qbinfo->logpool);
}

void qbuild_log_file(node_t node, const char *path) {
  apr_file_open(&node->qbinfo->logfile, path, APR_FOPEN_WRITE,
                APR_FPROT_OS_DEFAULT, node->qbinfo->logpool);
}

void qbuild_log_fn(node_t node, qbuild_log_fn_t *fn, void *ud) {
  node->qbinfo->logfn = fn;
  node->qbinfo->ud = ud;
}

void qbuild_vlogf(qbinfo_t *qbinfo, const char *fmt, va_list ap) {
  const char *out = apr_pvsprintf(qbinfo->logpool, fmt, ap);
  if (qbinfo->logstderr)
    apr_file_puts(out, qbinfo->logstderr);
  if (qbinfo->logfile)
    apr_file_puts(out, qbinfo->logfile);
  if (qbinfo->logfn)
    qbinfo->logfn(out, qbinfo->ud);
}

void qbinfo_logf(qbinfo_t *qbinfo, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  qbuild_vlogf(qbinfo, fmt, ap);
  va_end(ap);
}

void qbuild_logf(node_t node, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  qbuild_vlogf(node->qbinfo, fmt, ap);
  va_end(ap);
}

void qbuild_init() { apr_initialize(); }
void qbuild_quit() { apr_terminate(); }

// void node_throw(node_t *node, int rc) { node_throw(node, rc); }

node_t subnode_create(node_t parent, apr_size_t sz) {
  apr_pool_t *pool;
  apr_pool_create(&pool, parent->catch->pool);
  node_t node = apr_pcalloc(pool, sz);
  node->pool = pool;
  node->qbinfo = parent->qbinfo;
  node->catch = parent->catch;

  // apr_dir_make_recursive("build_cache/", APR_FPROT_OS_DEFAULT, pool);

  return node;
}

void node_include_subdir(node_t node, const char *path) {
  src_step_t build_c = src_create(node, "build.c");
  src_add_args(build_c, "-g");
  linkable_t build_obj = src_compile(build_c);

  so_step_t build_so_step =
      so_create(node, apr_pstrcat(node->pool, path, "/", "build.so", NULL));
  so_add_libs(build_so_step, "qbuild");
  so_add_linkables(build_so_step, build_obj);
  so_file_t build_so = so_link(build_so_step);

  /*
  src_destroy(build_c);
  linkable_destroy(build_obj);
  so_destroy(build_so_step);
  */

  // for (int i = 0; i < node->qbinfo->cmdc; ++i) {
  // if (strcmp(node->qbinfo->cmdv[i], "build") == 0) {
  if (node->qbinfo->cmd == QB_BUILD) {
    node_t subnode = subnode_create(node, sizeof(node_t));
    so_run_build(build_so, subnode);
    /*} else if (strcmp(node->qbinfo->cmdv[i], "test") == 0) {
      node_t *subnode = subnode_create(node, sizeof(node_t));
      so_run_tests(build_so, subnode);*/
  } else {
    qbuild_logf(node, "Unknown command %d\n", node->qbinfo->cmd);
  }
  // node_destroy(subnode);
  // }
}

void exec_unit_push(exec_unit_t *unit) {
  qbinfo_t *qbinfo = unit->parent->qbinfo;
  if (qbinfo->head == NULL) {
    qbinfo->head = unit;
  } else {
    qbinfo->tail->next = unit;
  }
  qbinfo->tail = unit;
}

exec_unit_t *exec_unit_pop(qbinfo_t *qbinfo) {
  if (qbinfo->head == NULL)
    return NULL;
  exec_unit_t *ret = qbinfo->head;
  qbinfo->head = qbinfo->head->next;
  if (qbinfo->head == NULL)
    qbinfo->tail = NULL;
  return ret;
}

apr_size_t exec_unit_io_update_single(apr_file_t *pipe,
                                      apr_array_header_t *buffer) {
  apr_size_t n, total = 0;
  int rc;
  do {
    stream_chunk_t *chunk =
        &APR_ARRAY_IDX(buffer, buffer->nelts - 1, stream_chunk_t);
    n = STREAM_CHUNK_NBYTES - chunk->n;
    if (n == 0) {
      n = STREAM_CHUNK_NBYTES;
      chunk = &APR_ARRAY_PUSH(buffer, stream_chunk_t);
      chunk->buf = apr_palloc(buffer->pool, STREAM_CHUNK_NBYTES);
      chunk->n = 0;
    }
    rc = apr_file_read(pipe, chunk->buf, &n);
    if (rc == APR_SUCCESS) {
      total += n;
      chunk->n += n;
    }
    // if (rc == APR_EOF)
    // qbuild_logf(node, "EOF Found!\n");
  } while (rc == APR_SUCCESS);
  return total;
}

void exec_unit_io_update(exec_unit_t *unit) {
  unit->stdout_total +=
      exec_unit_io_update_single(unit->proc.out, unit->stdout_buffer);
  unit->stderr_total +=
      exec_unit_io_update_single(unit->proc.err, unit->stderr_buffer);
}

void exec_unit_maint(int reason, void *ud, int code);
void exec_check_rdeps(exec_unit_t *node);
void exec_unit_flush(qbinfo_t *qbinfo);

void exec_unit_run(exec_unit_t *unit) {
  /*
  apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
  qbuild_logf(node, "nexecunits: %d | maxexecunits %d\n",
  unit->parent->qbinfo->nexecunits, unit->parent->qbinfo->maxexecunits); while
  (unit->parent->qbinfo->nexecunits >= unit->parent->qbinfo->maxexecunits) {
    apr_sleep(500);
    apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
  }
  */

  arg_list_t *args = unit->build_args(unit->parent, unit->baton);
  unit->cmdline = apr_array_pstrcat(unit->pool, args->arr, ' ');
  const char **null_elt = apr_array_push(args->arr);
  *null_elt = NULL;
  const char *const *argv = (const char *const *)args->arr->elts;

  if (unit->status == EXEC_UNIT_SKIPPED) {
    qbuild_logf(unit->parent, "Skip: %s %pp\n", unit->cmdline, unit);
    unit->processed = true;
    exec_check_rdeps(unit);
  } else {
    unit->status = EXEC_UNIT_RUNNING;
    qbuild_logf(unit->parent, "Exec: %s %pp\n", unit->cmdline, unit);

    apr_procattr_t *procattr;
    apr_procattr_create(&procattr, unit->pool);
    apr_procattr_cmdtype_set(procattr, APR_PROGRAM_PATH);
    apr_procattr_io_set(procattr, APR_NO_PIPE, APR_CHILD_BLOCK,
                        APR_CHILD_BLOCK);
    apr_proc_create(&unit->proc, argv[0], argv, NULL, procattr, unit->pool);
    apr_proc_other_child_register(&unit->proc, exec_unit_maint, unit,
                                  unit->proc.out, unit->pool);

    stream_chunk_t *chunk;
    unit->stdout_buffer = apr_array_make(unit->pool, 1, sizeof(stream_chunk_t));
    chunk = &APR_ARRAY_PUSH(unit->stdout_buffer, stream_chunk_t);
    chunk->buf = apr_palloc(unit->pool, STREAM_CHUNK_NBYTES);
    chunk->n = 0;
    unit->stdout_total = 0;
    unit->stderr_buffer = apr_array_make(unit->pool, 1, sizeof(stream_chunk_t));
    chunk = &APR_ARRAY_PUSH(unit->stderr_buffer, stream_chunk_t);
    chunk->buf = apr_palloc(unit->pool, STREAM_CHUNK_NBYTES);
    chunk->n = 0;
    unit->stderr_total = 0;
    unit->parent->qbinfo->nexecunits += 1;
  }
}

void exec_add_dep(exec_unit_t *node, exec_unit_t *dep) {
  // qbuild_assert(node, node->parent);
  qbuild_assert(dep, node->parent);

  if (node->deps == NULL)
    node->deps = apr_array_make(node->pool, 4, sizeof(node_t *));

  if (dep->rdeps == NULL)
    dep->rdeps = apr_array_make(dep->pool, 4, sizeof(node_t *));

  *(exec_unit_t **)apr_array_push(node->deps) = dep;
  *(exec_unit_t **)apr_array_push(dep->rdeps) = node;
}

void exec_unit_flush(qbinfo_t *qbinfo) {
  while (qbinfo->nexecunits < qbinfo->maxexecunits) {
    exec_unit_t *torun = exec_unit_pop(qbinfo);
    if (torun) {
      qbinfo_logf(qbinfo, "Run from head\n");
      exec_unit_run(torun);
    } else
      break;
  }
}

void exec_check_deps(exec_unit_t *unit) {
  exec_unit_flush(unit->parent->qbinfo);
  qbuild_logf(unit->parent, "Check deps for %pp\n", unit->parent);
  if (unit && unit->deps) {
    for (int d = 0; d < unit->deps->nelts; ++d) {
      exec_unit_t *dep = APR_ARRAY_IDX(unit->deps, d, exec_unit_t *);
      if (dep->processed == false)
        return;
    }
  }

  if (unit->queued)
    return;
  unit->queued = true;

  qbinfo_t *qbinfo = unit->parent->qbinfo;
  if (qbinfo->nexecunits < qbinfo->maxexecunits) {
    if (qbinfo->head) {
      qbinfo_logf(qbinfo, "Run from head\n");
      exec_unit_run(exec_unit_pop(qbinfo));
    } else {
      qbinfo_logf(qbinfo, "Run current\n");
      exec_unit_run(unit);
    }
  } else {
    qbinfo_logf(qbinfo, "Push to tail\n");
    exec_unit_push(unit);
  }
}

void exec_check_rdeps(exec_unit_t *node) {
  // qbuild_assert(node, node->parent);
  exec_unit_flush(node->parent->qbinfo);
  qbuild_logf(node->parent, "Check rdeps for %pp\n", node->parent);
  if (node->rdeps) {
    for (int r = 0; r < node->rdeps->nelts; ++r) {
      exec_unit_t *rdep = APR_ARRAY_IDX(node->rdeps, r, exec_unit_t *);
      exec_check_deps(rdep);
    }
  }
}

void log_chunk_by_line(stream_chunk_t *chunk, node_t node) {
  qbuild_logf(node, "%.*s", chunk->n, chunk->buf);
}

void exec_unit_maint(int reason, void *ud, int code) {
  exec_unit_t *unit = ud;
  switch (reason) {
  case APR_OC_REASON_RUNNING:
    exec_unit_io_update(unit);
    break;
  case APR_OC_REASON_UNREGISTER:
    // qbuild_logf(node, "unreg %pp\n", unit);
    return;
  case APR_OC_REASON_DEATH:
  case APR_OC_REASON_LOST:
    unit->parent->qbinfo->nexecunits -= 1;
    qbuild_logf(unit->parent, "Completed: %pp\n", unit);
    exec_unit_io_update(unit);
    apr_proc_other_child_unregister(unit);
    if (code == 0) {
      unit->status = EXEC_UNIT_SUCCESS;
      unit->process(unit->parent, unit->baton);
      unit->processed = true;
      exec_check_rdeps(unit);
    } else {
      unit->status = EXEC_UNIT_FAILURE;
      qbuild_logf(unit->parent, "Failed with code %d\n", code);
      qbuild_logf(unit->parent, "CMD: %s\n", unit->cmdline);
      if (unit->stdout_buffer && unit->stdout_buffer->nelts > 0 &&
          unit->stdout_total > 0) {
        for (int e = 0; e < unit->stdout_buffer->nelts; ++e) {
          stream_chunk_t *chunk =
              &APR_ARRAY_IDX(unit->stdout_buffer, e, stream_chunk_t);
          log_chunk_by_line(chunk, unit->parent);
        }
        qbuild_logf(unit->parent, "\n");
      }
      if (unit->stderr_buffer && unit->stderr_buffer->nelts > 0 &&
          unit->stderr_total > 0) {
        for (int e = 0; e < unit->stderr_buffer->nelts; ++e) {
          stream_chunk_t *chunk =
              &APR_ARRAY_IDX(unit->stderr_buffer, e, stream_chunk_t);
          log_chunk_by_line(chunk, unit->parent);
        }
        qbuild_logf(unit->parent, "\n");
      }
      qbuild_throw(unit->parent, code);
    }
    break;
  }
  if (reason != APR_OC_REASON_RUNNING) {
    // qbuild_logf(node, "MAINT CALLED!!! %d %d %pp\n", reason, code, ud);
  }
}

void exec_unit_mark_skip(exec_unit_t *unit) {
  unit->status = EXEC_UNIT_SKIPPED;
}

exec_unit_t *exec_unit_init(node_t node, process_fn_t process,
                            build_args_fn_t build_args) {
  apr_pool_t *pool;
  apr_pool_create(&pool, node->pool);
  exec_unit_t *unit = apr_pcalloc(pool, sizeof(*unit));
  unit->pool = pool;
  unit->parent = node;
  unit->build_args = build_args;
  unit->process = process;

  return unit;
}

void exec_unit_set_baton(exec_unit_t *unit, void *baton) {
  unit->baton = baton;
}

void exec_unit_submit(exec_unit_t *unit) {
  if (unit->queued) // Not necessary, but shortcuts checking
    return;
  exec_check_deps(unit); //
}

void exec_unit_wait(exec_unit_t *unit) {
  if (unit->status == EXEC_UNIT_SKIPPED)
    return;

  apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
  while (unit->status == EXEC_UNIT_RUNNING) {
    apr_sleep(500);
    apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
    exec_unit_flush(unit->parent->qbinfo);
  }
}

int qbuild_is_done(node_t node) {
  return !(node->qbinfo->nexecunits > 0 || node->qbinfo->head != NULL);
}

void qbuild_update_once(node_t node) {
  apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
  exec_unit_flush(node->qbinfo);
}

void qbuild_wait_all(node_t node) {
  // apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
  qbuild_update_once(node);
  qbuild_logf(node, "Waiting on all...\n");
  qbuild_logf(node, "nexecunits: %d | maxexecunits %d\n",
              node->qbinfo->nexecunits, node->qbinfo->maxexecunits);
  while (!qbuild_is_done(node)) {
    apr_sleep(500);
    qbuild_update_once(node);
  }
}

void exec_unit_destroy(exec_unit_t *unit) {
  apr_pool_destroy(unit->pool); //
}

void pcdep_process(node_t node, void *vbaton) {
  pcdep_t dep = (pcdep_t)node;
  pcdep_baton_t *baton = vbaton;
  exec_unit_t *unit = baton->exec_unit;
  char *output = apr_palloc(unit->pool, unit->stdout_total + 1);

  int total = 0;
  for (int i = 0; i < unit->stdout_buffer->nelts; i++) {
    stream_chunk_t *elt = &((stream_chunk_t *)unit->stdout_buffer->elts)[i];
    memmove(&output[total], elt->buf, elt->n);
    total += elt->n;
  }
  output[total] = '\0';

  char *saveptr = NULL;
  char *token;

  // arg_list_init(&baton->args_out, dep->node.pool);
  const char *seps = " \n\r\t";
  for (token = apr_strtok(output, seps, &saveptr); token != NULL;
       token = apr_strtok(NULL, seps, &saveptr)) {
    arg_list_add(&baton->args_out, dep->node.pool, token, NULL);
    // const char **elt = apr_array_push(pcargs);
    // *elt = apr_pstrdup(dep->node.pool, token);
  }

  qbuild_logf(node, "Done processing %s\n", dep->depname);

  // baton->args_out = pcargs;
  // exec_unit_destroy(unit);
}

arg_list_t *pcdep_build_args(node_t node, void *vbaton) {
  pcdep_baton_t *baton = vbaton;
  return &baton->args_in;
}

pcdep_t _pcdep_create(node_t node, ...) {
  pcdep_t dep = (void *)subnode_create(node, sizeof(*dep));

  apr_pool_t *argpool;
  apr_pool_create(&argpool, dep->node.pool);
  arg_list_t cflags_args = {0}, libs_args = {0};
  // apr_array_header_t *cflags_args = apr_array_make(argpool, 4, sizeof(char
  // *));
  arg_list_add(&cflags_args, argpool, "pkg-config", "--cflags", NULL);
  // apr_array_header_t *libs_args = apr_array_make(argpool, 4, sizeof(char *));
  arg_list_add(&libs_args, argpool, "pkg-config", "--libs", NULL);

  const char *pcname;
  va_list ap;
  va_start(ap, node);
  while ((pcname = va_arg(ap, const char *)) != NULL) {
    if (dep->depname == NULL)
      dep->depname = apr_pstrdup(argpool, pcname);
    arg_list_add(&cflags_args, argpool, pcname, NULL);
    arg_list_add(&libs_args, argpool, pcname, NULL);
  }
  va_end(ap);

  dep->cflags.args_in = cflags_args;
  dep->cflags.exec_unit =
      exec_unit_init(&dep->node, pcdep_process, pcdep_build_args);
  exec_unit_set_baton(dep->cflags.exec_unit, &dep->cflags);
  exec_unit_submit(dep->cflags.exec_unit);

  dep->libs.args_in = libs_args;
  dep->libs.exec_unit =
      exec_unit_init(&dep->node, pcdep_process, pcdep_build_args);
  exec_unit_set_baton(dep->libs.exec_unit, &dep->libs);
  exec_unit_submit(dep->libs.exec_unit);

  // apr_pool_destroy(argpool);
  return dep;
}

void _src_add_args(src_step_t step, ...) {
  va_list ap;
  va_start(ap, step);
  arg_list_vadd(&step->args, step->node.pool, ap);
  va_end(ap);
}

void _src_add_deps(src_step_t step, ...) {
  va_list ap;
  va_start(ap, step);
  pcdep_t dep;
  while ((dep = va_arg(ap, pcdep_t)) != NULL) {
    // cat_args_array(&step->cflags_args, dep->cflags_args, step->node.pool);
    // cat_args_array(&step->libs_args, dep->libs_args, step->node.pool);
    if (step->deps == NULL)
      step->deps = apr_array_make(step->node.pool, 4, sizeof(pcdep_t *));
    pcdep_t *step_dep = apr_array_push(step->deps);
    *step_dep = dep;
    qbuild_logf(&step->node, "Pushed %pp to %pp\n", dep, step->deps);
    exec_add_dep(step->exec_unit, dep->cflags.exec_unit);
  }
  va_end(ap);
}

arg_list_t *src_build_args(node_t node, void *vbaton) {
  src_step_t step = (src_step_t)node;
  linkable_t file = vbaton;

  if (file->PIC || !file->node.qbinfo->isrelease)
    src_add_args(step, "-fPIC");

  if (step->deps && step->deps->nelts > 0) {
    arg_map_t cflags_args = {0};
    for (int d = 0; d < step->deps->nelts; ++d) {
      pcdep_t dep = APR_ARRAY_IDX(step->deps, d, pcdep_t);
      // arg_map_append_list(&cflags_args, &dep->cflags.args_out, node->pool);
      arg_map_add(&cflags_args, node->pool, &dep->cflags.args_out, NULL);
    }

    arg_list_add(&step->args, node->pool, &cflags_args, NULL);
  }

  qbuild_logf(node, "src_build_args: %ld < %ld\n", step->mtime, file->mtime);
  if (step->mtime > 0 && step->mtime < file->mtime)
    exec_unit_mark_skip(step->exec_unit);

  return &step->args;
}

void src_process(node_t node, void *vbaton) {
  linkable_t file = vbaton;
  file->mtime = get_mtime(file->path, file->node.pool);
}

src_step_t src_create(node_t node, const char *path) {
  src_step_t step = (void *)subnode_create(node, sizeof(*step));

  step->c_path = apr_pstrdup(step->node.pool, path);
  step->compiled_path = apr_psprintf(step->node.pool, "build_cache/%s.o", path);

  // step->args = apr_array_make(step->node.pool, 8, sizeof(const char *));
  src_add_args(step, "gcc", "-c", "-o", step->compiled_path, path, "-Werror",
               NULL);
  if (!node->qbinfo->isrelease)
    src_add_args(step, "-DENABLE_TESTS");
  step->mtime = get_mtime(step->c_path, step->node.pool);

  step->exec_unit = exec_unit_init(&step->node, src_process, src_build_args);

  return step;
}

#define PATH_SEPARATOR '/'

/* Remove trailing separators that don't affect the meaning of PATH. */

static const char *path_canonicalize(const char *path, apr_pool_t *pool) {
  /* At some point this could eliminate redundant components.  For
   * now, it just makes sure there is no trailing slash. */

  apr_size_t len = strlen(path);
  apr_size_t orig_len = len;
  while ((len > 0) && (path[len - 1] == PATH_SEPARATOR))
    len--;

  if (len != orig_len)
    return apr_pstrndup(pool, path, len);
  else
    return path;
}

/* Remove one component off the end of PATH. */
static char *path_remove_last_component(const char *path, apr_pool_t *pool) {
  const char *newpath = path_canonicalize(path, pool);
  int i;
  for (i = (strlen(newpath) - 1); i >= 0; i--) {
    if (path[i] == PATH_SEPARATOR)
      break;
  }

  return apr_pstrndup(pool, path, (i < 0) ? 0 : i);
}

linkable_t src_compile(src_step_t step) {
  apr_procattr_t *procattr;
  apr_procattr_create(&procattr, step->node.pool);
  apr_procattr_cmdtype_set(procattr, APR_PROGRAM_PATH);

  linkable_t file = (void *)subnode_create(&step->node, sizeof(*file));

  file->node.type = STEP_OBJFILE;

  file->path = apr_pstrdup(file->node.pool, step->compiled_path);
  file->exec_unit = step->exec_unit;
  qbuild_logf(&step->node, "Setting deps\n");
  file->deps = &step->deps;
  file->mtime = get_mtime(file->path, file->node.pool);

  exec_unit_set_baton(step->exec_unit, file);

  const char *dirpath = path_remove_last_component(file->path, file->node.pool);
  apr_dir_make_recursive(dirpath, APR_FPROT_OS_DEFAULT, file->node.pool);

  // exec_unit_submit(step->exec_unit, file);
  // NOTE: ^ was moved to vadd_linkables (to determine if PIC or not)

  // cat_args(&step->libs_args, b->libs_args, step->pool);
  // file->libs_args = NULL;
  // if (step->libs_args)
  // file->libs_args = apr_hash_copy(file->node.pool, step->libs_args);

  return file;
}

/*
void src_destroy(src_step_t *step) {
  node_destroy(&step->node); //
}

void linkable_destroy(linkable_t *file) {
  node_destroy(&file->node); //
}
*/

linkable_t ar_import(node_t node, const char *path) {
  linkable_t file = (void *)subnode_create(node, sizeof(*file));
  file->node.type = STEP_ARFILE;

  file->path = apr_pstrdup(file->node.pool, path);
  return file;
}

void _exe_add_args(exe_step_t step, ...) {
  va_list ap;
  va_start(ap, step);
  arg_list_vadd(&step->args, step->node.pool, ap);
  va_end(ap);
}

void _exe_add_libs(exe_step_t step, ...) {
  va_list ap;
  va_start(ap, step);

  const char *s;
  while ((s = va_arg(ap, const char *)) != NULL) {
    arg_map_add(&step->libs_args, step->node.pool,
                apr_pstrcat(step->node.pool, "-l", s, NULL), NULL);
  }
  va_end(ap);
}

bool complete_builds(apr_array_header_t *linkables, arg_list_t *args,
                     arg_map_t *libs_args, node_t node, const char *outfile,
                     apr_time_t mtime, bool skip_test) {
  apr_pool_t *pool = node->pool;

  so_step_t test = NULL;

  // Create the test library to run when the user calls "qb test"
  if (!node->qbinfo->isrelease && !skip_test) {
    const char *test_path = "build_cache/tests/";
    apr_dir_make_recursive(test_path, APR_FPROT_OS_DEFAULT, pool);
    test = so_create(node, apr_pstrcat(pool, test_path, outfile, ".so", NULL));
    qbuild_logf(node, "Setting up test: %pp\n", test);
    test->skip_test = true;
  }

  bool skip = mtime > 0;
  for (int i = 0; i < linkables->nelts; ++i) {
    linkable_t file = APR_ARRAY_IDX(linkables, i, linkable_t);
    arg_list_add(args, pool, file->path, NULL);

    if (file->exec_unit) {
      // exec_unit_wait(file->exec_unit);
      qbuild_logf(node, "complete_builds: %ld > %ld\n", file->mtime, mtime);
      if (file->mtime > mtime)
        skip = false;
    }

    if (test)
      so_add_linkables(test, file);

    if (file->deps && *file->deps) {
      apr_array_header_t *deps = *file->deps;
      for (int d = 0; d < deps->nelts; ++d) {
        pcdep_t dep = APR_ARRAY_IDX(deps, d, pcdep_t);
        if (dep->libs.args_out.arr && dep->libs.args_out.arr->nelts > 0) {
          qbuild_logf(node, "adding %pp to %pp\n", dep, libs_args);
          arg_map_add(libs_args, pool, &dep->libs.args_out, NULL);
        }
        // arg_map_qbuild_logf(node, libs_args);
      }
    }
  }

  if (test)
    so_link(test);

  arg_list_add(args, pool, libs_args, NULL);
  return skip;
}

arg_list_t *exe_build_args(node_t node, void *vbaton) {
  exe_step_t step = (exe_step_t)node;
  if (complete_builds(step->linkables, &step->args, &step->libs_args,
                      &step->node, step->exename, step->mtime, false))
    exec_unit_mark_skip(step->exec_unit);
  return &step->args;
}

void exe_process(node_t node, void *vbaton) {
  // Does nothing
}

exe_step_t exe_create(node_t node, const char *exename) {
  exe_step_t step = (exe_step_t)subnode_create(node, sizeof(*step));

  step->exename = apr_pstrdup(step->node.pool, exename);

  // step->args = apr_array_make(step->node.pool, 8, sizeof(const char *));
  exe_add_args(step, "gcc", "-o", step->exename, NULL);
  step->mtime = get_mtime(step->exename, step->node.pool);

  step->exec_unit = exec_unit_init(&step->node, exe_process, exe_build_args);

  return step;
}

void vadd_linkables(apr_array_header_t **linkables, va_list ap,
                    apr_pool_t *pool, bool PIC) {
  linkable_t linkable;
  while ((linkable = va_arg(ap, linkable_t)) != NULL) {
    if (*linkables == NULL)
      *linkables = apr_array_make(pool, 8, sizeof(linkable_t));

    // TODO: Find some way to add -fPIC to the args if release && executable
    linkable->PIC = PIC;
    exec_unit_submit(linkable->exec_unit);

    // qbuild_logf(node, "adding linkable %d\n", (*linkables)->nelts);
    linkable_t *elt = apr_array_push(*linkables);
    *elt = linkable;
  }
}

void _exe_add_linkables(exe_step_t step, ...) {
  va_list ap;
  va_start(ap, step);
  vadd_linkables(&step->linkables, ap, step->node.pool, false);
  va_end(ap);
}

void exe_link(exe_step_t step) {
  if (step->linkables && step->linkables->nelts > 0)
    for (int l = 0; l < step->linkables->nelts; ++l) {
      linkable_t linkable = APR_ARRAY_IDX(step->linkables, l, linkable_t);
      if (linkable->exec_unit)
        exec_add_dep(step->exec_unit, linkable->exec_unit);
    }

  exec_unit_submit(step->exec_unit);
}

/*
void exe_destroy(exe_step_t *step) {
  node_destroy(&step->node); //
}
*/

void _so_add_args(so_step_t step, ...) {
  va_list ap;
  va_start(ap, step);
  arg_list_vadd(&step->args, step->node.pool, ap);
  va_end(ap);
}

void _so_add_libs(so_step_t step, ...) {
  va_list ap;
  va_start(ap, step);
  // if (step->libs_args.hash == NULL)
  // arg_map_init(&step->libs_args, step->node.pool);

  const char *s;
  while ((s = va_arg(ap, const char *)) != NULL) {
    // apr_hash_set(step->libs_args, apr_pstrcat(step->node.pool, "-l", s,
    // NULL), APR_HASH_KEY_STRING, (void *)1);
    arg_map_add(&step->libs_args, step->node.pool,
                apr_pstrcat(step->node.pool, "-l", s, NULL), NULL);
  }
  va_end(ap);
}

arg_list_t *so_build_args(node_t node, void *vbaton) {
  so_step_t step = (so_step_t)node;
  if (complete_builds(step->linkables, &step->args, &step->libs_args,
                      &step->node, step->soname, step->mtime, step->skip_test))
    exec_unit_mark_skip(step->exec_unit);
  return &step->args;
}

void so_process(node_t node, void *vbaton) {
  so_file_t outfile = vbaton;
  outfile->mtime = get_mtime(outfile->soname, outfile->node.pool);
}

so_step_t so_create(node_t node, const char *soname) {
  so_step_t step = (so_step_t)subnode_create(node, sizeof(*step));

  step->soname = apr_pstrdup(step->node.pool, soname);

  // step->args = apr_array_make(step->node.pool, 8, sizeof(const char *));
  so_add_args(step, "gcc", "-o", step->soname, "-shared", NULL);
  step->mtime = get_mtime(step->soname, step->node.pool);

  step->exec_unit = exec_unit_init(&step->node, so_process, so_build_args);

  return step;
}

void _so_add_linkables(so_step_t step, ...) {
  va_list ap;
  va_start(ap, step);
  vadd_linkables(&step->linkables, ap, step->node.pool, true);
  va_end(ap);
}

so_file_t so_link(so_step_t step) {
  if (step->linkables && step->linkables->nelts > 0)
    for (int l = 0; l < step->linkables->nelts; ++l) {
      linkable_t linkable = APR_ARRAY_IDX(step->linkables, l, linkable_t);
      if (linkable->exec_unit)
        exec_add_dep(step->exec_unit, linkable->exec_unit);
    }

  so_file_t outfile = (so_file_t)subnode_create(&step->node, sizeof(*outfile));

  outfile->dso = NULL;
  outfile->soname = apr_pstrdup(outfile->node.pool, step->soname);
  outfile->exec_unit = step->exec_unit;

  exec_unit_set_baton(step->exec_unit, outfile);
  exec_unit_submit(step->exec_unit);

  return outfile;
}

/*
void so_destroy(so_step_t *step) {
  node_destroy(&step->node); //
}
*/

void *root_node_create(node_t *newnode, void *parent_pool, qb_command_t cmd) {
  qbinfo_t *qbinfo;
  apr_pool_t *pool;
  apr_pool_create(&pool, parent_pool);
  qbinfo = apr_pcalloc(pool, sizeof(*qbinfo));
  qbinfo->maxexecunits = 4;
  // qbinfo->cmdc = cmdc;
  // qbinfo->cmdv = cmdv;
  qbinfo->cmd = cmd;
  qbinfo->logpool = pool;
  apr_dir_make_recursive("build_cache/", APR_FPROT_OS_DEFAULT, pool);

  node_t node = apr_pcalloc(pool, sizeof(*node));
  node->pool = pool;
  node->qbinfo = qbinfo;
  node->catch = apr_palloc(pool, sizeof(catch_t));
  node->catch->pool = pool;

  *newnode = node;
  return &node->catch->env;
}

void node_destroy(node_t node) {
  apr_pool_destroy(node->pool); //
}

void so_load_file(so_file_t file) {
  apr_status_t s;
  if (file->dso == NULL) {
    s = apr_dso_load(&file->dso, file->soname, file->node.pool);
    if (s != APR_SUCCESS) {
      char errbuf[256];
      qbuild_logf(&file->node, "FAILED TO LOAD DSO: %s %pp\n",
                  apr_strerror(s, errbuf, sizeof(errbuf)), file->dso);
      apr_dso_error(file->dso, errbuf, sizeof(errbuf));
      qbuild_logf(&file->node, "dso_load failed: %s\n", errbuf);
      qbuild_throw(&file->node, -1);
    }
  }
}

void *so_load_sym(so_file_t file, const char *sym) {
  void *ret = NULL;
  int rv = apr_dso_sym(&ret, file->dso, sym);
  if (rv != APR_SUCCESS) {
    char errbuf[256];
    apr_dso_error(file->dso, errbuf, sizeof(errbuf));
    qbuild_logf(&file->node, "dso_sym failed: %s\n", errbuf);
    qbuild_throw(&file->node, -1);
  }
  return ret;
}

typedef void (*build_fn_t)(void *);

void so_run_build(so_file_t file, void *arg) {
  exec_unit_wait(file->exec_unit); // TODO: Replace with a "on_complete"
  so_load_file(file);

  qbuild_logf(&file->node, "Building %s\n", file->soname);
  build_fn_t build_fn = so_load_sym(file, "build");

  build_fn(arg);
}

void so_run_tests(so_file_t file, void *arg) {
  exec_unit_wait(file->exec_unit); // TODO: Replace with a "on_complete"
  so_load_file(file);

  test_func_t *start_tests = so_load_sym(file, "__start_test_array"),
              *stop_tests = so_load_sym(file, "__stop_test_array");

  qbuild_logf(&file->node, "Main: Triggering test suite in %s...\n",
              file->soname);
  for (test_func_t *it = start_tests; it != stop_tests; it = &it[1])
    if (it && *it)
      (*it)();
  qbuild_logf(&file->node, "Main: Suite finished.\n");
}
