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

#ifndef MANAGER_H
#define MANAGER_H

#include <event2/http.h>
#include "ws.h"
#include "queue.h"

struct manager_queue;
struct manager_queue_want;

void manager_startup(void);
int manager_add_server(struct evhttp *http, struct evws *ws);
void manager_shutdown(void);

struct manager_queue *manager_queue_get(const char name[QUEUE_UUID_STR_LEN],
                                        int create_new);
void manager_queue_free(struct manager_queue *queue);

/* get the queue being managed. the returned queue MUST NOT be destroyed */
struct queue *manager_queue_get_queue(struct manager_queue *queue);

const char *manager_queue_get_id(struct manager_queue *queue);

struct manager_queue_want *manager_queue_want_new(const char *id,
                                                  struct evws_connection *con,
                                                  struct manager_queue *queue);
void manager_queue_want_free(struct manager_queue_want *want);

/* iterate each queue, calling cb with the item. if one of the callbacks return
   0 then iteration will be stopped and the function returns 0, otherwise it
   returns 1 */
int manager_queue_foreach(int(*cb)(struct manager_queue *, void *), void *arg);

/* remove all wants for this queue */
void manager_queue_want_remove(struct manager_queue *queue);

/* remove all wants for a closed connection */
void manager_queue_want_close(struct evws_connection *connection);

int manager_queue_want_is_cancelled(struct manager_queue_want *want);
struct evws_connection *manager_queue_want_get_connection(
  struct manager_queue_want *want);
const char *manager_queue_want_get_identifier(struct manager_queue_want *want);

#endif
