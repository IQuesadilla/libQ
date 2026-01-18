#ifndef LIBQ_QBUILD_H
#define LIBQ_QBUILD_H

typedef struct project project_t;
typedef struct pcdep pcdep_t;
typedef struct src_step src_step_t;
typedef struct obj_file obj_file_t;
typedef struct exe_step exe_step_t;
typedef struct so_step so_step_t;
typedef struct so_file so_file_t;
typedef struct qbinfo qbinfo_t;

void project_throw(project_t *proj, int rc);

void *qbinfo_create(qbinfo_t **newqbinfo, qbinfo_t *parent, int cmdc,
                    const char *cmdv[]);
void qbinfo_destroy(qbinfo_t *qbinfo);

void qbuild_init();
void qbuild_quit();

project_t *project_create(qbinfo_t *qbinfo);
int project_include_subdir(project_t *proj, const char *path, void *arg);
void project_destroy(project_t *proj);

pcdep_t *pcdep_create(project_t *proj, const char *pcname);

src_step_t *src_create(project_t *proj, const char *path);
void src_add_args(src_step_t *b, ...);
void src_add_deps(src_step_t *step, ...);
obj_file_t *src_compile(src_step_t *b);
void src_destroy(src_step_t *step);

void obj_destroy(obj_file_t *step);

exe_step_t *exe_create(project_t *proj, const char *exename);
void exe_link(exe_step_t *step, ...);
void exe_destroy(exe_step_t *step);

so_step_t *so_create(project_t *proj, const char *soname);
void so_add_args(so_step_t *b, ...);
void so_add_libs(so_step_t *step, ...);
so_file_t *so_link(so_step_t *step, ...);
void so_destroy(so_step_t *step);

int so_run(so_file_t *file, const char *sym, void *arg);

#endif
