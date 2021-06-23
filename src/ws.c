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

#include "ws.h"
#include "ws-internal.h"

#include <stdlib.h>
#include <stdio.h>

#include <event2/event.h>
#include <event2/util.h>
#include <openssl/bio.h>
#include <openssl/evp.h>


#define REQ_VERSION_BEFORE(major, minor, major_v, minor_v)     \
  ((major) < (major_v) ||                                      \
      ((major) == (major_v) && (minor) < (minor_v)))

#define SHA_HASH_SIZE 20

const char security_key[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

const char *create_security_key(const char *key)
{
  BIO *mem;
  BIO *sha1;
  BIO *base64;
  char accept_key[(SHA_HASH_SIZE * 8 + 4) / 6 + 2/*round_up+nullbyte*/] = {0};
  char digest[SHA_HASH_SIZE];
  int rd;

  mem = BIO_new(BIO_s_mem());
  sha1 = BIO_new(BIO_f_md());

  sha1 = BIO_push(sha1, mem);
  BIO_set_md(sha1, EVP_sha1());

  BIO_write(sha1, key, strlen(key));
  BIO_write(sha1, security_key, strlen(security_key));

  rd = BIO_gets(sha1, digest, sizeof(digest));
  if (rd != SHA_HASH_SIZE) {
    BIO_free_all(sha1);
    fprintf(stderr, "rd: %d\n", rd);
    return NULL;
  }

  BIO_reset(mem);
  mem = BIO_pop(sha1);
  BIO_free(sha1);

  base64 = BIO_new(BIO_f_base64());

  BIO_set_flags(base64, BIO_FLAGS_BASE64_NO_NL);

  base64 = BIO_push(base64, mem);

  BIO_write(base64, digest, sizeof(digest));
  BIO_flush(base64);

  rd = BIO_read(mem, accept_key, sizeof(accept_key) - 1/*-nullbyte*/);
  BIO_free_all(base64);
  if (rd <= 0) {
    fprintf(stderr, "rd: %d\n", rd);
    return NULL;
  }

  return strdup(accept_key);
}

int header_contains(char *value, const char *look)
{
  char *token;

  token = strtok(value, ",");
  while (token != NULL) {
    while (token[0] == ' ') {
      token++;
    }

    if (!evutil_ascii_strcasecmp(token, look)) {
      return 1;
    }

    token = strtok(NULL, ",");
  }

  return 0;
}

void evws_upgrade_http_cb_(struct evhttp_request *req, void *user)
{
  struct evkeyvalq *headers = evhttp_request_get_input_headers(req);
  const char *upgrade_header;
  const char *connection_header;
  const char *version_header;
  const char *request_key;
  const char *response_key;
  struct evws_connection *connection;
  struct evws *ws = (struct evws *)user;
  char major;
  char minor;

  /* do not handle if this is not an upgrade request */
  upgrade_header = evhttp_find_header(headers, "Upgrade");
  connection_header = evhttp_find_header(headers, "Connection");
  if (!upgrade_header || !connection_header ||
      !header_contains(upgrade_header, "websocket") ||
      !header_contains(connection_header, "Upgrade")) {
    evhttp_send_error(req, HTTP_BADREQUEST, NULL);
    fprintf(stderr, "Upgrade: %s, Connection: %s\n", upgrade_header,
            connection_header);
    return;
  }

  /* request method MUST be GET and version MUST be >= 1.1 */
  evhttp_request_get_version(req, &major, &minor);
  if (evhttp_request_get_command(req) != EVHTTP_REQ_GET ||
      REQ_VERSION_BEFORE(major, minor, 1, 1)) {
    evhttp_send_error(req, HTTP_BADREQUEST, NULL);
    fprintf(stderr, "HTTP Version: %d.%d\n", major, minor);
    return;
  }

  /* check version support - wslay is version 13. */
  version_header = evhttp_find_header(headers, "Sec-WebSocket-Version");
  if (!version_header || atoi(version_header) != 13) {
    evhttp_add_header(evhttp_request_get_output_headers(req),
                      "Sec-WebSocket-Version", "13");
    evhttp_send_error(req, HTTP_BADREQUEST, NULL);
    fprintf(stderr, "Sec-WebSocket-Version: %s\n", version_header);
    return;
  }

  /* create the security key */
  request_key = evhttp_find_header(headers, "Sec-WebSocket-Key");
  if (!request_key) {
    evhttp_send_error(req, HTTP_BADREQUEST, NULL);
    fprintf(stderr, "Sec-WebSocket-Key: %s\n", request_key);
    return;
  }

  response_key = create_security_key(request_key);
  if (!response_key) {
    evhttp_send_error(req, HTTP_INTERNAL, NULL);
    fprintf(stderr, "Sec-WebSocket-Accept: %s\n", response_key);
    return;
  }

  /* add the response key */
  evhttp_add_header(evhttp_request_get_output_headers(req),
                    "Sec-WebSocket-Accept", response_key);
  free(response_key);

  /* add the upgrade headers */
  evhttp_add_header(evhttp_request_get_output_headers(req),
                    "Upgrade", "websocket");
  evhttp_add_header(evhttp_request_get_output_headers(req),
                    "Connection", "Upgrade");

  /* send the response */
  /* TODO: Make sure evhttp_is_request_connection_close returns false */
  evhttp_send_reply(req, /*SWITCHING_PROTOCOLS*/101, NULL, NULL);

  /* switch the protocol */
  connection = evws_connection_new_(evhttp_request_get_connection(req));
  if (!connection) {
    evhttp_send_error(req, HTTP_INTERNAL, NULL);
    fprintf(stderr, "evws_connection_new_ failed\n");
    return;
  }

  connection->evws = ws;

  /* http -> ws transition complete, notify the ws object about the new
     connection. */
  TAILQ_INSERT_TAIL(&ws->connections, connection, next);

  if (ws->opencb) {
    ws->opencb(connection, ws->opencbarg);
  }
}

struct evws *evws_new(struct evhttp *http)
{
  struct evws *ws;

  if ((ws = calloc(1, sizeof(struct evws))) == NULL) {
    fprintf(stderr, "evws_new: calloc\n");
    return NULL;
  }

  ws->http = http;

  TAILQ_INIT(&ws->connections);

  return ws;
}

void evws_bind(struct evws *ws)
{
  evhttp_set_gencb(ws->http, evws_upgrade_http_cb_, ws);
}

int evws_bind_path(struct evws *ws, const char *path)
{
  return evhttp_set_cb(ws->http, path, evws_upgrade_http_cb_, ws);
}

int evws_unbind_path(struct evws *ws, const char *path)
{
  return evhttp_del_cb(ws->http, path);
}

void evws_set_cb(struct evws *ws,
                 void (*cb)(struct evws_message *, void *), void *arg)
{
  ws->datacb = cb;
  ws->datacbarg = arg;
}

void evws_set_open_cb(struct evws *ws,
                      void (*cb)(struct evws_connection *, void *), void *arg)
{
  ws->opencb = cb;
  ws->opencbarg = arg;
}

void evws_set_error_cb(struct evws *ws,
                       void (*cb)(struct evws_connection *, void *), void *arg)
{
  ws->errorcb = cb;
  ws->errorcbarg = arg;
}

void evws_set_close_cb(struct evws *ws,
                       void (*cb)(struct evws_connection *, void *), void *arg)
{
  ws->closecb = cb;
  ws->closecbarg = arg;
}

void evws_free(struct evws *ws)
{
  struct evws_connection *connection;

  while ((connection = TAILQ_FIRST(&ws->connections)) != NULL) {
    /* evws_connection_free removes */
    evws_connection_free(connection);
  }

  free(ws);
}

void evws_connection_send(struct evws_connection *conn, const char *text)
{
  evws_connection_send_(conn, WSLAY_TEXT_FRAME, text, strlen(text));
}

void evws_connection_send_binary(struct evws_connection *conn,
                                const unsigned char *data, size_t len)
{
  evws_connection_send_(conn, WSLAY_BINARY_FRAME, data, len);
}

void evws_connection_free(struct evws_connection *conn)
{
  struct evws_message *message;

  /* TODO: The other socket expects us to receive a close message from them
     before we shutdown, but this does not wait! */
  if (conn->active) {
    evws_connection_send_close_(conn);
    conn->active = 0;
  }

  if (conn->evws->closecb) {
    conn->evws->closecb(conn, conn->evws->closecbarg);
  }

  while ((message = TAILQ_FIRST(&conn->messages)) != NULL) {
    /* evws_message_free removes */
    evws_message_free(message);
  }

  TAILQ_REMOVE(&conn->evws->connections, conn, next);

  bufferevent_free(conn->buffer);
  free(conn->address);
  free(conn);
}

void evws_connection_get_peer(struct evws_connection *conn,
                              char **address, ev_uint16_t *port)
{
  *address = conn->address;
  *port = conn->port;
}

void evws_message_free(struct evws_message *msg)
{
  TAILQ_REMOVE(&msg->evcon->messages, msg, next);

  evbuffer_free(msg->buffer);
  free(msg);
}

struct evws_connection *evws_message_get_connection(struct evws_message *msg)
{
  return msg->evcon;
}

struct evbuffer *evws_message_get_buffer(struct evws_message *msg)
{
  return msg->buffer;
}

ev_uint8_t evws_message_get_opcode(struct evws_message *msg)
{
  return msg->opcode;
}

void evws_message_own(struct evws_message *msg)
{
  msg->own = 1;
}

void evws_connection_event_cb_(struct bufferevent *bev, short events,
                               void *user)
{
  struct evws_connection *ws = (struct evws_connection *)user;

  if (events & BEV_EVENT_EOF) {
    if (ws->evws->closecb && ws->active) {
      ws->evws->closecb(ws, ws->evws->closecbarg);
    }
  } else if (events & BEV_EVENT_ERROR) {
    if (ws->evws->errorcb) {
      ws->evws->errorcb(ws, ws->evws->errorcbarg);
    }
  }
}

void evws_close_(struct bufferevent *bev, void *user)
{
  struct evws_connection *ws = (struct evws_connection *)user;

  ws->active = 0;
  if (ws->evws->closecb) {
    ws->evws->closecb(ws, ws->evws->closecbarg);
  }
}

void evws_connection_write_(struct evws_connection *ws)
{
  if (wslay_event_want_write(ws->wslay)) {
    ws->wslay_last_error = wslay_event_send(ws->wslay);
    if (ws->wslay_last_error < 0) {
      fprintf(stderr, "event send: %d\n", ws->wslay_last_error);
      evws_close_(NULL, ws);
      return;
    }
  }

  if (wslay_event_get_close_sent(ws->wslay)) {
    /* mark close notify event when all data has been transmitted */
    bufferevent_setcb(ws->buffer, NULL, evws_close_, evws_connection_event_cb_,
                      ws);
  }
}

void evws_connection_read_cb_(struct bufferevent *bev, void *user)
{
  struct evws_connection *ws = (struct evws_connection *)user;

  ws->wslay_last_error = wslay_event_recv(ws->wslay);
  if (ws->wslay_last_error < 0) {
    fprintf(stderr, "evws_connection_read_cb_: %d\n", ws->wslay_last_error);
    evws_close_(NULL, ws);
    return;
  }

  evws_connection_write_(ws);
}

ev_ssize_t evws_wslay_send_callback_(wslay_event_context_ptr ctx,
                                     const ev_uint8_t *data, size_t len,
                                     int flags, void *user)
{
  struct evws_connection *ws = (struct evws_connection *)user;

  ws->wslay_last_error = 0;
  if (bufferevent_write(ws->buffer, data, len) != 0) {
    fprintf(stderr, "bufferevent_write fail\n");
    /* we don't know if the bufferevent error was because of blocking */
    wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
    return -1;
  }

  /* always takes all of the data and buffers it for sending */
  return len;
}

ev_ssize_t evws_wslay_recv_callback_(wslay_event_context_ptr ctx,
                                     ev_uint8_t *data, size_t len,
                                     int flags, void *user)
{
  struct evws_connection *ws = (struct evws_connection *)user;
  struct evbuffer *buffer = bufferevent_get_input(ws->buffer);
  size_t read;

  /* bufferevent_read() does not provide a way to tell if there was an error or
     just no data yet. */
  ws->wslay_last_error = 0;
  read = evbuffer_remove(buffer, data, len);
  if (read < 0) {
    fprintf(stderr, "evbuffer_remove fail\n");
    /* we don't know if the evbuffer error was because of blocking */
    wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
    return -1;
  }

  return read;
}

void evws_wslay_on_msg_recv_callback_(wslay_event_context_ptr ctx,
                                      struct wslay_event_on_msg_recv_arg *arg,
                                      void *user)
{
  struct evws_connection *ws = (struct evws_connection *)user;
  struct evws_message *message;

  if (!wslay_is_ctrl_frame(arg->opcode)) {
    if (ws->evws->datacb) {
      if (arg->opcode == WSLAY_TEXT_FRAME ||
          arg->opcode == WSLAY_BINARY_FRAME) {
        message = evws_message_new_(ws, arg);
        if (!message) {
          return;
        }

        ws->evws->datacb(message, ws->evws->datacbarg);
        if (!message->own) {
          evws_message_free(message);
        }
      }
    }
  }
}

struct evws_connection *evws_connection_new_(struct evhttp_connection *evcon)
{
  struct evws_connection *ws;
  struct bufferevent *bev;
  char *address;
  ev_uint16_t port;

  if ((ws = calloc(1, sizeof(struct evws_connection))) == NULL) {
    fprintf(stderr, "evws_connection_new_: calloc\n");
    return NULL;
  }

  evhttp_connection_get_peer(evcon, &address, &port);
  ws->address = strdup(address);
  ws->port = port;

  if ((bev = evhttp_connection_take_ownership(evcon)) == NULL) {
    fprintf(stderr, "evws_connection_new_: take_ownership\n");
    free(ws->address);
    free(ws);
    return NULL;
  }

  ws->active = 1;
  ws->buffer = bev;

  /* initialise wslay */
  ws->wslay_callbacks.recv_callback = evws_wslay_recv_callback_;
  ws->wslay_callbacks.send_callback = evws_wslay_send_callback_;
  ws->wslay_callbacks.on_msg_recv_callback = evws_wslay_on_msg_recv_callback_;
  ws->wslay_last_error = wslay_event_context_server_init(&ws->wslay,
                                                         &ws->wslay_callbacks,
                                                         ws);
  if (ws->wslay_last_error) {
    fprintf(stderr, "initialise fail: %d\n", ws->wslay_last_error);
    bufferevent_free(bev);
    free(ws->address);
    free(ws);
    return NULL;
  }

  bufferevent_setcb(bev, evws_connection_read_cb_, NULL,
                    evws_connection_event_cb_, ws);
  /* evhttp_connection_take_ownership disabled both read & write. */
  bufferevent_enable(bev, EV_READ | EV_WRITE);

  TAILQ_INIT(&ws->messages);

  return ws;
}

struct evws_message *evws_message_new_(struct evws_connection *conn,
                                       struct wslay_event_on_msg_recv_arg *arg)
{
  struct evws_message *message;

  message = calloc(1, sizeof(struct evws_message));
  if (!message) {
    return NULL;
  }

  message->evcon = conn;
  message->opcode = arg->opcode;
  message->buffer = evbuffer_new();
  if (!message->buffer) {
    free(message);
    return NULL;
  }

  if (evbuffer_add(message->buffer, arg->msg, arg->msg_length) != 0) {
    evbuffer_free(message->buffer);
    free(message);
    return NULL;
  }

  TAILQ_INSERT_TAIL(&conn->messages, message, next);

  return message;
}

void evws_connection_send_(struct evws_connection *conn,
                           ev_uint8_t opcode, const ev_uint8_t *message,
                           size_t length)
{
  struct wslay_event_msg msg;

  if (!conn->active) {
    return;
  }

  msg.opcode = opcode;
  msg.msg = message;
  msg.msg_length = length;

  conn->wslay_last_error = wslay_event_queue_msg(conn->wslay, &msg);
  if (conn->wslay_last_error < 0) {
    evws_close_(NULL, conn);
    return;
  }

  evws_connection_write_(conn);
}

void evws_connection_send_close_(struct evws_connection *conn)
{
  if (!conn->active) {
    return;
  }

  conn->wslay_last_error = wslay_event_queue_close(conn->wslay, 0, NULL, 0);
  if (conn->wslay_last_error < 0) {
    evws_close_(NULL, conn);
    return;
  }

  evws_connection_write_(conn);
}
