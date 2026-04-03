#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <apr_events.h>

#define CHUNK_SIZE 8192

typedef struct data_node {
  char data[CHUNK_SIZE];
  apr_size_t used;
  struct data_node *next;
} data_node_t;

typedef void (*downloader_fn_t)(data_node_t *list, void *ud);

int start_download(const char *raw_url, apr_pool_t *pool, apr_loop_t *loop,
                   downloader_fn_t on_complete, void *ud);

#endif
