#include <apr.h>
#include <apr_general.h>
#include <apr_poll.h>

typedef apr_status_t (*apr_event_cb_t)(void *descriptor);

struct apr_event {
  apr_event_cb_t fn;
  void *ud;
};
typedef struct apr_event apr_event_t;

static apr_status_t internal_run(void *baton, apr_pollfd_t *pfd) {
  apr_event_t *ev = pfd->client_data;
  if (pfd->desc_type == APR_POLL_FILE)
    return ev->fn(pfd->desc.f);
  else if (pfd->desc_type == APR_POLL_SOCKET)
    return ev->fn(pfd->desc.s);
  else
    fprintf(stderr, "NO EVENT TYPE\n");
  return APR_EINVAL;
}

void apr_event_add_file(apr_pollcb_t *pcb, apr_pool_t *pool) {
  apr_event_t *ev = apr_palloc(pool, sizeof(*ev));
  apr_pollfd_t *pfd = apr_palloc(pool, sizeof(*pfd));
  *pfd = (apr_pollfd_t){
      .desc_type = APR_POLL_SOCKET,
      .desc.s = NULL,
      .client_data = ev,
      .p = pool,
      .reqevents = APR_POLLIN,
  };

  apr_pollcb_add(pcb, pfd);
}

void apr_event_run(apr_pollcb_t *pcb) {
  apr_pollcb_poll(pcb, -1, internal_run, NULL);
}

int main(int argc, char *argv[]) {
  apr_initialize();

  apr_pool_t *pool;
  apr_pool_create_unmanaged(&pool);

  apr_pollcb_t *pcb;
  apr_pollcb_create(&pcb, 16, pool, APR_POLLSET_WAKEABLE);

  apr_event_add_file(pcb, pool);

  apr_event_run(pcb);

  apr_terminate();
}
