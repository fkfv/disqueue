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

#include <stdio.h>
#include <stdlib.h>

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>

#include "ws.h"

void ws_print(struct evws_connection* conn, const char* msg, int len)
{
  char* address;
  ev_uint16_t port;

  evws_connection_get_peer(conn, &address, &port);
  printf("[%s:%d]: %.*s\n", address, port, len, msg);
}

void ws_callback(struct evws_message *msg, void *user)
{
  struct evws_connection *connection;
  struct evbuffer *buffer;
  void *data;
  size_t len;

  connection = evws_message_get_connection(msg);
  buffer = evws_message_get_buffer(msg);
  len = evbuffer_get_length(buffer);
  data = calloc(len, sizeof(char));

  evbuffer_copyout(buffer, data, len);
  ws_print(connection, data, len);

  evws_connection_send(connection, "hello how are you!");

  free(data);
}

void ws_error(struct evws_connection *conn, void *user)
{
  ws_print(conn, "ws error!", 10);
}

void ws_close(struct evws_connection* conn, void* user)
{
  ws_print(conn, "ws close!", 10);
}

int main(int argc, char *argv[])
{
  struct event_base *base;
  struct evhttp *http;
  struct evws *ws;
#ifdef _WIN32
  WSADATA wsa;

  WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

  base = event_base_new();
  if (!base) {
    return 1;
  }

  http = evhttp_new(base);
  if (!http) {
    event_base_free(base);
    return 1;
  }

  if (evhttp_bind_socket(http, "0.0.0.0", 3682) != 0) {
    evhttp_free(http);
    event_base_free(base);
    return 1;
  }

  ws = evws_new(http);
  if (!ws) {
    evhttp_free(http);
    event_base_free(base);
    return 1;
  }

  if (evws_bind_path(ws, "/ws") != 0) {
    evws_free(ws);
    evhttp_free(http);
    event_base_free(base);
    return 1;
  }

  evws_set_cb(ws, ws_callback, NULL);
  evws_set_error_cb(ws, ws_error, NULL);
  evws_set_close_cb(ws, ws_close, NULL);

  if (event_base_dispatch(base) == -1) {
    evhttp_free(http);
    event_base_free(base);
    return 1;
  }

#ifdef _WIN32
  WSACleanup();
#endif

  return 0;
}
