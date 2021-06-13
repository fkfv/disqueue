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

#include <signal.h>

#ifndef _WIN32
#include <netinet/in.h>
#ifdef _XOPEN_SOURCE_EXTENDED
#include <arpa/inet.h>
#endif
#include <sys/socket.h>
#endif

#include <stdio.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

void listener_cb(struct evconnlistener *, evutil_socket_t, struct sockaddr *,
                 int, void *);
void conn_writecb(struct bufferevent *, void *);
void conn_eventcb(struct bufferevent *, short, void *);
void signal_cb(evutil_socket_t, short, void *);

const char MESSAGE[] = "Hello, world!\n";

int main(int argc, const char *argv[])
{
  struct event_base *base;
  struct evconnlistener *listener;
  struct event *signal_event;
  struct sockaddr_in sin;
#ifdef _WIN32
  WSADATA wsa_data;

  WSAStartup(0x0201, &wsa_data);
#endif

  base = event_base_new();
  if (!base) {
    fprintf(stderr, "Could not initialise libevent\n");
    return 1;
  }

  sin.sin_family = AF_INET;
  sin.sin_port = htons(5543);

  listener = evconnlistener_new_bind(base, listener_cb, (void *)base,
                                     LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,
                                     -1, (struct sockaddr *)&sin, sizeof(sin));
  if (!listener) {
    fprintf(stderr, "Could not create listener\n");
    return 1;
  }

  signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);
  if (!signal_event || event_add(signal_event, NULL) < 0) {
    fprintf(stderr, "Could not create/add signal event\n");
    return 1;
  }

  event_base_dispatch(base);

  evconnlistener_free(listener);
  event_free(signal_event);
  event_base_free(base);

#ifdef _WIN32
  WSACleanup();
#endif

  return 0;
}

void listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
                 struct sockaddr *sa, int socklen, void *user_data)
{
  struct bufferevent *bev;
  struct event_base *base = user_data;

  bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
  if (!bev) {
    fprintf(stderr, "Error creating bufferevent\n");
    event_base_loopbreak(base);
    return;
  }

  bufferevent_setcb(bev, NULL, conn_writecb, conn_eventcb, NULL);
  bufferevent_enable(bev, EV_WRITE);
  bufferevent_disable(bev, EV_READ);

  bufferevent_write(bev, MESSAGE, strlen(MESSAGE));
}

void conn_writecb(struct bufferevent *bev, void *user_data)
{
  struct evbuffer *output;

  output = bufferevent_get_output(bev);
  if (evbuffer_get_length(output) == 0) {
    printf("flushed answer\n");
    bufferevent_free(bev);
  }
}

void conn_eventcb(struct bufferevent *bev, short events, void *user_data)
{
  if (events & BEV_EVENT_EOF) {
    printf("Connection closed\n");
  } else if (events & BEV_EVENT_ERROR) {
    printf("Connection error: %s\n", strerror(errno));
  }

  bufferevent_free(bev);
}

void signal_cb(evutil_socket_t sig, short events, void *user_data)
{
  struct event_base *base = user_data;
  struct timeval delay = {0, 0};

  printf("Interrupt signal, waiting for resources to close and exiting...\n");
  event_base_loopexit(base, &delay);
}
