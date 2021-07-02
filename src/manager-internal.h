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

struct manager_server {
  LIST_ENTRY(manager_server) next;

  struct evhttp *http;
  struct evws *ws;
};

struct manager_context {
  LIST_HEAD(mshead, manager_server) servers;

  /* queues being managed */
  TAILQ_HEAD(mqhead, manager_queue) queues;

  /* all queue items being waited on */
  TAILQ_HEAD(mqwhead, manager_queue_want) wants;
};

/* the global context instance */
extern struct manager_context manager_context_;
