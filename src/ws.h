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
#include <wslay/wslay.h>

struct evws;
struct evws_connection;

/**
 * Create a new WebSocket server.
 *
 * @param http the http server to use for connection upgrades
 * @return a pointer to a newly initialized evws server structure or NULL on
 *   error
 * @see evws_free()
 */
struct evws *evws_new(struct evhttp *http);

/**
 * Binds the websocket to the server. Any connections making it to the gencb of
 * the http server will be checked and a websocket upgrade will be attempted.
 *
 * This overrides the gencb of the http server.
 *
 * @param ws a pointer to an evws object
 * @see evws_bind_path()
 */
void evws_bind(struct evws *ws);

/**
 * Binds the websocket to a path on the server. A websocket upgrade will be
 * attempted for all requests to this path.
 *
 * Can be called multiple times to bind the same websocket server to multiple
 * different paths.
 *
 * @param ws a pointer to an evws object
 * @param path the path to attempt upgrades on
 * @return 0 on success, -1 if the callback existed already, -2 on failure
 */
int evws_bind_path(struct evws *ws, const char *path);

/** Unbinds the websocket from a path */
int evws_unbind_path(struct evws *ws, const char *path);

/**
 * Set the callback for when data is received from the websocket.
 *
 * @param ws a pointer to an evws object
 * @param cb the callback for received data
 * @param arg optional context argument for the callback
 */
void evws_set_cb(struct evws *ws,
                 void (*cb)(struct evws_message *, void *), void *arg);

void evws_set_open_cb(struct evws *ws,
                      void (*cb)(struct evws_connection *, void *), void *arg);

void evws_set_error_cb(struct evws *ws,
                       void (*cb)(struct evws_connection *, void *), void *arg);

void evws_set_close_cb(struct evws *ws,
                       void (*cb)(struct evws_connection *, void *), void *arg);

void evws_free(struct evws *ws);

/**
 * Send data over the websocket. text data must be NULL terminated.
 *
 * @param conn an evws_connection that has been established and is ready to
 *   exchange frames
 */
void evws_connection_send(struct evws_connection *conn, const char *text);
void evws_connection_send_binary(struct evws_connection *conn,
                                const unsigned char *data, size_t len);

void evws_connection_free(struct evws_connection *conn);

void evws_connection_get_peer(struct evws_connection *conn,
                              char **address, ev_uint16_t *port);

int evws_connection_get_last_error(struct evws_connection *conn);

void evws_message_free(struct evws_message *msg);

struct evws_connection *evws_message_get_connection(struct evws_message *msg);
struct evbuffer *evws_message_get_buffer(struct evws_message *msg);
ev_uint8_t evws_message_get_opcode(struct evws_message *msg);

/**
 * Own a message. When the message is owned it will not be freed after the
 * data notify callback is invoked. It must be manually freed using
 * evws_message_free().
 *
 * @param conn an evws_message that is still in the callback stage
 */
void evws_message_own(struct evws_message *msg);
