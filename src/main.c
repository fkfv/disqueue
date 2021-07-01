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

#define options "a:p:c:h"

/* start and run libevent */
int event_startup(const char *hostname, short port)
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

  if (evhttp_bind_socket(http, hostname, port) != 0) {
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

void usage(const char *progname)
{
  printf("-- %s\n", progname);
  printf("  -c [config file]: load configuration from file (default: none)\n");
  printf("  -a [address]    : listen on address (default: 127.0.0.1)\n");
  printf("  -p [port]       : listen on port (default: 3682)\n");
  printf("  -h              : show help\n");
}

/* process command line arguments, return 1 if the arguments have been parsed
   and the program should continue */
int command_line(int argc, char *argv[])
{
  int c;
  int show_help = 0;
  const char *config_file = NULL;
  const char *hostname = NULL;
  const char *port = NULL;

  while ((c = getopt(argc, argv, options)) != -1) {
    switch (c) {
    case 'c':
      config_file = optarg;
      break;
    case 'a':
      hostname = optarg;
      break;
    case 'p':
      port = optarg;
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
    if (!config_parse(config_file)) {
      fprintf(stderr, "failed to load %s\n", config_file);
      return 0;
    }
  }

  if (hostname) {
    if (!config_set_string("/host", hostname)) {
      fprintf(stderr, "failed to set hostname\n");
      return 0;
    }
  }

  if (port) {
    if (!config_set_short("/port", (short)atoi(port))) {
      fprintf(stderr, "failed to set port\n");
      return 0;
    }
  }

  return 1;
}

int main(int argc, char *argv[])
{
  const char *hostname;
  short port;

  if (!config_load_defaults()) {
    return 1;
  }

  if (command_line(argc, argv) != 1) {
    return 0;
  }

  hostname = config_get_string("/host");
  port = config_get_short("/port");

  return event_startup(hostname, port);
}
