#include "qexec.h"

#include <apr.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <apr_thread_proc.h>

#include <stdbool.h>

#define STREAM_CHUNK_NBYTES 1024

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

// ------ Argument List / Map ------

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

// ------ Exec units ------

void exec_unit_run(exec_unit_t *unit);

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

void exec_unit_destroy(exec_unit_t *unit) {
  apr_pool_destroy(unit->pool); //
}

void exec_unit_mark_skip(exec_unit_t *unit) {
  unit->status = EXEC_UNIT_SKIPPED;
}

void exec_unit_set_baton(exec_unit_t *unit, void *baton) {
  unit->baton = baton;
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

void run_process(exec_unit_t *unit) {
  char *output = apr_palloc(unit->pool, unit->stdout_total + 1);

  int total = 0;
  for (int i = 0; i < unit->stdout_buffer->nelts; i++) {
    stream_chunk_t *elt = &((stream_chunk_t *)unit->stdout_buffer->elts)[i];
    memmove(&output[total], elt->buf, elt->n);
    total += elt->n;
  }
  output[total] = '\0';

  unit->process(unit->parent, unit->baton, total, output);
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
      if (unit->process)
        run_process(unit);
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

void exec_unit_submit(exec_unit_t *unit) {
  if (unit->queued) // Not necessary, but shortcuts checking
    return;
  exec_check_deps(unit); //
}

int qbuild_is_done(node_t node) {
  return !(node->qbinfo->nexecunits > 0 || node->qbinfo->head != NULL);
}

void exec_unit_wait(exec_unit_t *unit) {
  if (unit == NULL)
    return;

  if (unit->status == EXEC_UNIT_SKIPPED)
    return;

  apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
  while (unit->status == EXEC_UNIT_RUNNING) {
    apr_sleep(500);
    apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
    exec_unit_flush(unit->parent->qbinfo);
  }
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
