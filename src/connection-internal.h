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

#ifndef CONNECTION_INTERNAL_H
#define CONNECTION_INTERNAL_H

#include <json-c/json_object.h>
#include <event2/keyvalq_struct.h>
#include "manager.h"
#include "ws.h"

struct connection_params {
  /* authentication for this callback */
  struct auth *auth;
  const char *realm;

  /* real callback */
  void (*cb)(struct evhttp_request *, void *);
  void *cb_arg;
};

/* validate a request. if create_new is 1 this will succeed if the given queue
   name is none or valid, and create the queue. if the queue name is present
   but invalid or create_new is 0 and the queue does not exist the function
   will return NULL and send an error describing the problem.
 */
struct manager_queue *connection_http_validate_(struct evhttp_request *request,
                                                int create_new,
                                                struct evkeyvalq *params);

/* read requests */
int connection_http_read_(struct evhttp_request *request,
                          struct evkeyvalq *params);
struct json_object *connection_ws_read_(struct evws_message *message);

/* create responses. releases `payload` parameter. */
void connection_http_json_(struct evhttp_request *request,
                           struct evkeyvalq *params,
                           int code, struct json_object *object);
void connection_http_error_(struct evhttp_request *request,
                            struct evkeyvalq *params,
                            int code, const char *message);
void connection_http_payload_(struct evhttp_request *request,
                              struct evkeyvalq *params,
                              struct json_object *payload);
void connection_ws_json_(struct evws_connection *connection,
                         struct json_object *object);
void connection_ws_error_(struct evws_connection *connection,
                          const char *message);
void connection_ws_payload_(struct evws_connection *connection,
                            struct json_object *payload);

/* create www-authenticate header */
void connection_http_auth_required_(struct evhttp_request *request,
                                    const char *realm);

/* http callbacks */
void connection_http_callback_list_(struct evhttp_request *request, void *);
void connection_http_callback_new_(struct evhttp_request *request, void *);
void connection_http_callback_info_(struct evhttp_request *request, void *);
void connection_http_callback_delete_(struct evhttp_request *request, void *);

/* queue callbacks */
void connection_queue_callback_wait_(struct queue_item *item, void *user);

#endif
