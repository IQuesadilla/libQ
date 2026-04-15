#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <apr_events.h>

typedef void (*downloader_fn_t)(data_node_t *list, void *ud);

int start_download(const char *raw_url, apr_pool_t *pool, apr_loop_t *loop,
                   downloader_fn_t on_complete, void *ud);

#endif
