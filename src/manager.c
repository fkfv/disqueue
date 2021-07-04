/*
  Copyright (c) 2021 Matthew (fkfv).

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include <json-c/json_object.h>
#include <json-c/json_tokener.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <stdio.h>
#include <stdlib.h>

#include "manager.h"
#include "manager-internal.h"
#include "hostnet.h"
#include "strcase.h"
#include "protocol.h"
#include "connection.h"

struct manager_context manager_context_;

void manager_startup(void)
{
  LIST_INIT(&manager_context_.servers);
  TAILQ_INIT(&manager_context_.queues);
  TAILQ_INIT(&manager_context_.wants);
}

int manager_add_server(struct evhttp *http, struct evws *ws, struct auth *auth,
                       const char *auth_realm)
{
  struct manager_server *server;

  server = calloc(1, sizeof(struct manager_server));
  if (!server) {
    return 0;
  }

  server->http = http;
  server->ws = ws;

  if (auth && auth_realm) {
    evhttp_set_cb(http, "/queues", connection_http_authenticated,
                  connection_http_auth_callback(auth, auth_realm,
                  connection_http_callback_queues, NULL));
    evhttp_set_cb(http, "/queue", connection_http_authenticated,
                  connection_http_auth_callback(auth, auth_realm,
                  connection_http_callback_queue, NULL));
    evhttp_set_cb(http, "/take", connection_http_authenticated,
                  connection_http_auth_callback(auth, auth_realm,
                  connection_http_callback_take, NULL));
    evhttp_set_cb(http, "/peek", connection_http_authenticated,
                  connection_http_auth_callback(auth, auth_realm,
                  connection_http_callback_peek, NULL));
    evhttp_set_cb(http, "/put", connection_http_authenticated,
                  connection_http_auth_callback(auth, auth_realm,
                  connection_http_callback_put, NULL));

    evws_set_upgrade_cb(ws, connection_ws_authenticated,
                        connection_http_auth_callback(auth, auth_realm,
                        NULL, NULL));
  } else {
    /* create http callbacks */
    evhttp_set_cb(http, "/queues", connection_http_callback_queues, NULL);
    evhttp_set_cb(http, "/queue", connection_http_callback_queue, NULL);
    evhttp_set_cb(http, "/take", connection_http_callback_take, NULL);
    evhttp_set_cb(http, "/peek", connection_http_callback_peek, NULL);
    evhttp_set_cb(http, "/put", connection_http_callback_put, NULL);
  }

  /* create ws callbacks */
  evws_bind_path(ws, "/take/ws");
  evws_set_cb(ws, connection_ws_callback_wait, NULL);
  evws_set_close_cb(ws, connection_ws_callback_close, NULL);
  evws_set_error_cb(ws, connection_ws_callback_error, NULL);

  LIST_INSERT_HEAD(&manager_context_.servers, server, next);
  return 1;
}

void manager_shutdown(void)
{
  struct manager_queue_want *want;
  struct manager_queue *queue;
  struct manager_server *server;

  while ((server = LIST_FIRST(&manager_context_.servers)) != NULL) {
    evhttp_del_cb(server->http, "/queues");
    evhttp_del_cb(server->http, "/queue");
    evhttp_del_cb(server->http, "/take");
    evhttp_del_cb(server->http, "/peek");
    evhttp_del_cb(server->http, "/put");

    evws_unbind_path(server->ws, "/take/ws");
  }

  while ((want = TAILQ_FIRST(&manager_context_.wants)) != NULL) {
    /* removes self from queue */
    manager_queue_want_free(want);
  }

  while ((queue = TAILQ_FIRST(&manager_context_.queues)) != NULL) {
    /* removes self from queue */
    manager_queue_free(queue);
  }
}

struct manager_queue *manager_queue_get(const char name[QUEUE_UUID_STR_LEN],
                                        int create_new)
{
  unsigned char r[QUEUE_UUID_LEN];
  unsigned char *used_r = r;
  struct manager_queue *q;

  if (name) {
    /* always fail if an invalid ID was given */
    if (strlen(name) != QUEUE_UUID_STR_LEN) {
      return NULL;
    }

    /* see if an existing queue is found */
    TAILQ_FOREACH(q, &manager_context_.queues, next) {
      if (!strcasecmp(q->id, name)) {
        return q;
      }
    }

    /* name wasn't found and not creating new - fail */
    if (!create_new) {
      return NULL;
    }

    /* the name will be in string format, it needs to be converted into byte
     format. since some of the bytes are read as integers, they need to be
     swapped to network byte order */
    if (sscanf(name, "%8llx-%4lx-%4lx-%4lx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
               (unsigned long long *)&r[0],
               (unsigned long *)&r[4],
               (unsigned long *)&r[6],
               (unsigned long *)&r[8],
               &r[10],
               &r[11],
               &r[12],
               &r[13],
               &r[14],
               &r[15]) != 10) {
      return NULL;
    }

    *(unsigned long *)&r[0] = htonl(*(unsigned long *)&r[0]);
    *(unsigned short *)&r[4] = htons(*(unsigned short *)&r[4]);
    *(unsigned short *)&r[6] = htons(*(unsigned short *)&r[6]);
    *(unsigned short *)&r[8] = htons(*(unsigned short *)&r[8]);
  } else {
    /* no name and create new not specified - always fails */
    if (!create_new) {
      return NULL;
    }

    /* set r null so a random id is generated for this queue */
    used_r = NULL;
  }

  /* try to create a new manager queue */
  q = calloc(1, sizeof(struct manager_queue));
  if (!q) {
    return NULL;
  }

  /* create the queue object */
  q->q = queue_new(used_r);
  if (!q->q) {
    free(q);
    return NULL;
  }

  /* cache the id of the new queue */
  queue_get_uuid(q->q, q->id);

  TAILQ_INSERT_TAIL(&manager_context_.queues, q, next);
  return q;
}

void manager_queue_free(struct manager_queue *queue)
{
  TAILQ_REMOVE(&manager_context_.queues, queue, next);
  queue_free(queue->q);
  free(queue);
}

struct manager_queue_want *manager_queue_want_new(const char *id,
                                                  struct evws_connection *con,
                                                  struct manager_queue *queue)
{
  struct manager_queue_want *want;

  want = calloc(1, sizeof(struct manager_queue_want));
  if (!want) {
    return NULL;
  }

  want->identifier = strdup(id);
  if (!want->identifier) {
    free(want);
    return NULL;
  }

  want->client = con;
  want->queue = queue;

  TAILQ_INSERT_TAIL(&manager_context_.wants, want, next);

  return want;
}

void manager_queue_want_free(struct manager_queue_want *want)
{
  TAILQ_REMOVE(&manager_context_.wants, want, next);
  free(want->identifier);
  free(want);
}

int manager_queue_want_is_cancelled(struct manager_queue_want *want)
{
  return want->cancelled;
}

struct evws_connection *manager_queue_want_get_connection(
  struct manager_queue_want *want)
{
  return want->client;
}

const char *manager_queue_want_get_identifier(struct manager_queue_want *want)
{
  return want->identifier;
}

struct queue *manager_queue_get_queue(struct manager_queue *queue)
{
  return queue->q;
}

const char *manager_queue_get_id(struct manager_queue *queue)
{
  return queue->id;
}

int manager_queue_foreach(int(*cb)(struct manager_queue *, void *), void *arg)
{
  struct manager_queue *queue;

  queue = TAILQ_FIRST(&manager_context_.queues);
  while (queue != NULL) {
    if (cb(queue, arg) == 0) {
      return 0;
    }

    queue = TAILQ_NEXT(queue, next);
  }

  return 1;
}

void manager_queue_want_remove(struct manager_queue *queue)
{
  struct manager_queue_want *want;
  struct manager_queue_want *next;

  want = TAILQ_FIRST(&manager_context_.wants);
  while (want != NULL) {
    next = TAILQ_NEXT(want, next);
    if (want->queue == queue) {
      want->cancelled = 1;
    }

    want = next;
  }
}

void manager_queue_want_close(struct evws_connection *connection)
{
  struct manager_queue_want *want;
  struct manager_queue_want *next;

  want = TAILQ_FIRST(&manager_context_.wants);
  while (want != NULL) {
    next = TAILQ_NEXT(want, next);
    if (want->client == connection) {
      want->cancelled = 1;
    }

    want = next;
  }
}
