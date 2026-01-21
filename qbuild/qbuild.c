#include "qbuild.h"
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

struct qbinfo {
  int nexecunits, maxexecunits;
  int cmdc;
  const char **cmdv;
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

struct exec_unit {
  apr_pool_t *pool;
  apr_proc_t proc;
  // qbinfo_t *qbinfo;
  node_t *parent;
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
};
typedef struct exec_unit exec_unit_t;

/*
struct step_core {
  step_type_t type;
  node_t *node;
  apr_pool_t *pool;
};
typedef struct step_core step_core_t;
*/

struct pcdep {
  node_t node;
  apr_array_header_t *cflags_args, *libs_args;
};

struct src_step {
  node_t node;
  apr_array_header_t *args;
  apr_hash_t *cflags_args, *libs_args;
  char *c_path, *compiled_path;
};

struct linkable {
  node_t node;
  apr_hash_t *libs_args;
  exec_unit_t *exec_unit;
  bool impored;
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
  node_t node;
  apr_array_header_t *args, *linkables;
  apr_hash_t *libs_args;
  char *exename;
};

struct so_step {
  node_t node;
  apr_array_header_t *args, *linkables;
  apr_hash_t *libs_args;
  char *soname;
};

struct so_file {
  node_t node;
  apr_dso_handle_t *dso;
  char *soname;
};

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

void internal_throw(catch_t *catch, int rc) {
  longjmp(catch->env, rc); //
}

void qbuild_throw(node_t *node, int rc) {
  internal_throw(node->catch, rc); //
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

void cat_args_array(apr_hash_t **dst, apr_array_header_t *src,
                    apr_pool_t *dstpool) {
  if (src == NULL || dst == NULL)
    return;

  if (src->nelts <= 0)
    return;

  const char **src_elts = (const char **)src->elts;

  if (*dst == NULL) {
    // printf("making hash table\n");
    *dst = apr_hash_make(dstpool);
  }

  for (int i = 0; i < src->nelts; i++) {
    apr_hash_set(*dst, src_elts[i], APR_HASH_KEY_STRING, (void *)1);
  }
}

int cat_args_hash_do_callback_fn_t(void *rec, const void *key, apr_ssize_t klen,
                                   const void *value) {
  apr_hash_t *dst = rec;
  apr_hash_set(dst, key, klen, (void *)1);
  return 1;
}

void cat_args_hash(apr_hash_t **dst, apr_hash_t *src, apr_pool_t *dstpool) {
  if (src == NULL || dst == NULL)
    return;

  if (*dst == NULL) {
    *dst = apr_hash_copy(dstpool, src);
    return;
  }

  apr_hash_do(cat_args_hash_do_callback_fn_t, *dst, src);
}

int array_args_from_hash_do_callback_fn_t(void *rec, const void *key,
                                          apr_ssize_t klen, const void *value) {
  apr_array_header_t *dst = rec;
  APR_ARRAY_PUSH(dst, char *) = (char *)key;
  return 1;
}

void array_args_from_hash(apr_array_header_t **dst, apr_hash_t *src,
                          apr_pool_t *dstpool) {
  if (src == NULL || dst == NULL)
    return;

  if (*dst == NULL) {
    *dst = apr_array_make(dstpool, apr_hash_count(src), sizeof(char *));
  }

  apr_hash_do(array_args_from_hash_do_callback_fn_t, *dst, src);
}

bool should_skip(const char *gend, const char *input, apr_pool_t *pool) {
  apr_finfo_t fgend, finput;
  apr_status_t rc;

  rc = apr_stat(&fgend, gend, APR_FINFO_MTIME, pool);
  if (rc != APR_SUCCESS)
    return false;

  rc = apr_stat(&finput, input, APR_FINFO_MTIME, pool);
  if (rc != APR_SUCCESS)
    return false;

  return fgend.mtime > finput.mtime;
}

void qbuild_init() { apr_initialize(); }
void qbuild_quit() { apr_terminate(); }

// void node_throw(node_t *node, int rc) { node_throw(node, rc); }

node_t *subnode_create(node_t *parent, apr_size_t sz) {
  apr_pool_t *pool;
  apr_pool_create(&pool, parent->catch->pool);
  node_t *node = apr_pcalloc(pool, sz);
  node->pool = pool;
  node->qbinfo = parent->qbinfo;
  node->catch = parent->catch;

  // apr_dir_make_recursive("build_cache/", APR_FPROT_OS_DEFAULT, pool);

  return node;
}

int node_include_subdir(node_t *node, const char *path, void *arg) {
  src_step_t *build_c = src_create(node, "build.c");
  src_add_args(build_c, "-g");
  linkable_t *build_obj = src_compile(build_c);

  so_step_t *build_so_step = so_create(node, "build.so");
  so_add_libs(build_so_step, "qbuild");
  so_add_linkables(build_so_step, build_obj);
  so_file_t *build_so = so_link(build_so_step);

  /*
  src_destroy(build_c);
  linkable_destroy(build_obj);
  so_destroy(build_so_step);
  */

  int rc;
  for (int i = 0; i < node->qbinfo->cmdc; ++i) {
    printf("Run cmd %s\n", node->qbinfo->cmdv[i]);
    rc = so_run(build_so, node->qbinfo->cmdv[i], arg);
    if (rc != 0)
      printf("Failed to run command: %s, code %d\n", node->qbinfo->cmdv[i], rc);
  }
  return rc;
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
    // printf("EOF Found!\n");
  } while (rc == APR_SUCCESS);
  return total;
}

void exec_unit_io_update(exec_unit_t *unit) {
  unit->stdout_total +=
      exec_unit_io_update_single(unit->proc.out, unit->stdout_buffer);
  unit->stderr_total +=
      exec_unit_io_update_single(unit->proc.err, unit->stderr_buffer);
}

void exec_unit_maint(int reason, void *ud, int code) {
  exec_unit_t *unit = ud;
  switch (reason) {
  case APR_OC_REASON_RUNNING:
    exec_unit_io_update(unit);
    break;
  case APR_OC_REASON_UNREGISTER:
    // printf("unreg %p\n", unit);
    return;
  case APR_OC_REASON_DEATH:
  case APR_OC_REASON_LOST:
    unit->parent->qbinfo->nexecunits -= 1;
    exec_unit_io_update(unit);
    apr_proc_other_child_unregister(unit);
    if (code == 0) {
      unit->status = EXEC_UNIT_SUCCESS;
    } else {
      unit->status = EXEC_UNIT_FAILURE;
      printf("Failed with code %d\n", code);
      printf("CMD: %s\n", unit->cmdline);
      if (unit->stdout_buffer && unit->stdout_buffer->nelts > 0 &&
          unit->stdout_total > 0) {
        for (int e = 0; e < unit->stdout_buffer->nelts; ++e) {
          stream_chunk_t *chunk =
              &APR_ARRAY_IDX(unit->stdout_buffer, e, stream_chunk_t);
          write(STDOUT_FILENO, chunk->buf, chunk->n);
        }
        printf("\n");
      }
      if (unit->stderr_buffer && unit->stderr_buffer->nelts > 0 &&
          unit->stderr_total > 0) {
        for (int e = 0; e < unit->stderr_buffer->nelts; ++e) {
          stream_chunk_t *chunk =
              &APR_ARRAY_IDX(unit->stderr_buffer, e, stream_chunk_t);
          write(STDOUT_FILENO, chunk->buf, chunk->n);
        }
        printf("\n");
      }
      qbuild_throw(unit->parent, code);
    }
    break;
  }
  if (reason != APR_OC_REASON_RUNNING) {
    // printf("MAINT CALLED!!! %d %d %p\n", reason, code, ud);
  }
}

exec_unit_t *exec_unit_init(node_t *node, apr_array_header_t *args, bool skip) {
  apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
  printf("nexecunits: %d | maxexecunits %d\n", node->qbinfo->nexecunits,
         node->qbinfo->maxexecunits);
  while (node->qbinfo->nexecunits >= node->qbinfo->maxexecunits) {
    apr_sleep(500);
    apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
  }

  apr_pool_t *pool;
  apr_pool_create(&pool, node->pool);
  exec_unit_t *unit = apr_pcalloc(pool, sizeof(*unit));
  unit->pool = pool;
  unit->parent = node;

  unit->cmdline = apr_array_pstrcat(unit->pool, args, ' ');
  const char **null_elt = apr_array_push(args);
  *null_elt = NULL;
  const char *const *argv = (const char *const *)args->elts;

  if (skip) {
    unit->status = EXEC_UNIT_SKIPPED;
    // printf("Skip: %s %p\n", unit->cmdline, unit);
  } else {
    unit->status = EXEC_UNIT_RUNNING;
    // printf("Exec: %s %p\n", unit->cmdline, unit);

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

  return unit;
}

void exec_unit_wait(exec_unit_t *unit) {
  if (unit->status == EXEC_UNIT_SKIPPED)
    return;

  apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
  while (unit->status == EXEC_UNIT_RUNNING) {
    apr_sleep(500);
    apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
  }
}

void exec_unit_destroy(exec_unit_t *unit) {
  apr_pool_destroy(unit->pool); //
}

apr_array_header_t *pcdep_runpc(pcdep_t *dep, apr_array_header_t *args) {
  exec_unit_t *unit = exec_unit_init(&dep->node, args, false);
  exec_unit_wait(unit);

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

  apr_array_header_t *pcargs =
      apr_array_make(dep->node.pool, 8, sizeof(const char *));
  const char *seps = " \n\r\t";
  for (token = apr_strtok(output, seps, &saveptr); token != NULL;
       token = apr_strtok(NULL, seps, &saveptr)) {
    const char **elt = apr_array_push(pcargs);
    *elt = apr_pstrdup(dep->node.pool, token);
  }

  exec_unit_destroy(unit);

  return pcargs;
}

void vadd_args(apr_array_header_t *args, va_list ap) {
  const char *s;

  while ((s = va_arg(ap, const char *)) != NULL) {
    const char **elt = apr_array_push(args);
    *elt = apr_pstrdup(args->pool, s);
  }
}

void add_args(apr_array_header_t *args, ...) {
  va_list ap;
  va_start(ap, args);
  vadd_args(args, ap);
  va_end(ap);
}

pcdep_t *pcdep_create(node_t *node, const char *pcname) {
  apr_pool_t *argpool;
  pcdep_t *dep = (void *)subnode_create(node, sizeof(*dep));

  apr_pool_create(&argpool, dep->node.pool);
  apr_array_header_t *cflags_args = apr_array_make(argpool, 4, sizeof(char *));
  add_args(cflags_args, "pkg-config", pcname, "--cflags", NULL);
  dep->cflags_args = pcdep_runpc(dep, cflags_args);

  apr_array_header_t *libs_args = apr_array_make(argpool, 4, sizeof(char *));
  add_args(libs_args, "pkg-config", pcname, "--libs", NULL);
  dep->libs_args = pcdep_runpc(dep, libs_args);

  apr_pool_destroy(argpool);
  return dep;
}

src_step_t *src_create(node_t *node, const char *path) {
  src_step_t *step = (void *)subnode_create(node, sizeof(*step));

  step->c_path = apr_pstrdup(step->node.pool, path);
  step->compiled_path = apr_psprintf(step->node.pool, "build_cache/%s.o", path);

  step->cflags_args = NULL;
  step->libs_args = NULL;
  step->args = apr_array_make(step->node.pool, 8, sizeof(const char *));
  src_add_args(step, "gcc", "-c", "-o", step->compiled_path, path, "-fPIC",
               NULL);

  return step;
}

void _src_add_args(src_step_t *b, ...) {
  va_list ap;
  va_start(ap, b);
  vadd_args(b->args, ap);
  va_end(ap);
}

void _src_add_deps(src_step_t *step, ...) {
  va_list ap;
  va_start(ap, step);
  pcdep_t *dep;
  while ((dep = va_arg(ap, pcdep_t *)) != NULL) {
    cat_args_array(&step->cflags_args, dep->cflags_args, step->node.pool);
    cat_args_array(&step->libs_args, dep->libs_args, step->node.pool);
  }
  va_end(ap);
}

linkable_t *src_compile(src_step_t *step) {
  apr_procattr_t *procattr;
  apr_procattr_create(&procattr, step->node.pool);
  apr_procattr_cmdtype_set(procattr, APR_PROGRAM_PATH);

  array_args_from_hash(&step->args, step->cflags_args, step->node.pool);

  linkable_t *file = (void *)subnode_create(&step->node, sizeof(*file));

  file->node.type = STEP_OBJFILE;

  file->path = apr_pstrdup(file->node.pool, step->compiled_path);

  // cat_args(&step->libs_args, b->libs_args, step->pool);
  file->libs_args = NULL;
  if (step->libs_args)
    file->libs_args = apr_hash_copy(file->node.pool, step->libs_args);

  file->exec_unit = exec_unit_init(
      &file->node, step->args,
      should_skip(step->compiled_path, step->c_path, step->node.pool));

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

linkable_t *ar_import(node_t *node, const char *path) {
  linkable_t *file = (void *)subnode_create(node, sizeof(*file));
  file->node.type = STEP_ARFILE;

  file->path = apr_pstrdup(file->node.pool, path);
  return file;
}

void exe_add_args(exe_step_t *b, ...) {
  va_list ap;
  va_start(ap, b);
  vadd_args(b->args, ap);
  va_end(ap);
}

exe_step_t *exe_create(node_t *node, const char *exename) {
  exe_step_t *step = (void *)subnode_create(node, sizeof(*step));

  step->exename = apr_pstrdup(step->node.pool, exename);

  step->libs_args = NULL;
  step->linkables = NULL;
  step->args = apr_array_make(step->node.pool, 8, sizeof(const char *));
  exe_add_args(step, "gcc", "-o", step->exename, NULL);

  return step;
}

bool complete_builds(apr_array_header_t *linkables, apr_array_header_t *args,
                     apr_hash_t *libs_args, apr_pool_t *pool,
                     const char *outfile) {
  bool skip = true;
  for (int i = 0; i < linkables->nelts; ++i) {
    linkable_t *file = APR_ARRAY_IDX(linkables, i, linkable_t *);
    add_args(args, file->path, NULL);

    if (file->exec_unit) {
      exec_unit_wait(file->exec_unit);
      if (file->exec_unit->status != EXEC_UNIT_SKIPPED)
        skip = false;
    }

    if (file->libs_args && apr_hash_count(file->libs_args) > 0) {
      cat_args_hash(&libs_args, file->libs_args, pool);
    }
  }

  array_args_from_hash(&args, libs_args, pool);
  return skip;
}

void vadd_linkables(apr_array_header_t **linkables, va_list ap,
                    apr_pool_t *pool) {
  linkable_t *linkable;
  while ((linkable = va_arg(ap, linkable_t *)) != NULL) {
    if (*linkables == NULL)
      *linkables = apr_array_make(pool, 8, sizeof(linkable_t *));

    // printf("adding linkable %d\n", (*linkables)->nelts);
    linkable_t **elt = apr_array_push(*linkables);
    *elt = linkable;
  }
}

void _exe_add_linkables(exe_step_t *step, ...) {
  va_list ap;
  va_start(ap, step);
  vadd_linkables(&step->linkables, ap, step->node.pool);
  va_end(ap);
}

void exe_link(exe_step_t *step) {
  bool skip = complete_builds(step->linkables, step->args, step->libs_args,
                              step->node.pool, step->exename);

  exec_unit_t *exec_unit = exec_unit_init(&step->node, step->args, skip);
  exec_unit_wait(exec_unit);

  printf("Done building exe %s\n", step->exename);
}

/*
void exe_destroy(exe_step_t *step) {
  node_destroy(&step->node); //
}
*/

void _so_add_args(so_step_t *b, ...) {
  va_list ap;
  va_start(ap, b);
  vadd_args(b->args, ap);
  va_end(ap);
}

void _so_add_libs(so_step_t *step, ...) {
  va_list ap;
  va_start(ap, step);
  const char *s;
  while ((s = va_arg(ap, const char *)) != NULL) {
    if (step->libs_args == NULL)
      step->libs_args = apr_hash_make(step->node.pool);
    apr_hash_set(step->libs_args, apr_pstrcat(step->node.pool, "-l", s, NULL),
                 APR_HASH_KEY_STRING, (void *)1);
  }
  va_end(ap);
}

so_step_t *so_create(node_t *node, const char *soname) {
  so_step_t *step = (void *)subnode_create(node, sizeof(*step));

  step->soname = apr_pstrdup(step->node.pool, soname);

  step->libs_args = NULL;
  step->linkables = NULL;
  step->args = apr_array_make(step->node.pool, 8, sizeof(const char *));
  so_add_args(step, "gcc", "-o", step->soname, "-shared", NULL);

  return step;
}

void _so_add_linkables(so_step_t *step, ...) {
  va_list ap;
  va_start(ap, step);
  vadd_linkables(&step->linkables, ap, step->node.pool);
  va_end(ap);
}

so_file_t *so_link(so_step_t *step) {
  bool skip = complete_builds(step->linkables, step->args, step->libs_args,
                              step->node.pool, step->soname);

  exec_unit_t *exec_unit = exec_unit_init(&step->node, step->args, skip);

  exec_unit_wait(exec_unit);

  printf("Done building so %s\n", step->soname);

  so_file_t *outfile = (void *)subnode_create(&step->node, sizeof(*outfile));

  outfile->dso = NULL;
  outfile->soname = apr_pstrdup(outfile->node.pool, step->soname);

  return outfile;
}

/*
void so_destroy(so_step_t *step) {
  node_destroy(&step->node); //
}
*/

typedef void (*build_fn_t)(void *);

void *node_create(node_t **newnode, node_t *parent, int cmdc,
                  const char *cmdv[]) {
  qbinfo_t *qbinfo;
  apr_pool_t *pool;
  if (parent) {
    apr_pool_create(&pool, parent->pool);
    qbinfo = parent->qbinfo;
  } else {
    apr_pool_create_unmanaged(&pool);
    qbinfo = apr_palloc(pool, sizeof(*qbinfo));
    qbinfo->nexecunits = 0;
    qbinfo->maxexecunits = 8;
    qbinfo->cmdc = cmdc;
    qbinfo->cmdv = cmdv;
    apr_dir_make_recursive("build_cache/", APR_FPROT_OS_DEFAULT, pool);
  }
  node_t *node = apr_palloc(pool, sizeof(*node));
  node->pool = pool;
  node->qbinfo = qbinfo;
  node->catch = apr_palloc(pool, sizeof(catch_t));
  node->catch->pool = pool;
  *newnode = node;
  return &node->catch->env;
}

void node_destroy(node_t *node) {
  apr_pool_destroy(node->pool); //
}

int so_run(so_file_t *file, const char *sym, void *arg) {
  apr_status_t s;
  if (file->dso == NULL) {
    s = apr_dso_load(&file->dso,
                     apr_pstrcat(file->node.pool, "./", file->soname, NULL),
                     file->node.pool);
    if (s != APR_SUCCESS) {
      char errbuf[256];
      printf("FAILED TO LOAD DSO: %s %p\n",
             apr_strerror(s, errbuf, sizeof(errbuf)), file->dso);
      apr_dso_error(file->dso, errbuf, sizeof(errbuf));
      fprintf(stderr, "dso_load failed: %s\n", errbuf);
      return -1;
    }
  }

  build_fn_t build_fn;
  int rv = apr_dso_sym((apr_dso_handle_sym_t *)&build_fn, file->dso, sym);
  if (rv != APR_SUCCESS) {
    char errbuf[256];
    apr_dso_error(file->dso, errbuf, sizeof(errbuf));
    fprintf(stderr, "dso_sym failed: %s\n", errbuf);
    return -1;
  }

  node_t *node;
  int rc = setjmp(*(jmp_buf *)node_create(&node, &file->node, 0, NULL));
  if (rc == 0) {
    build_fn(node);
    printf("Ran build\n");
  }
  // node_destroy(node);
  return rc;
}
