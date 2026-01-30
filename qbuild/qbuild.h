#ifndef LIBQ_QBUILD_H
#define LIBQ_QBUILD_H

typedef struct node *node_t;
typedef struct pcdep *pcdep_t;
typedef struct src_step *src_step_t;
typedef struct linkable *linkable_t;
typedef struct exe_step *exe_step_t;
typedef struct so_step *so_step_t;
typedef struct so_file *so_file_t;

// General tree structure
void node_destroy(node_t qbinfo);

void node_include_subdir(node_t node, const char *path);

void qbuild_throw(node_t node, int rc);

void qbuild_logf(node_t node, const char *fmt, ...);

#define qbuild_assert(test, node)                                              \
  if (!((test)))                                                               \
  _qbuild_assert(node, __FILE__, __LINE__)
void _qbuild_assert(node_t node, const char *file, const int line);

// Package config dependencies
#define pcdep_create(node, ...) _pcdep_create(node, __VA_ARGS__, 0)
pcdep_t _pcdep_create(node_t node, ...);

// C source files
src_step_t src_create(node_t node, const char *path);

#define src_add_args(step, ...) _src_add_args(step, __VA_ARGS__, 0)
void _src_add_args(src_step_t step, ...);

#define src_add_deps(step, ...) _src_add_deps(step, __VA_ARGS__, 0)
void _src_add_deps(src_step_t step, ...);

linkable_t src_compile(src_step_t step);

// Static libraries / archives
linkable_t ar_import(node_t node, const char *path);

// Executables
exe_step_t exe_create(node_t node, const char *exename);

#define exe_add_args(step, ...) _exe_add_args(step, __VA_ARGS__, 0);
void _exe_add_args(exe_step_t step, ...);

#define exe_add_libs(step, ...) _exe_add_libs(step, __VA_ARGS__, 0);
void _exe_add_libs(exe_step_t step, ...);

#define exe_add_linkables(step, ...) _exe_add_linkables(step, __VA_ARGS__, 0)
void _exe_add_linkables(exe_step_t step, ...);

void exe_link(exe_step_t step);

// Shared objects
so_step_t so_create(node_t node, const char *soname);

so_file_t so_import(node_t node, const char *path);

#define so_add_args(step, ...) _so_add_args(step, __VA_ARGS__, 0)
void _so_add_args(so_step_t step, ...);

#define so_add_libs(step, ...) _so_add_libs(step, __VA_ARGS__, 0)
void _so_add_libs(so_step_t step, ...);

#define so_add_linkables(step, ...) _so_add_linkables(step, __VA_ARGS__, 0)
void _so_add_linkables(so_step_t step, ...);

so_file_t so_link(so_step_t step);

void so_run_build(so_file_t file, void *arg);

#endif
