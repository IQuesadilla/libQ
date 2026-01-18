#include "qbuild.h"
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>

#include <apr.h>
#include <apr_getopt.h>

apr_getopt_option_t opts[] = {
    {"help", 'h', false},
    {0, 0, 0},
};

int main(int argc, const char *argv[]) {
  qbuild_init();

  apr_pool_t *pool;
  apr_pool_create_unmanaged(&pool);
  apr_getopt_t *os;
  apr_getopt_init(&os, pool, argc, argv);
  os->interleave = 1;

  struct {
    uint help : 1;
  } flags = {0};

  int optch;
  const char *arg;
  apr_status_t s;
  while ((s = apr_getopt_long(os, opts, &optch, &arg)) == APR_SUCCESS) {
    switch (optch) {
    case 'h':
      flags.help = 1;
      break;
    }
  }
  if (s != APR_EOF) {
    return 1;
  }

  if (flags.help || os->ind == os->argc) {
    printf("Usage: %s\n", argv[0]);
    return 0;
  }

  qbinfo_t *qbinfo;
  int rc = setjmp(*(jmp_buf *)qbinfo_create(&qbinfo, NULL, os->argc - os->ind,
                                            &os->argv[os->ind]));
  if (rc == 0) {
    project_t *qb = project_create(qbinfo);
    project_include_subdir(qb, ".", qbinfo);
    project_destroy(qb);
  } else {
    printf("Failed to build, quitting\n");
  }
  qbinfo_destroy(qbinfo);

  qbuild_quit();
  return rc;
}
