#include "qbuild.h"
#include "qb.h"
#include "qexec.h"
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

void qbuild_set_disp_cmds(node_t root_node, qb_disp_cmds_t *cmds) {
  root_node->qbinfo->disp = *cmds;
}

void qbuild_log_file(node_t node, const char *path) {
  apr_file_open(&node->qbinfo->logfile, path, APR_FOPEN_WRITE,
                APR_FPROT_OS_DEFAULT, node->qbinfo->logpool);
}

void qbuild_log_fn(node_t node, qbuild_log_fn_t fn, void *ud) {
  node->qbinfo->disp.qb_log_fn = fn;
  node->qbinfo->ud = ud;
}

void qbuild_vlogf(qbinfo_t *qbinfo, const char *fmt, va_list ap) {
  const char *out = apr_pvsprintf(qbinfo->logpool, fmt, ap);
  if (qbinfo->logstderr)
    apr_file_puts(out, qbinfo->logstderr);
  if (qbinfo->logfile)
    apr_file_puts(out, qbinfo->logfile);
  if (qbinfo->disp.qb_log_fn)
    qbinfo->disp.qb_log_fn(out, qbinfo->ud);
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
  if (node->qbinfo->cmd == QB_BUILD) {
    src_step_t build_c = src_create(node, "build.c");
    src_add_args(build_c, "-g");
    linkable_t build_obj = src_compile(build_c);

    so_step_t build_so_step =
        so_create(node, apr_pstrcat(node->pool, path, "/", "build.so", NULL));
    so_add_libs(build_so_step, "qbuild");
    so_add_linkables(build_so_step, build_obj);
    so_file_t build_so = so_link(build_so_step);

    node_t subnode = subnode_create(node, sizeof(node_t));
    so_run_build(build_so, subnode);
  } else if (node->qbinfo->cmd == QB_TEST) {
    const char *dirpath =
        apr_pstrcat(node->pool, path, "/build_cache/tests/", NULL);

    apr_dir_t *dir;
    apr_dir_open(&dir, dirpath, node->pool);

    apr_status_t s;
    apr_finfo_t finfo;
    while ((s = apr_dir_read(&finfo, APR_FINFO_NAME, dir)) == APR_SUCCESS) {
      if (finfo.name[0] != '.') {
        fprintf(stderr, "Test %s\n", finfo.name);
        so_file_t file =
            so_import(node, apr_pstrcat(node->pool, dirpath, finfo.name, NULL));
        so_run_tests(file, file);
      }
    }
  } else {
    qbuild_logf(node, "Unknown command %d\n", node->qbinfo->cmd);
  }
  // node_destroy(subnode);
  // }
}

void pcdep_process(node_t node, void *vbaton, int len, char *str) {
  pcdep_t dep = (pcdep_t)node;
  pcdep_baton_t *baton = vbaton;
  exec_unit_t *unit = baton->exec_unit;

  char *saveptr = NULL;
  char *token;

  // arg_list_init(&baton->args_out, dep->node.pool);
  const char *seps = " \n\r\t";
  for (token = apr_strtok(str, seps, &saveptr); token != NULL;
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

void src_process(node_t node, void *vbaton, int len, char *str) {
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

  // file->node.type = STEP_OBJFILE;

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
  // file->node.type = STEP_ARFILE;

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

void exe_process(node_t node, void *vbaton, int len, char *str) {
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

  const char *s;
  while ((s = va_arg(ap, const char *)) != NULL) {
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

void so_process(node_t node, void *vbaton, int len, char *str) {
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

so_file_t so_import(node_t node, const char *path) {
  so_file_t file = (void *)subnode_create(node, sizeof(*file));
  // file->node.type = STEP_SOFILE;

  file->soname = apr_pstrdup(file->node.pool, path);
  file->mtime = get_mtime(file->soname, file->node.pool);

  return file;
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
    return NULL;
  }
  return ret;
}

void so_load_throw(so_file_t file) {
  char errbuf[256];
  apr_dso_error(file->dso, errbuf, sizeof(errbuf));
  qbuild_logf(&file->node, "dso_sym failed: %s\n", errbuf);
  qbuild_throw(&file->node, -1);
}

typedef void (*build_fn_t)(void *);

void so_run_build(so_file_t file, void *arg) {
  exec_unit_wait(file->exec_unit); // TODO: Replace with a "on_complete"
  so_load_file(file);

  qbuild_logf(&file->node, "Building %s\n", file->soname);
  build_fn_t build_fn = so_load_sym(file, "build");
  if (build_fn == NULL)
    so_load_throw(file);

  build_fn(arg);
}

void so_run_tests(so_file_t file, void *arg) {
  exec_unit_wait(file->exec_unit); // TODO: Replace with a "on_complete"
  so_load_file(file);

  test_func_t *start_tests = so_load_sym(file, "__start_test_array"),
              *stop_tests = so_load_sym(file, "__stop_test_array");

  if (start_tests == NULL || stop_tests == NULL)
    return;

  qbuild_logf(&file->node, "Main: Triggering test suite in %s...\n",
              file->soname);
  for (test_func_t *it = start_tests; it != stop_tests; it = &it[1])
    if (it && *it)
      (*it)();
  qbuild_logf(&file->node, "Main: Suite finished.\n");
}
