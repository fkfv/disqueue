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

#include "queue-compat.h"

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/http.h>
#include <wslay/wslay.h>

struct evws_message;
struct evws;

struct evws_connection {
  TAILQ_ENTRY(evws_connection) next;

  int active;

  /* underlying bufferevent for this connection */
  struct bufferevent *buffer;

  /* wslay context for this connection */
  wslay_event_context_ptr wslay;

  /* wslay callbacks for this websocket connection */
  struct wslay_event_callbacks wslay_callbacks;

  /* evws object that created this connection */
  struct evws *evws;

  /* messages from this connection */
  TAILQ_HEAD(evcon_messageq, evws_message) messages;

  int wslay_last_error;

  /* address of connected server */
  char *address;
  ev_uint16_t port;
};

struct evws {
  /* http server providing the connection negotiation. */
  struct evhttp *http;

  /* all live connections on this server */
  TAILQ_HEAD(evwsconq, evws_connection) connections;

  /* callback for data received on websocket */
  void (*datacb)(struct evws_message *, void *);
  void *datacbarg;

  void (*errorcb)(struct evws_connection *, void *);
  void *errorcbarg;

  void (*closecb)(struct evws_connection *, void *);
  void *closecbarg;
};

struct evws_message {
  TAILQ_ENTRY(evws_message) next;

  /* which connection this message came from */
  struct evws_connection *evcon;

  /* text/binary data from the frame */
  struct evbuffer *buffer;

  ev_uint8_t opcode;
};

/* callback to try upgrade requests */
void evws_upgrade_http_cb_(struct evhttp_request *req, void *user);

/* queue a message for sending. handles creating the wslay_event_msg */
void evws_connection_send_(struct evws_connection *conn,
                           ev_uint8_t opcode, const ev_uint8_t *message,
                           size_t length);

/* indicate to other socket that we would like to close */
void evws_connection_send_close_(struct evws_connection *conn);

/* create a new evws_connection from an evhttp_connection. this will mean that
   the socket associated with the evhttp_connection no longer supports the http
   protocol. */
struct evws_connection *evws_connection_new_(struct evhttp_connection *evcon);

struct evws_message *evws_message_new_(struct evws_connection *conn,
                                       struct wslay_event_on_msg_recv_arg *arg);
