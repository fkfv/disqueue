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

#include "config.h"
#include "ws.h"
#include "manager.h"
#include "getopt.h"
#include "ssl.h"

#define options "c:h"

int create_server(struct event_base *base, struct config_server *server)
{
  struct evhttp *http = NULL;
  struct evws *ws = NULL;

  http = evhttp_new(base);
  if (!http) {
    goto error;
  }

  if (evhttp_bind_socket(http,
                         config_server_get_hostname(server),
                         config_server_get_port(server)) != 0) {
    goto error;
  }

  ws = evws_new(http);
  if (!ws) {
    goto error;
  }

  if (config_server_has_security(server)) {
    if (!ssl_setup()) {
      goto error;
    }

    if (!ssl_load_certificate(config_server_get_certificate(server)) ||
        !ssl_load_privatekey(config_server_get_privatekey(server))) {
      goto error;
    }

    ssl_use(http);
  }

  if (!manager_add_server(http, ws)) {
    goto error;
  }

  return 1;

error:
  ssl_destroy();

  if (http) {
    evhttp_free(http);
  }

  if (ws) {
    evws_free(ws);
  }

  return 0;
}

void usage(const char *progname)
{
  printf("-- %s\n", progname);
  printf("  -c [config file]: load configuration from file - default config\n"\
         "      is to run a server on 127.0.0.1:3682\n");
  printf("  -h              : show help\n");
}

/* process command line arguments, return 1 if the arguments have been parsed
   and the program should continue */
int command_line(int argc, char *argv[])
{
  int c;
  int show_help = 0;
  const char *config_file = NULL;

  while ((c = getopt(argc, argv, options)) != -1) {
    switch (c) {
    case 'c':
      config_file = optarg;
      break;
    case 'h':
      show_help = 1;
      break;
    case '?':
      if (optopt == 'c' || optopt == 'a' || optopt == 'p') {
        fprintf(stderr, "option -%c requres an argument\n", optopt);
      } else if (isprint(optopt)) {
        fprintf(stderr, "unknown option -%c\n", optopt);
      } else {
        fprintf(stderr, "invalid option\n");
      }
      show_help = 1;
      break;
    }
  }

  if (show_help) {
    usage(argv[0]);
    return 0;
  }

  if (config_file) {
    if (!config_load_file(config_file)) {
      fprintf(stderr, "failed to load %s\n", config_file);
      return EINVAL;
    }
  }

  return -1;
}

int main(int argc, char *argv[])
{
  int c;
  struct config_server *server;
  struct event_base *base;
#ifdef _WIN32
  WSADATA wsa;

  WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

  if ((c = command_line(argc, argv)) != -1) {
    return c;
  }

  if (!config_iter_server_begin()) {
    fprintf(stderr, "no servers to run on\n");
    return 1;
  }

  base = event_base_new();
  if (!base) {
    fprintf(stderr, "failed to start libevent\n");
    return 1;
  }

  manager_startup();

  while ((server = config_iter_server_next()) != NULL) {
    if (!create_server(base, server)) {
      fprintf(stderr, "failed to add server\n");
      return 1;
    }
  }

  if (event_base_dispatch(base) == -1) {
    fprintf(stderr, "failed to run libevent loop\n");
    return 1;
  }

  manager_shutdown();

#ifdef _WIN32
  WSACleanup();
#endif

  return 0;
}
