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

#ifndef CONFIG_H
#define CONFIG_H

#include "auth.h"

struct config_server;
struct config_authentication;

int config_load_file(const char *filename);
void config_free(void);

/* iterate config_server instances. iter_server_begin should be called to start
   iteration, iter_server_next will return config_server and null when the last
   one has been consumed, then iter_server_close will finish */
int config_iter_server_begin(void);
void config_iter_server_close(void);
struct config_server *config_iter_server_next(void);

/* get properties from a config_server */
const char *config_server_get_hostname(struct config_server *server);
unsigned short config_server_get_port(struct config_server *server);
int config_server_has_security(struct config_server *server);
const char *config_server_get_certificate(struct config_server *server);
const char *config_server_get_privatekey(struct config_server *server);
struct auth *config_server_get_authentication(struct config_server *server);
const char *config_server_get_realm(struct config_server *server);

#endif
