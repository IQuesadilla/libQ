#include "apr_events.h"

apr_status_t file_event(apr_file_t *file, void *ud) {
  apr_file_t *err = ud;

  apr_size_t nbytes = 1024;
  char buffer[nbytes];
  apr_file_read(file, buffer, &nbytes);
  apr_file_puts("Got: <", err);
  apr_file_write_full(err, buffer, nbytes, NULL);
  apr_file_puts(">\n", err);
  return APR_SUCCESS;
}

int main(int argc, char *argv[]) {
  apr_initialize();

  apr_pool_t *pool;
  apr_pool_create_unmanaged(&pool);

  apr_file_t *err;
  apr_file_open_stderr(&err, pool);

  apr_file_t *in, *out;
  apr_file_pipe_create(&in, &out, pool);

  apr_loop_t *loop;
  apr_event_setup(&loop, pool);

  apr_event_add_file(loop, in, pool, APR_POLLIN, file_event, err);

  apr_file_printf(out, "Test");
  apr_event_run(loop);

  apr_terminate();
}
