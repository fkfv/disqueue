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

#include <event2/http.h>
#include "ws.h"
#include "queue-compat.h"
#include "queue.h"

/* http/ws callbacks */
void manager_bad_request_cb_(struct evhttp_request *request, void *user);
void manager_internal_error_(struct evhttp_request *request);

void manager_queues_cb_(struct evhttp_request *request, void *user);
void manager_queues_list_cb_(struct evhttp_request *request, void *user);
void manager_queues_new_cb_(struct evhttp_request *request, void *user);

void manager_queue_cb_(struct evhttp_request *request, void *user);
void manager_queue_info_cb_(struct evhttp_request *request, void *user);
void manager_queue_delete_cb_(struct evhttp_request *request, void *user);

void manager_take_item_cb_(struct evhttp_request *request, void *user);

void manager_peek_cb_(struct evhttp_request *request, void *user);
void manager_peek_item_cb_(struct evhttp_request *request, void *user);
void manager_peek_done_cb_(struct evhttp_request *request, void *user);

void manager_wait_item_cb_(struct evws_message *message, void *user);
void manager_wait_close_cb_(struct evws_connection *connection, void *user);

void manager_put_item_cb_(struct evhttp_request *request, void *user);

/* queue callbacks */
void manager_wait_receive_cb_(struct queue_item *item, void *user);

/* validate request, return 0 if an error was sent, 1 if the request can
   continue */
int manager_validate_(struct evhttp_request *request,
                      struct manager_queue **q, int create_new,
                      struct evkeyvalq *params);

struct manager_queue {
  TAILQ_ENTRY(manager_queue) next;

  /* queue being managed */
  struct queue *q;

  /* cached pretty print id of queue */
  char id[QUEUE_UUID_STR_LEN + 1/*NULL*/];
};

/* details of a waiting event */
struct manager_queue_want {
  TAILQ_ENTRY(manager_queue_want) next;

  /* an identifier so the client can identify this response */
  char *identifier;

  /* if the want is cancelled */
  int cancelled;

  /* the queue a want is waiting on */
  struct manager_queue *queue;

  /* websocket connection to the client */
  struct evws_connection *client;
};

struct manager_context {
  struct evhttp *http;
  struct evws *ws;

  /* queues being managed */
  TAILQ_HEAD(mqhead, manager_queue) queues;

  /* all queue items being waited on */
  TAILQ_HEAD(mqwhead, manager_queue_want) wants;
};

/* get a manager_queue, optionally create it if it does not exist */
struct manager_queue *manager_get_queue_(const char name[QUEUE_UUID_STR_LEN],
                                         int create_new);
void manager_queue_free_(struct manager_queue *queue);

/* create/free want events */
struct manager_queue_want *manager_queue_want_new_(const char *id,
                                                   struct evws_connection *con,
                                                   struct manager_queue *queue);
void manager_queue_want_free_(struct manager_queue_want *want);

/* the global context instance */
extern struct manager_context manager_context_;
