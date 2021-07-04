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

#include <json-c/json_util.h>
#include <json-c/json_pointer.h>
#include <json-c/linkhash.h>
#include <limits.h>
#include <stdlib.h>
#include "config.h"
#include "config-internal.h"
#include "strcase.h"

struct config_context global_config_context_ = {0};

/* likely size that the configuration would fit in. must be less than INT_MAX
   even though fread returns size_t */
#define CONFIG_BUF_SIZE 4096

int config_load_file(const char *filename)
{
  struct json_object *servers;
  struct json_object *server;
  struct json_object *authentications;
  size_t server_count;
  size_t index;

  global_config_context_.object = json_object_from_file(filename);
  if (!global_config_context_.object) {
    return 0;
  }

  authentications = json_object_object_get(global_config_context_.object,
                                           "authentication");
  if (authentications) {
    json_object_object_foreach(authentications, key, value) {
      if (!config_process_authentication_(key, value)) {
        return 0;
      }
    }
  }

  servers = json_object_object_get(global_config_context_.object, "servers");
  if (!servers || json_object_get_type(servers) != json_type_array) {
    return 0;
  }

  server_count = json_object_array_length(servers);
  for (index = 0; index < server_count; index++) {
    server = json_object_array_get_idx(servers, index);
    if (!server || !config_process_server_(server)) {
      return 0;
    }
  }

  return 1;
}

void config_free(void)
{
  json_object_put(global_config_context_.object);
}

int config_iter_server_begin(void)
{
  if (global_config_context_.current_iter) {
    return 0;
  }

  global_config_context_.current_iter =
    LIST_FIRST(&global_config_context_.servers);
  return global_config_context_.current_iter != NULL;
}

void config_iter_server_close(void)
{
  global_config_context_.current_iter = NULL;
}

struct config_server *config_iter_server_next(void)
{
  struct config_server *iter = global_config_context_.current_iter;

  if (iter == NULL) {
    return NULL;
  }

  global_config_context_.current_iter =
    LIST_NEXT(global_config_context_.current_iter, next);
  return iter;
}

const char *config_server_get_hostname(struct config_server *server)
{
  return server->hostname;
}

unsigned short config_server_get_port(struct config_server *server)
{
  return server->port;
}

int config_server_has_security(struct config_server *server)
{
  return server->certificate && server->privatekey;
}

const char *config_server_get_certificate(struct config_server *server)
{
  return server->certificate;
}

const char *config_server_get_privatekey(struct config_server *server)
{
  return server->privatekey;
}

struct auth *config_server_get_authentication(struct config_server *server)
{
  return server->authentication->auth;
}

int config_process_server_(struct json_object *config)
{
  struct json_object *obj;
  struct config_server *server;
  const char *authentication;
  int port;

  server = calloc(1, sizeof(struct config_server));
  if (!server) {
    goto error;
  }

  if (json_pointer_get(config, "/hostname", &obj) ||
      !(server->hostname = json_object_get_string(obj))) {
    goto error;
  }

  if (json_pointer_get(config, "/port", &obj) ||
      !(port = json_object_get_int(obj))) {
    goto error;
  }

  if (port <= 0 || port > SHRT_MAX) {
    goto error;
  }
  server->port = (unsigned short)port;

  if (!json_pointer_get(config, "/security/certificate", &obj)) {
    /* check that it is possible the certificate may point to a file if
       defined */
    server->certificate = json_object_get_string(obj);
    if (!server->certificate) {
      goto error;
    }

    /* only try and get the privatekey if we had a certificate, and fail if the
       private key is not also present */
    if (json_pointer_get(config, "/security/privatekey", &obj) ||
        !(server->privatekey = json_object_get_string(obj))) {
      goto error;
    }
  }

  if (!json_pointer_get(config, "/authentication", &obj)) {
    authentication = json_object_get_string(obj);
    if (!authentication) {
      goto error;
    }

    /* check that the authentication exists */
    server->authentication = config_authentication_get_(authentication);
    if (!server->authentication) {
      goto error;
    }
  }

  LIST_INSERT_HEAD(&global_config_context_.servers, server, next);
  return 1;

error:
  if (server) {
    free(server);
  }

  return 0;
}

int config_process_authentication_(const char *name,
                                   struct json_object *config)
{
  struct json_object *obj;
  struct config_authentication *authentication;

  authentication = calloc(1, sizeof(struct config_authentication));
  if (!authentication) {
    goto error;
  }

  authentication->name = name;

  if (json_pointer_get(config, "/type", &obj) ||
      !(authentication->type = json_object_get_string(obj))) {
    goto error;
  }

  if (json_pointer_get(config, "/file", &obj) ||
      !(authentication->file = json_object_get_string(obj))) {
    goto error;
  }

  authentication->auth = auth_method_from_string(authentication->type);
  if (!authentication->auth) {
    goto error;
  }

  LIST_INSERT_HEAD(&global_config_context_.authentications, authentication,
                   next);
  return 1;

error:
  if (authentication) {
    free(authentication);
  }

  return 0;
}

struct config_authentication *config_authentication_get_(const char *name)
{
  struct config_authentication *authentication;

  LIST_FOREACH(authentication, &global_config_context_.authentications, next) {
    if (!strcasecmp(authentication->name, name)) {
      return authentication;
    }
  }

  return NULL;
}
