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

#include <event2/event.h>
#include <event2/http.h>

#include "ws.h"
#include "manager.h"

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

  manager_startup(http, ws);

  if (event_base_dispatch(base) == -1) {
    evhttp_free(http);
    event_base_free(base);
    return 1;
  }

  manager_shutdown();

#ifdef _WIN32
  WSACleanup();
#endif

  return 0;
}
