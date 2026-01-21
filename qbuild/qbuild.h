#ifndef LIBQ_QBUILD_H
#define LIBQ_QBUILD_H

typedef struct node node_t;
typedef struct pcdep pcdep_t;
typedef struct src_step src_step_t;
typedef struct linkable linkable_t;
typedef struct exe_step exe_step_t;
typedef struct so_step so_step_t;
typedef struct so_file so_file_t;

void qbuild_init();
void qbuild_quit();

// General tree structure
void *node_create(node_t **newnode, node_t *parent, int cmdc,
                  const char *cmdv[]);
void node_destroy(node_t *qbinfo);

int node_include_subdir(node_t *node, const char *path, void *arg);

void qbuild_throw(node_t *node, int rc);

// Package config dependencies
pcdep_t *pcdep_create(node_t *node, const char *pcname);

// C source files
src_step_t *src_create(node_t *node, const char *path);

#define src_add_args(step, ...) _src_add_args(step, __VA_ARGS__, 0)
void _src_add_args(src_step_t *step, ...);

#define src_add_deps(step, ...) _src_add_deps(step, __VA_ARGS__, 0)
void _src_add_deps(src_step_t *step, ...);

linkable_t *src_compile(src_step_t *b);

// Static libraries / archives
linkable_t *ar_import(node_t *node, const char *path);

// Executables
exe_step_t *exe_create(node_t *node, const char *exename);

#define exe_add_linkables(step, ...) _exe_add_linkables(step, __VA_ARGS__, 0)
void _exe_add_linkables(exe_step_t *step, ...);

void exe_link(exe_step_t *step);

// Shared objects
so_step_t *so_create(node_t *node, const char *soname);

#define so_add_args(step, ...) _so_add_args(step, __VA_ARGS__, 0)
void _so_add_args(so_step_t *step, ...);

#define so_add_libs(step, ...) _so_add_libs(step, __VA_ARGS__, 0)
void _so_add_libs(so_step_t *step, ...);

#define so_add_linkables(step, ...) _so_add_linkables(step, __VA_ARGS__, 0)
void _so_add_linkables(so_step_t *step, ...);

so_file_t *so_link(so_step_t *step);

int so_run(so_file_t *file, const char *sym, void *arg);

#endif
