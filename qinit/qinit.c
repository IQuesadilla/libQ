#include <apr.h>
#include <apr_file_info.h>
#include <apr_fnmatch.h>
#include <apr_pools.h>

#include <qbuild.h>
#include <setjmp.h>

#include <stdio.h>

int main(int argc, char *argv[]) {
  apr_pool_t *pool;
  apr_pool_create_unmanaged(&pool);

  apr_array_header_t *configs;
  apr_match_glob("/etc/qinit/*.c", &configs, pool);

  for (int c = 0; c < configs->nelts; ++c) {
    const char *config_path = APR_ARRAY_IDX(configs, c, const char *);

    node_t node;
    int rc = setjmp(*(jmp_buf *)root_node_create(&node, NULL, 0));
    qbuild_log_stderr(node);
    if (rc == 0) {
      src_step_t config = src_create(node, config_path);
      src_add_args(build_c, "-g");
      linkable_t config_obj = src_compile(build_c);

      so_step_t config_so_step =
          so_create(node, apr_pstrcat(node->pool, config_path, ".so", NULL));
      so_add_libs(config_so_step, "qbuild");
      so_add_linkables(config_so_step, config_obj);
      so_file_t config_so = so_link(config_so_step);

      node_t subnode = subnode_create(node, sizeof(node_t));
      // so_run_build(config_so, subnode);
      qbuild_wait_all(node);
      printf("Cleaning up\n");
    } else {
      printf("Failed to build, quitting\n");
    }
    node_destroy(node);
  }

  return 0;
}
