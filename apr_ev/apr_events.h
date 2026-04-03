#include <apr_poll.h>
#include <apr_pools.h>

typedef struct apr_loop apr_loop_t;
typedef struct apr_event apr_event_t;
typedef struct apr_prerun apr_prerun_t;

typedef apr_status_t (*apr_prerun_cb_t)(void *ud);
typedef apr_status_t (*apr_event_cb_t)(const apr_pollfd_t *pfd, void *ud);

void apr_event_setup(apr_loop_t **loop, apr_pool_t *pool);

void apr_event_add_file(apr_loop_t *loop, apr_file_t *file, apr_pool_t *pool,
                        int reqevents, apr_event_cb_t fn, void *ud);
void apr_event_add_pollfd(apr_loop_t *loop, apr_pollfd_t *pfd, apr_pool_t *pool,
                          apr_event_cb_t fn, void *ud);
void apr_event_remove_pollfd(apr_loop_t *loop, const apr_pollfd_t *pfd);

apr_prerun_t *apr_event_add_prerun(apr_loop_t *loop, apr_prerun_cb_t fn,
                                   void *ud);
void apr_event_remove_prerun(apr_prerun_t *prerun);

void apr_event_run(apr_loop_t *loop);
