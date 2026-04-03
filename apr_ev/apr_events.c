#include "apr_events.h"

#include <apr.h>
#include <apr_general.h>
#include <apr_poll.h>

struct apr_event {
  apr_event_cb_t fn;
  void *ud;
  apr_pollfd_t pfd;
};

struct apr_prerun {
  uint8_t valid;
  apr_prerun_cb_t fn;
  void *ud;
};

struct apr_loop {
  apr_array_header_t *preruns;
  apr_pollset_t *pset;
};

static apr_status_t internal_run(void *baton, apr_pollfd_t *pfd) {
  return APR_EINVAL;
}

void apr_event_add_file(apr_loop_t *loop, apr_file_t *file, apr_pool_t *pool,
                        int reqevents, apr_event_cb_t fn, void *ud) {
  apr_pollfd_t pfd = {
      .desc_type = APR_POLL_FILE,
      .desc.f = file,
      .client_data = (void *)0xdefed,
      .p = pool,
      .reqevents = reqevents,
  };

  apr_event_add_pollfd(loop, &pfd, pool, fn, ud);
}

void apr_event_add_pollfd(apr_loop_t *loop, apr_pollfd_t *pfd, apr_pool_t *pool,
                          apr_event_cb_t fn, void *ud) {
  apr_event_t *ev = apr_palloc(pool, sizeof(*ev));
  *ev = (apr_event_t){
      .fn = fn,
      .ud = ud,
      .pfd = *pfd,
  };

  ev->pfd.client_data = ev;

  apr_pollset_add(loop->pset, &ev->pfd);
}

void apr_event_remove_pollfd(apr_loop_t *loop, const apr_pollfd_t *pfd) {
  apr_pollset_remove(loop->pset, pfd);
}

apr_prerun_t *apr_event_add_prerun(apr_loop_t *loop, apr_prerun_cb_t fn,
                                   void *ud) {
  apr_prerun_t *prerun = NULL;
  for (int k = 0; k < loop->preruns->nelts && !prerun; ++k) {
    apr_prerun_t *maybe = &APR_ARRAY_IDX(loop->preruns, k, apr_prerun_t);
    if (!maybe->valid)
      prerun = maybe;
  }
  if (!prerun)
    prerun = &APR_ARRAY_PUSH(loop->preruns, apr_prerun_t);

  prerun->fn = fn;
  prerun->ud = ud;
  prerun->valid = 1;

  return prerun;
}

void apr_event_remove_prerun(apr_prerun_t *prerun) {
  prerun->valid = 0; //
}

void apr_event_run(apr_loop_t *loop) {
  while (1) {
    for (int k = 0; k < loop->preruns->nelts; ++k) {
      apr_prerun_t *prerun = &APR_ARRAY_IDX(loop->preruns, k, apr_prerun_t);
      if (prerun->valid)
        prerun->fn(prerun->ud);
    }

    apr_int32_t num;
    const apr_pollfd_t *descriptors;
    apr_pollset_poll(loop->pset, -1, &num, &descriptors);

    for (int pidx = 0; pidx < num; ++pidx) {
      const apr_pollfd_t *pfd = &descriptors[pidx];
      apr_event_t *ev = pfd->client_data;
      if (pfd->desc_type == APR_POLL_FILE)
        ev->fn(pfd, ev->ud);
      else if (pfd->desc_type == APR_POLL_SOCKET)
        ev->fn(pfd, ev->ud);
      else
        fprintf(stderr, "NO EVENT TYPE\n");
    }
  }
}

void apr_event_setup(apr_loop_t **loop, apr_pool_t *pool) {
  *loop = apr_palloc(pool, sizeof(**loop));
  (*loop)->preruns = apr_array_make(pool, 16, sizeof(apr_prerun_t));
  apr_pollset_create(&(*loop)->pset, 16, pool, APR_POLLSET_WAKEABLE);
}
