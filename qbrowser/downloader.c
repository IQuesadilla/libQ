#include "downloader.h"

#include <serf.h>
/* ====================================================================
 *    Licensed to the Apache Software Foundation (ASF) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The ASF licenses this file
 *    to you under the Apache License, Version 2.0 (the
 *    "License"); you may not use this file except in compliance
 *    with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing,
 *    software distributed under the License is distributed on an
 *    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *    KIND, either express or implied.  See the License for the
 *    specific language governing permissions and limitations
 *    under the License.
 * ====================================================================
 */

#include <stdlib.h>

#include <apr.h>
#include <apr_atomic.h>
#include <apr_base64.h>
#include <apr_getopt.h>
#include <apr_strings.h>
#include <apr_uri.h>
#include <apr_version.h>

#include "serf.h"

/* Add Connection: close header to each request. */
/* #define CONNECTION_CLOSE_HDR */

typedef struct {
  data_node_t *head;
  data_node_t *tail;
} buffer_list_t;

typedef struct {
  const char *hostinfo;
  int using_ssl;
  int head_request;
  serf_ssl_context_t *ssl_ctx;
  serf_bucket_alloc_t *bkt_alloc;

  serf_context_t *ctx;

  apr_loop_t *loop;
  apr_prerun_t *prerun;
  apr_pool_t *pool;
  void *serf_baton;

  buffer_list_t list;
  downloader_fn_t on_complete;
  void *on_complete_ud;

#if APR_MAJOR_VERSION > 0
  apr_uint32_t completed_requests;
#else
  apr_atomic_t completed_requests;
#endif
  int print_headers;
  apr_file_t *output_file;

  serf_connection_t *connection;

  serf_response_acceptor_t acceptor;
  serf_response_handler_t handler;

  const char *host;
  const char *method;
  const char *path;
  const char *req_body_path;
  const char *username;
  const char *password;
  int auth_attempts;
  serf_bucket_t *req_hdrs;
} app_baton_t;

void gather_iovecs(app_baton_t *app_ctx, const struct iovec *vecs,
                   int vecs_len) {
  /* Initialize the list if it's completely empty */
  if (!app_ctx->list.head) {
    app_ctx->list.head = apr_pcalloc(app_ctx->pool, sizeof(data_node_t));
    app_ctx->list.tail = app_ctx->list.head;
  }

  for (int i = 0; i < vecs_len; i++) {
    char *src = (char *)vecs[i].iov_base;
    apr_size_t src_len = vecs[i].iov_len;

    /* Inner loop: Keep copying until this specific iovec is fully drained */
    while (src_len > 0) {
      data_node_t *current = app_ctx->list.tail;
      apr_size_t space_left = CHUNK_SIZE - current->used;

      /* If the current tail node is full, allocate and append a new one */
      if (space_left == 0) {
        current->next = apr_pcalloc(app_ctx->pool, sizeof(data_node_t));
        app_ctx->list.tail = current->next;
        current = app_ctx->list.tail;
        space_left = CHUNK_SIZE;
      }

      /* Determine how much we can safely copy in this iteration */
      apr_size_t to_copy = (src_len < space_left) ? src_len : space_left;

      /* Copy the data */
      memcpy(current->data + current->used, src, to_copy);

      /* Advance all our tracking pointers and counters */
      current->used += to_copy;
      src += to_copy;
      src_len -= to_copy;
    }
  }
}

static void closed_connection(serf_connection_t *conn, void *closed_baton,
                              apr_status_t why, apr_pool_t *pool) {
  app_baton_t *ctx = closed_baton;

  ctx->ssl_ctx = NULL;

  if (why) {
    abort();
  }
}

static void print_ssl_cert_errors(int failures) {
  if (failures) {
    fprintf(stderr, "INVALID CERTIFICATE:\n");
    if (failures & SERF_SSL_CERT_NOTYETVALID)
      fprintf(stderr, "* The certificate is not yet valid.\n");
    if (failures & SERF_SSL_CERT_EXPIRED)
      fprintf(stderr, "* The certificate expired.\n");
    if (failures & SERF_SSL_CERT_SELF_SIGNED)
      fprintf(stderr, "* The certificate is self-signed.\n");
    if (failures & SERF_SSL_CERT_UNKNOWNCA)
      fprintf(stderr, "* The CA is unknown.\n");
    if (failures & SERF_SSL_CERT_UNKNOWN_FAILURE)
      fprintf(stderr, "* Unknown failure.\n");
  }
}

static apr_status_t ignore_all_cert_errors(void *data, int failures,
                                           const serf_ssl_certificate_t *cert) {
  print_ssl_cert_errors(failures);

  /* In a real application, you would normally would not want to do this */
  return APR_SUCCESS;
}

static char *convert_organisation_to_str(apr_hash_t *org, apr_pool_t *pool) {
  return apr_psprintf(pool, "%s, %s, %s, %s, %s (%s)",
                      (char *)apr_hash_get(org, "OU", APR_HASH_KEY_STRING),
                      (char *)apr_hash_get(org, "O", APR_HASH_KEY_STRING),
                      (char *)apr_hash_get(org, "L", APR_HASH_KEY_STRING),
                      (char *)apr_hash_get(org, "ST", APR_HASH_KEY_STRING),
                      (char *)apr_hash_get(org, "C", APR_HASH_KEY_STRING),
                      (char *)apr_hash_get(org, "E", APR_HASH_KEY_STRING));
}

static apr_status_t print_certs(void *data, int failures, int error_depth,
                                const serf_ssl_certificate_t *const *certs,
                                apr_size_t certs_len) {
  apr_pool_t *pool;
  const serf_ssl_certificate_t *current;

  apr_pool_create(&pool, NULL);

  fprintf(stderr, "Received certificate chain with length %d\n",
          (int)certs_len);
  print_ssl_cert_errors(failures);
  if (failures)
    fprintf(stderr, "Error at depth=%d\n", error_depth);
  else
    fprintf(stderr, "Chain provided with depth=%d\n", error_depth);

  while ((current = *certs) != NULL) {
    apr_hash_t *issuer, *subject, *serf_cert;
    apr_array_header_t *san;

    subject = serf_ssl_cert_subject(current, pool);
    issuer = serf_ssl_cert_issuer(current, pool);
    serf_cert = serf_ssl_cert_certificate(current, pool);

    fprintf(stderr, "\n-----BEGIN CERTIFICATE-----\n");
    fprintf(stderr, "Hostname: %s\n",
            (const char *)apr_hash_get(subject, "CN", APR_HASH_KEY_STRING));
    fprintf(stderr, "Sha1: %s\n",
            (const char *)apr_hash_get(serf_cert, "sha1", APR_HASH_KEY_STRING));
    fprintf(stderr, "Valid from: %s\n",
            (const char *)apr_hash_get(serf_cert, "notBefore",
                                       APR_HASH_KEY_STRING));
    fprintf(
        stderr, "Valid until: %s\n",
        (const char *)apr_hash_get(serf_cert, "notAfter", APR_HASH_KEY_STRING));
    fprintf(stderr, "Issuer: %s\n", convert_organisation_to_str(issuer, pool));

    san = apr_hash_get(serf_cert, "subjectAltName", APR_HASH_KEY_STRING);
    if (san) {
      int i;
      for (i = 0; i < san->nelts; i++) {
        char *s = APR_ARRAY_IDX(san, i, char *);
        fprintf(stderr, "SubjectAltName: %s\n", s);
      }
    }

    fprintf(stderr, "%s\n", serf_ssl_cert_export(current, pool));
    fprintf(stderr, "-----END CERTIFICATE-----\n");
    ++certs;
  }

  apr_pool_destroy(pool);
  return APR_SUCCESS;
}

static apr_status_t conn_setup(apr_socket_t *skt, serf_bucket_t **input_bkt,
                               serf_bucket_t **output_bkt, void *setup_baton,
                               apr_pool_t *pool) {
  serf_bucket_t *c;
  app_baton_t *ctx = setup_baton;

  c = serf_bucket_socket_create(skt, ctx->bkt_alloc);
  if (ctx->using_ssl) {
    c = serf_bucket_ssl_decrypt_create(c, ctx->ssl_ctx, ctx->bkt_alloc);
    if (!ctx->ssl_ctx) {
      ctx->ssl_ctx = serf_bucket_ssl_decrypt_context_get(c);
    }
    serf_ssl_server_cert_chain_callback_set(
        ctx->ssl_ctx, ignore_all_cert_errors, print_certs, NULL);
    serf_ssl_set_hostname(ctx->ssl_ctx, ctx->hostinfo);

    *output_bkt = serf_bucket_ssl_encrypt_create(*output_bkt, ctx->ssl_ctx,
                                                 ctx->bkt_alloc);
  }

  *input_bkt = c;

  return APR_SUCCESS;
}

static serf_bucket_t *accept_response(serf_request_t *request,
                                      serf_bucket_t *stream,
                                      void *acceptor_baton, apr_pool_t *pool) {
  serf_bucket_t *c;
  serf_bucket_t *response;
  serf_bucket_alloc_t *bkt_alloc;
  app_baton_t *app_ctx = acceptor_baton;

  /* get the per-request bucket allocator */
  bkt_alloc = serf_request_get_alloc(request);

  /* Create a barrier so the response doesn't eat us! */
  c = serf_bucket_barrier_create(stream, bkt_alloc);

  response = serf_bucket_response_create(c, bkt_alloc);

  if (app_ctx->head_request)
    serf_bucket_response_set_head(response);

  return response;
}

/* Kludges for APR 0.9 support. */
#if APR_MAJOR_VERSION == 0
#define apr_atomic_inc32 apr_atomic_inc
#define apr_atomic_dec32 apr_atomic_dec
#define apr_atomic_read32 apr_atomic_read
#endif

static int append_request_headers(void *baton, const char *key,
                                  const char *value) {
  serf_bucket_t *hdrs_bkt = baton;
  serf_bucket_headers_setc(hdrs_bkt, key, value);
  return 0;
}

static apr_status_t setup_request(serf_request_t *request, void *setup_baton,
                                  serf_bucket_t **req_bkt,
                                  serf_response_acceptor_t *acceptor,
                                  void **acceptor_baton,
                                  serf_response_handler_t *handler,
                                  void **handler_baton, apr_pool_t *pool) {
  app_baton_t *ctx = setup_baton;
  serf_bucket_t *hdrs_bkt;
  serf_bucket_t *body_bkt;

  if (ctx->req_body_path) {
    apr_file_t *file;
    apr_status_t status;

    status = apr_file_open(&file, ctx->req_body_path, APR_READ, APR_OS_DEFAULT,
                           pool);

    if (status) {
      printf("Error opening file (%s)\n", ctx->req_body_path);
      return status;
    }

    body_bkt = serf_bucket_file_create(file, serf_request_get_alloc(request));
  } else {
    body_bkt = NULL;
  }

  *req_bkt = serf_request_bucket_request_create(
      request, ctx->method, ctx->path, body_bkt,
      serf_request_get_alloc(request));

  hdrs_bkt = serf_bucket_request_get_headers(*req_bkt);

  serf_bucket_headers_setn(hdrs_bkt, "User-Agent", "Serf/" SERF_VERSION_STRING);
  /* Shouldn't serf do this for us? */
  serf_bucket_headers_setn(hdrs_bkt, "Accept-Encoding", "gzip");
#ifdef CONNECTION_CLOSE_HDR
  serf_bucket_headers_setn(hdrs_bkt, "Connection", "close");
#endif

  /* Add the extra headers from the command line */
  if (ctx->req_hdrs != NULL) {
    serf_bucket_headers_do(ctx->req_hdrs, append_request_headers, hdrs_bkt);
  }

  *acceptor = ctx->acceptor;
  *acceptor_baton = ctx;
  *handler = ctx->handler;
  *handler_baton = ctx;

  return APR_SUCCESS;
}

static apr_status_t handle_response(serf_request_t *request,
                                    serf_bucket_t *response,
                                    void *handler_baton, apr_pool_t *pool) {
  serf_status_line sl;
  apr_status_t status;
  app_baton_t *ctx = handler_baton;

  if (!response) {
    /* A NULL response probably means that the connection was closed while
       this request was already written. Just requeue it. */
    serf_connection_t *conn = serf_request_get_conn(request);

    serf_connection_request_create(conn, setup_request, handler_baton);
    return APR_SUCCESS;
  }

  status = serf_bucket_response_status(response, &sl);
  if (status) {
    return status;
  }

  while (1) {
    struct iovec vecs[64];
    int vecs_read;
    apr_size_t bytes_written;

    status = serf_bucket_read_iovec(response, 8000, 64, vecs, &vecs_read);
    if (SERF_BUCKET_READ_ERROR(status))
      return status;

    /* got some data. print it out. */
    if (vecs_read) {
      // apr_file_writev(ctx->output_file, vecs, vecs_read, &bytes_written);
      gather_iovecs(ctx, vecs, vecs_read);
    }

    /* are we done yet? */
    if (APR_STATUS_IS_EOF(status)) {
      if (ctx->print_headers) {
        serf_bucket_t *hdrs;
        hdrs = serf_bucket_response_get_headers(response);
        while (1) {
          status = serf_bucket_read_iovec(hdrs, 8000, 64, vecs, &vecs_read);

          if (SERF_BUCKET_READ_ERROR(status))
            return status;

          if (vecs_read) {
            // apr_file_writev(ctx->output_file, vecs, vecs_read,
            // &bytes_written);
            gather_iovecs(ctx, vecs, vecs_read);
          }
          if (APR_STATUS_IS_EOF(status)) {
            break;
          }
        }
      }

      apr_atomic_inc32(&ctx->completed_requests);
      return APR_EOF;
    }

    /* have we drained the response so far? */
    if (APR_STATUS_IS_EAGAIN(status))
      return status;

    /* loop to read some more. */
  }
  /* NOTREACHED */
}

static apr_status_t credentials_callback(char **username, char **password,
                                         serf_request_t *request, void *baton,
                                         int code, const char *authn_type,
                                         const char *realm, apr_pool_t *pool) {
  app_baton_t *ctx = baton;

  if (ctx->auth_attempts > 0) {
    return SERF_ERROR_AUTHN_FAILED;
  } else {
    *username = (char *)ctx->username;
    *password = (char *)ctx->password;
    ctx->auth_attempts++;

    return APR_SUCCESS;
  }
}

int my_socket_poll(const apr_pollfd_t *pfd, void *ud) {
  app_baton_t *app = ud;

  /* Tell Serf to handle the read/write activity on this socket */
  apr_status_t status = serf_event_trigger(app->ctx, app->serf_baton, pfd);
  if (APR_STATUS_IS_TIMEUP(status))
    return APR_SUCCESS;
  if (status) {
    char buf[200];
    const char *err_string;
    err_string = serf_error_string(status);
    if (!err_string) {
      err_string = apr_strerror(status, buf, sizeof(buf));
    }

    printf("Error running context: (%d) %s\n", status, err_string);
    exit(1);
  }
  return APR_SUCCESS;
}

int my_socket_prerun(void *ud) {
  app_baton_t *app_ctx = ud;
  if (apr_atomic_read32(&app_ctx->completed_requests) > 0) {
    serf_connection_close(app_ctx->connection);
    apr_event_remove_prerun(app_ctx->prerun);

    app_ctx->on_complete(app_ctx->list.head, app_ctx->on_complete_ud);
    return APR_SUCCESS;
  }
  serf_context_prerun(app_ctx->ctx);
  return APR_SUCCESS;
}

apr_status_t my_socket_add(void *user_baton, apr_pollfd_t *pfd,
                           void *serf_baton) {
  app_baton_t *app = user_baton;
  app->serf_baton = serf_baton;

  apr_event_add_pollfd(app->loop, pfd, app->pool, my_socket_poll, app);
  return APR_SUCCESS;
}

apr_status_t my_socket_remove(void *user_baton, apr_pollfd_t *pfd,
                              void *serf_baton) {
  app_baton_t *app = user_baton;

  apr_event_remove_pollfd(app->loop, pfd);
  return APR_SUCCESS;
}

// int main(int argc, const char **argv) {
int start_download(const char *raw_url, apr_pool_t *pool, apr_loop_t *loop,
                   downloader_fn_t on_complete, void *ud) {
  apr_status_t status;
  serf_bucket_alloc_t *bkt_alloc;
  app_baton_t *app_ctx = apr_pcalloc(pool, sizeof(*app_ctx));
  // handler_baton_t *handler_ctx = apr_pcalloc(pool, sizeof(*handler_ctx));
  serf_bucket_t *req_hdrs = NULL;
  apr_uri_t url;
  const char *method, *req_body_path = NULL;
  int count, inflight;
  int i;
  int print_headers;
  const char *username = NULL;
  const char *password = "";
  const char *opt_arg;

  app_ctx->loop = loop;
  app_ctx->pool = pool;
  app_ctx->on_complete = on_complete;
  app_ctx->on_complete_ud = ud;

  /* serf_initialize(); */
  bkt_alloc = serf_bucket_allocator_create(pool, NULL, NULL);

  /* Default to one round of fetching with no limit to max inflight reqs. */
  count = 1;
  inflight = 0;
  /* Default to GET. */
  method = "GET";
  /* Do not print headers by default. */
  print_headers = 0;

  // username = opt_arg;
  // password = opt_arg;

  apr_uri_parse(pool, raw_url, &url);
  if (!url.port) {
    url.port = apr_uri_port_of_scheme(url.scheme);
  }
  if (!url.path) {
    url.path = "/";
  }

  if (strcasecmp(url.scheme, "https") == 0) {
    app_ctx->using_ssl = 1;
  } else {
    app_ctx->using_ssl = 0;
  }

  if (strcasecmp(method, "HEAD") == 0) {
    app_ctx->head_request = 1;
  } else {
    app_ctx->head_request = 0;
  }

  app_ctx->hostinfo = url.hostinfo;

  app_ctx->ctx =
      serf_context_create_ex(app_ctx, my_socket_add, my_socket_remove, pool);

  if (username) {
    serf_config_authn_types(app_ctx->ctx, SERF_AUTHN_ALL);
  } else {
    serf_config_authn_types(app_ctx->ctx,
                            SERF_AUTHN_NTLM | SERF_AUTHN_NEGOTIATE);
  }

  serf_config_credentials_callback(app_ctx->ctx, credentials_callback);

  /* ### Connection or Context should have an allocator? */
  app_ctx->bkt_alloc = bkt_alloc;
  app_ctx->ssl_ctx = NULL;

  status = serf_connection_create2(&app_ctx->connection, app_ctx->ctx, url,
                                   conn_setup, app_ctx, closed_connection,
                                   app_ctx, pool);
  if (status) {
    printf("Error creating connection: %d\n", status);
    apr_pool_destroy(pool);
    exit(1);
  }

  app_ctx->completed_requests = 0;
  app_ctx->print_headers = print_headers;
  apr_file_open_stdout(&app_ctx->output_file, pool);

  app_ctx->host = url.hostinfo;
  app_ctx->method = method;
  app_ctx->path = apr_pstrcat(pool, url.path, url.query ? "?" : "",
                              url.query ? url.query : "", NULL);
  app_ctx->username = username;
  app_ctx->password = password;
  app_ctx->auth_attempts = 0;

  app_ctx->req_body_path = req_body_path;

  app_ctx->acceptor = accept_response;
  app_ctx->handler = handle_response;
  app_ctx->req_hdrs = req_hdrs;

  serf_connection_set_max_outstanding_requests(app_ctx->connection, inflight);

  for (i = 0; i < count; i++) {
    /* We don't need the returned request here. */
    serf_connection_request_create(app_ctx->connection, setup_request, app_ctx);
  }

  app_ctx->prerun =
      apr_event_add_prerun(app_ctx->loop, my_socket_prerun, app_ctx);

  return 0;
}
