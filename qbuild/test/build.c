#include <qbuild.h>

void build(node_t testbuild) {
  // pcdep_t *apr_util_1_dep = pcdep_create(testbuild, "apr-util-1");
  pcdep_t apr_1_dep = pcdep_create(testbuild, "apr-1");

  src_step_t tobuild_step = src_create(testbuild, "tobuild.c");
  src_add_args(tobuild_step, "-g", "-D_REAL_IMPL");
  src_add_deps(tobuild_step, apr_1_dep);
  linkable_t tobuild_obj = src_compile(tobuild_step);

  src_step_t add_step = src_create(testbuild, "add.c");
  src_add_args(add_step, "-g", "-D_REAL_IMPL");
  linkable_t add_obj = src_compile(add_step);

  // ar_file_t *add_sar = ar_import(testbuild, "sub.a");

  exe_step_t exe_step = exe_create(testbuild, "outapp");
  exe_add_linkables(exe_step, tobuild_obj, add_obj);
  exe_link(exe_step);

  so_step_t so_step = so_create(testbuild, "outadd.so");
  so_add_linkables(so_step, add_obj);
  so_link(so_step);
}
