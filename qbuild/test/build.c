#include <qbuild.h>

void build(qbinfo_t *qbinfo) {
  project_t *testbuild = project_create(qbinfo);

  pcdep_t *apr_util_1_dep = pcdep_create(testbuild, "apr-util-1");
  pcdep_t *apr_1_dep = pcdep_create(testbuild, "apr-1");

  src_step_t *tobuild_step = src_create(testbuild, "tobuild.c");
  src_add_args(tobuild_step, "-g", "-D_REAL_IMPL", 0);
  src_add_deps(tobuild_step, apr_1_dep, apr_util_1_dep, 0);
  obj_file_t *tobuild_obj = src_compile(tobuild_step);
  src_destroy(tobuild_step);

  src_step_t *add_step = src_create(testbuild, "add.c");
  src_add_args(add_step, "-g", "-D_REAL_IMPL", 0);
  obj_file_t *add_obj = src_compile(add_step);
  src_destroy(add_step);

  exe_step_t *exe_step = exe_create(testbuild, "outapp");
  exe_link(exe_step, tobuild_obj, add_obj, 0);
  exe_destroy(exe_step);

  obj_destroy(tobuild_obj);

  so_step_t *so_step = so_create(testbuild, "outadd.so");
  so_link(so_step, add_obj, 0);
  so_destroy(so_step);

  obj_destroy(add_obj);

  project_destroy(testbuild);
  return;
}
