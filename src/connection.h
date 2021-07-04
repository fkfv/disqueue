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

#ifndef CONNECTION_H
#define CONNECTION_H

#include <event2/http.h>
#include "ws.h"
#include "queue.h"

/* http callbacks */
void connection_http_callback_queues(struct evhttp_request *request, void *);
void connection_http_callback_queue(struct evhttp_request *request, void *);
void connection_http_callback_take(struct evhttp_request *request, void *);
void connection_http_callback_peek(struct evhttp_request *request, void *);
void connection_http_callback_put(struct evhttp_request *request, void *);

/* authentication callback */
void connection_http_authenticated(struct evhttp_request *request, void *user);
void *connection_http_auth_callback(struct auth *auth, const char *realm,
                                    void (*cb)(struct evhttp_request *, void *),
                                    void *cb_arg);
int connection_ws_authenticated(struct evhttp_request *request, void *user);

/* websocket callbacks */
void connection_ws_callback_wait(struct evws_message *message, void *);
void connection_ws_callback_close(struct evws_connection *connection, void *);
void connection_ws_callback_error(struct evws_connection *connection, void *); 

#endif
