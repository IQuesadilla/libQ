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

struct qbinfo {
  apr_pool_t *pool;
  int cmdc;
  const char **cmdv;
  jmp_buf env;
};

struct project {
  apr_pool_t *pool;
  int nexecunits, maxexecunits;
  qbinfo_t *qbinfo;
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
  project_t *proj;
  apr_array_header_t *stdout_buffer, *stderr_buffer;
  apr_size_t stdout_total, stderr_total;
  enum {
    EXEC_UNIT_RUNNING,
    EXEC_UNIT_SUCCESS,
    EXEC_UNIT_FAILURE,
    EXEC_UNIT_CRASH,
    EXEC_UNIT_SKIPPED,
  } status;
};
typedef struct exec_unit exec_unit_t;

struct pcdep {
  project_t *proj;
  apr_pool_t *pool;
  apr_array_header_t *cflags_args, *libs_args;
};

struct src_step {
  project_t *proj;
  apr_pool_t *pool;
  apr_array_header_t *args;
  apr_hash_t *cflags_args, *libs_args;
  char *c_path, *compiled_path;
};

struct obj_file {
  project_t *proj;
  apr_pool_t *pool;
  apr_hash_t *libs_args;
  exec_unit_t *exec_unit;
  char *path;
};

struct exe_step {
  project_t *proj;
  apr_pool_t *pool;
  apr_array_header_t *args;
  apr_hash_t *libs_args;
  char *exename;
};

struct so_step {
  project_t *proj;
  apr_pool_t *pool;
  apr_array_header_t *args;
  apr_hash_t *libs_args;
  char *soname;
};

struct so_file {
  project_t *proj;
  apr_pool_t *pool;
  apr_dso_handle_t *dso;
  char *soname;
};

void qbinfo_throw(qbinfo_t *qbinfo, int rc) {
  longjmp(qbinfo->env, rc); //
}

void print_args(const char *pre, const char *const *argv) {
  fputs(pre, stdout);
  for (int i = 0; argv[i]; i++) {
    if (i > 0)
      putchar(' ');

    fputs(argv[i], stdout);
  }

  putchar('\n');
}

void cat_args_array(apr_hash_t **dst, apr_array_header_t *src,
                    apr_pool_t *dstpool) {
  if (src == NULL || dst == NULL)
    return;

  if (src->nelts <= 0)
    return;

  const char **src_elts = (const char **)src->elts;

  if (*dst == NULL) {
    printf("making hash table\n");
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

bool is_file_newer(const char *newer, const char *older, apr_pool_t *pool) {
  apr_finfo_t fnewer, folder;
  apr_status_t rc;

  rc = apr_stat(&fnewer, newer, APR_FINFO_MTIME, pool);
  if (rc != APR_SUCCESS)
    return true;

  rc = apr_stat(&folder, older, APR_FINFO_MTIME, pool);
  if (rc != APR_SUCCESS)
    return true;

  return fnewer.mtime > folder.mtime;
}

void qbuild_init() { apr_initialize(); }
void qbuild_quit() { apr_terminate(); }

void project_throw(project_t *proj, int rc) { qbinfo_throw(proj->qbinfo, rc); }

project_t *project_create(qbinfo_t *qbinfo) {
  apr_pool_t *pool;
  apr_pool_create(&pool, NULL);
  project_t *proj = apr_palloc(pool, sizeof(*proj));
  proj->pool = pool;
  proj->nexecunits = 0;
  proj->maxexecunits = 8;
  proj->qbinfo = qbinfo;

  apr_dir_make_recursive("build_cache/", APR_FPROT_OS_DEFAULT, pool);

  return proj;
}

int project_include_subdir(project_t *proj, const char *path, void *arg) {
  src_step_t *build_c = src_create(proj, "build.c");
  src_add_args(build_c, "-g", NULL);
  obj_file_t *build_obj = src_compile(build_c);
  src_destroy(build_c);

  so_step_t *build_so_step = so_create(proj, "build.so");
  so_add_libs(build_so_step, "qbuild", NULL);
  so_file_t *build_so = so_link(build_so_step, build_obj, NULL);
  obj_destroy(build_obj);
  so_destroy(build_so_step);

  int rc;
  for (int i = 0; i < proj->qbinfo->cmdc; ++i) {
    printf("Run cmd %s\n", proj->qbinfo->cmdv[i]);
    rc = so_run(build_so, proj->qbinfo->cmdv[i], arg);
    if (rc != 0)
      printf("Failed to run command: %s, code %d\n", proj->qbinfo->cmdv[i], rc);
  }
  return rc;
}

void project_destroy(project_t *project) {
  apr_pool_destroy(project->pool); //
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
    // printf("unreg\n");
    return;
  case APR_OC_REASON_DEATH:
  case APR_OC_REASON_LOST:
    unit->proj->nexecunits -= 1;
    exec_unit_io_update(unit);
    apr_proc_other_child_unregister(unit);
    if (code == 0) {
      unit->status = EXEC_UNIT_SUCCESS;
    } else {
      unit->status = EXEC_UNIT_FAILURE;
      printf("failed with code %d\n", code);
      project_throw(unit->proj, 1);
    }
    break;
  }
  if (reason != APR_OC_REASON_RUNNING) {
    // printf("MAINT CALLED!!! %d %d %p\n", reason, code, ud);
  }
}

exec_unit_t *exec_unit_init(project_t *project, apr_array_header_t *args,
                            bool skip) {
  apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
  while (project->nexecunits >= project->maxexecunits) {
    apr_sleep(500);
    apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
  }

  apr_pool_t *pool;
  apr_pool_create(&pool, project->pool);
  exec_unit_t *unit = apr_pcalloc(pool, sizeof(*unit));
  unit->pool = pool;

  unit->proj = project;

  const char **null_elt = apr_array_push(args);
  *null_elt = NULL;
  const char *const *argv = (const char *const *)args->elts;

  if (skip) {
    print_args("Skip: ", argv);
    unit->status = EXEC_UNIT_SKIPPED;
  } else {
    unit->status = EXEC_UNIT_RUNNING;

    print_args("Exec: ", argv);

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
    project->nexecunits += 1;
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
  exec_unit_t *unit = exec_unit_init(dep->proj, args, false);
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
      apr_array_make(dep->pool, 8, sizeof(const char *));
  const char *seps = " \n\r\t";
  for (token = apr_strtok(output, seps, &saveptr); token != NULL;
       token = apr_strtok(NULL, seps, &saveptr)) {
    const char **elt = apr_array_push(pcargs);
    *elt = apr_pstrdup(dep->pool, token);
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

pcdep_t *pcdep_create(project_t *proj, const char *pcname) {
  apr_pool_t *pool, *argpool;
  apr_pool_create(&pool, proj->pool);
  pcdep_t *dep = apr_palloc(pool, sizeof(*dep));
  dep->pool = pool;
  dep->proj = proj;

  apr_pool_create(&argpool, pool);
  apr_array_header_t *cflags_args = apr_array_make(argpool, 4, sizeof(char *));
  add_args(cflags_args, "pkg-config", pcname, "--cflags", NULL);
  dep->cflags_args = pcdep_runpc(dep, cflags_args);

  apr_array_header_t *libs_args = apr_array_make(argpool, 4, sizeof(char *));
  add_args(libs_args, "pkg-config", pcname, "--libs", NULL);
  dep->libs_args = pcdep_runpc(dep, libs_args);

  apr_pool_destroy(argpool);
  return dep;
}

src_step_t *src_create(project_t *proj, const char *path) {
  apr_pool_t *pool;
  apr_pool_create(&pool, proj->pool);
  src_step_t *step = apr_palloc(pool, sizeof(*step));
  step->pool = pool;
  step->proj = proj;

  step->c_path = apr_pstrdup(step->pool, path);
  step->compiled_path = apr_psprintf(step->pool, "build_cache/%s.o", path);

  step->cflags_args = NULL;
  step->libs_args = NULL;
  step->args = apr_array_make(step->pool, 8, sizeof(const char *));
  src_add_args(step, "gcc", "-c", "-o", step->compiled_path, path, "-fPIC",
               NULL);

  return step;
}

void src_add_args(src_step_t *b, ...) {
  va_list ap;
  va_start(ap, b);
  vadd_args(b->args, ap);
  va_end(ap);
}

void src_add_deps(src_step_t *step, ...) {
  va_list ap;
  va_start(ap, step);
  pcdep_t *dep;
  while ((dep = va_arg(ap, pcdep_t *)) != NULL) {
    cat_args_array(&step->cflags_args, dep->cflags_args, step->pool);
    cat_args_array(&step->libs_args, dep->libs_args, step->pool);
  }
  va_end(ap);
}

obj_file_t *src_compile(src_step_t *step) {
  apr_procattr_t *procattr;
  apr_procattr_create(&procattr, step->pool);
  apr_procattr_cmdtype_set(procattr, APR_PROGRAM_PATH);

  array_args_from_hash(&step->args, step->cflags_args, step->pool);

  apr_pool_t *pool;
  apr_pool_create(&pool, NULL);
  obj_file_t *file = apr_palloc(pool, sizeof(*file));
  file->pool = pool;
  file->proj = step->proj;

  file->path = apr_pstrdup(file->pool, step->compiled_path);

  // cat_args(&step->libs_args, b->libs_args, step->pool);
  file->libs_args = NULL;
  if (step->libs_args)
    file->libs_args = apr_hash_copy(file->pool, step->libs_args);

  file->exec_unit = exec_unit_init(
      file->proj, step->args,
      is_file_newer(step->compiled_path, step->c_path, step->pool));

  return file;
}

void src_destroy(src_step_t *step) {
  apr_pool_destroy(step->pool); //
}

void obj_destroy(obj_file_t *file) {
  apr_pool_destroy(file->pool); //
}

void exe_add_args(exe_step_t *b, ...) {
  va_list ap;
  va_start(ap, b);
  vadd_args(b->args, ap);
  va_end(ap);
}

exe_step_t *exe_create(project_t *proj, const char *exename) {
  apr_pool_t *pool;
  apr_pool_create(&pool, proj->pool);
  exe_step_t *step = apr_palloc(pool, sizeof(*step));
  step->pool = pool;
  step->proj = proj;

  step->exename = apr_pstrdup(step->pool, exename);

  step->libs_args = NULL;
  step->args = apr_array_make(step->pool, 8, sizeof(const char *));
  exe_add_args(step, "gcc", "-o", step->exename, NULL);

  return step;
}

bool complete_builds(va_list ap, apr_array_header_t *args,
                     apr_hash_t *libs_args, apr_pool_t *pool) {
  obj_file_t *file;
  bool skip = true;
  while ((file = va_arg(ap, obj_file_t *)) != NULL) {
    const char **elt = apr_array_push(args);
    *elt = apr_pstrdup(pool, file->path);

    exec_unit_wait(file->exec_unit);
    if (file->exec_unit->status != EXEC_UNIT_SKIPPED)
      skip = false;

    if (file->libs_args && apr_hash_count(file->libs_args) > 0) {
      cat_args_hash(&libs_args, file->libs_args, pool);
    }
  }

  array_args_from_hash(&args, libs_args, pool);
  return skip;
}

void exe_link(exe_step_t *step, ...) {
  va_list ap;
  va_start(ap, step);
  bool skip = complete_builds(ap, step->args, step->libs_args, step->pool);
  va_end(ap);

  exec_unit_t *exec_unit = exec_unit_init(step->proj, step->args, skip);
  exec_unit_wait(exec_unit);

  printf("Done building exe %s\n", step->exename);
}

void exe_destroy(exe_step_t *step) {
  apr_pool_destroy(step->pool); //
}

void so_add_args(so_step_t *b, ...) {
  va_list ap;
  va_start(ap, b);
  vadd_args(b->args, ap);
  va_end(ap);
}

void so_add_libs(so_step_t *step, ...) {
  va_list ap;
  va_start(ap, step);
  const char *s;
  while ((s = va_arg(ap, const char *)) != NULL) {
    if (step->libs_args == NULL)
      step->libs_args = apr_hash_make(step->pool);
    apr_hash_set(step->libs_args, apr_pstrcat(step->pool, "-l", s, NULL),
                 APR_HASH_KEY_STRING, (void *)1);
  }
  va_end(ap);
}

so_step_t *so_create(project_t *proj, const char *soname) {
  apr_pool_t *pool;
  apr_pool_create(&pool, proj->pool);
  so_step_t *step = apr_palloc(pool, sizeof(*step));
  step->pool = pool;
  step->proj = proj;

  step->soname = apr_pstrdup(step->pool, soname);

  step->libs_args = NULL;
  step->args = apr_array_make(step->pool, 8, sizeof(const char *));
  so_add_args(step, "gcc", "-o", step->soname, "-shared", NULL);

  return step;
}

so_file_t *so_link(so_step_t *step, ...) {
  va_list ap;
  va_start(ap, step);
  bool skip = complete_builds(ap, step->args, step->libs_args, step->pool);
  va_end(ap);

  exec_unit_t *exec_unit = exec_unit_init(step->proj, step->args, skip);

  exec_unit_wait(exec_unit);

  printf("Done building so %s\n", step->soname);

  apr_pool_t *pool;
  apr_pool_create(&pool, step->proj->pool);
  so_file_t *outfile = apr_palloc(pool, sizeof(*step));
  outfile->pool = pool;
  outfile->proj = step->proj;
  outfile->dso = NULL;
  outfile->soname = apr_pstrdup(pool, step->soname);

  return outfile;
}

void so_destroy(so_step_t *step) {
  apr_pool_destroy(step->pool); //
}

typedef void (*build_fn_t)(void *);

void *qbinfo_create(qbinfo_t **newqbinfo, qbinfo_t *parent, int cmdc,
                    const char *cmdv[]) {
  apr_pool_t *pool;
  if (parent)
    apr_pool_create(&pool, parent->pool);
  else
    apr_pool_create_unmanaged(&pool);
  qbinfo_t *qbinfo = apr_palloc(pool, sizeof(*qbinfo));
  qbinfo->pool = pool;
  *newqbinfo = qbinfo;
  qbinfo->cmdv = cmdv;
  qbinfo->cmdc = cmdc;
  return &qbinfo->env;
}

void qbinfo_destroy(qbinfo_t *qbinfo) {
  apr_pool_destroy(qbinfo->pool); //
}

int so_run(so_file_t *file, const char *sym, void *arg) {
  apr_status_t s;
  if (file->dso == NULL) {
    s = apr_dso_load(&file->dso,
                     apr_pstrcat(file->pool, "./", file->soname, NULL),
                     file->pool);
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

  qbinfo_t *qbinfo, *parent = file->proj->qbinfo;
  int rc = setjmp(
      *(jmp_buf *)qbinfo_create(&qbinfo, parent, parent->cmdc, parent->cmdv));
  if (rc == 0) {
    build_fn(qbinfo);
    printf("Ran build\n");
  }
  qbinfo_destroy(qbinfo);
  return rc;
}
