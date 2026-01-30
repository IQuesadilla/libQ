#include <qbuild.h>

void build(node_t *testbuild) {
  pcdep_t *apr_1_dep = pcdep_create(testbuild, "apr-1");
  pcdep_t *ncurses_dep = pcdep_create(testbuild, "ncurses");

  src_step_t *tobuild_step = src_create(testbuild, "qbtui.c");
  src_add_deps(tobuild_step, ncurses_dep, apr_1_dep);
  linkable_t *tobuild_obj = src_compile(tobuild_step);

  exe_step_t *exe_step = exe_create(testbuild, "qbtui");
  exe_add_linkables(exe_step, tobuild_obj);
  exe_add_libs(exe_step, "qbuild");
  exe_link(exe_step);
}
