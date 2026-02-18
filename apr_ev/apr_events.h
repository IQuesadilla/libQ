#include <apr_poll.h>
#include <apr_pools.h>

typedef struct apr_loop apr_loop_t;

typedef apr_status_t (*apr_event_file_cb_t)(apr_file_t *file, void *ud);

void apr_event_add_file(apr_loop_t *loop, apr_pool_t *pool, int reqevents,
                        apr_event_file_cb_t fn, void *ud);
void apr_event_run(apr_loop_t *loop);
