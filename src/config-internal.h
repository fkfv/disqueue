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

#ifndef CONFIG_INTERNAL_H
#define CONFIG_INTERNAL_H

#include <json-c/json_object.h>
#include "queue-compat.h"
#include "auth.h"

struct config_authentication {
  LIST_ENTRY(config_authentication) next;

  /* name of authentication provider */
  const char *name;

  /* type of authentication and file store */
  const char *type;
  const char *file;

  /* auth instance */
  struct auth *auth;
};

struct config_server {
  LIST_ENTRY(config_server) next;

  const char *hostname;
  unsigned short port;

  const char *certificate;
  const char *privatekey;

  /* authentication used by this server */
  struct config_authentication *authentication;
};

struct config_context {
  LIST_HEAD(cshead, config_server) servers;
  LIST_HEAD(cahead, config_authentication) authentications;

  /* the object backing the string values used in each server. releasing this
     object will invalidate the loaded configuration. */
  struct json_object *object;

  /* current iteration item */
  struct config_server *current_iter;
};

int config_process_server_(struct json_object *server);
int config_process_authentication_(const char *name,
                                   struct json_object *config);

struct config_authentication *config_authentication_get_(const char *name);

extern struct config_context global_config_context_;

#endif
