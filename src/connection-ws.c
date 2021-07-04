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

#include <json-c/json_tokener.h>
#include <event2/buffer.h>
#include "connection.h"
#include "connection-internal.h"
#include "manager.h"
#include "protocol.h"

/* get a string attribute from a json object. returns null if the object is
   not present */
const char *json_get_string(struct json_object *object, const char *key)
{
  struct json_object *attribute;

  if (!json_object_object_get_ex(object, key, &attribute)) {
    return NULL;
  }

  return json_object_get_string(attribute);
}

int connection_ws_authenticated(struct evhttp_request *request, void *user)
{
  struct connection_params *params = (struct connection_params *)user;
  struct evkeyvalq *headers = evhttp_request_get_input_headers(request);
  const char *authstring;

  if ((authstring = evhttp_find_header(headers, "Authorization")) == NULL) {
    connection_http_auth_required_(request, params->realm);
    connection_http_error_(request, NULL, 401/*HTTP_UNAUTHENTICATED*/,
                           "authentication required");
    return 0;
  }

  if (!auth_verify(params->auth, authstring)) {
    connection_http_error_(request, NULL, 403/*HTTP_FORBIDDEN*/,
                           "authentication failed");
    return 0;
  }

  return 1;
}

void connection_ws_callback_wait(struct evws_message *message, void *user)
{
  struct json_object *request;
  struct manager_queue *queue;
  struct manager_queue_want *want;
  const char *identifier;
  const char *queue_name;
  const char *key;

  request = connection_ws_read_(message);
  if (!request) {
    connection_ws_error_(evws_message_get_connection(message), 
                         "failed to read message");
    return;
  }

  identifier = json_get_string(request, "identifier");
  if (!identifier) {
    connection_ws_error_(evws_message_get_connection(message),
                         "no identifier");
    goto cleanup;
  }

  queue_name = json_get_string(request, "queue");
  if (!identifier) {
    connection_ws_error_(evws_message_get_connection(message),
                         "no queue");
    goto cleanup;
  }

  queue = manager_queue_get(queue_name, 0);
  if (!queue) {
    connection_ws_error_(evws_message_get_connection(message),
                         "queue not found");
    goto cleanup;
  }

  key = json_get_string(request, "key");
  want = manager_queue_want_new(identifier,
                                evws_message_get_connection(message), queue);
  if (!want) {
    connection_ws_error_(evws_message_get_connection(message),
                         "failed to create want");
    goto cleanup;
  }

  if (queue_wait(manager_queue_get_queue(queue), key,
                 connection_queue_callback_wait_, want) < 0) {
    manager_queue_want_free(want);
    connection_ws_error_(evws_message_get_connection(message),
                         "failed to wait for want");
  }

cleanup:
  json_object_put(request);
}

void connection_ws_callback_close(struct evws_connection *connection,
                                  void *user)
{
  manager_queue_want_close(connection);
}

void connection_ws_callback_error(struct evws_connection *connection,
                                  void *user)
{
  manager_queue_want_close(connection);
}

struct json_object *connection_ws_read_(struct evws_message *message)
{
  struct json_object *obj = NULL;
  struct evbuffer *buffer;
  char *body = NULL;
  size_t bodylength;

  buffer = evws_message_get_buffer(message);
  bodylength = evbuffer_get_length(buffer);
  body = calloc(1, bodylength + 1/*NULL*/);
  if (!body || evbuffer_copyout(buffer, body, bodylength) != bodylength) {
    goto done;
  }

  obj = json_tokener_parse(body);

done:
  if (body) {
    free(body);
  }
  
  return obj;
}

void connection_ws_json_(struct evws_connection *connection,
                         struct json_object *object)
{
  const char *repr;
  size_t length;

  /* we cannot have a null object - it should always be wrapped in a payload
     so a null object means payload encoding failed or encoding the error
     failed so we can use the fallback string */
  repr = json_object_to_json_string_length(object, JSON_C_TO_STRING_PLAIN,
                                           &length);
  if (!repr || !object) {
    repr = protocol_failure_fallback();
    length = strlen(repr);
  }

  evws_connection_send(connection, repr);

  if (object) {
    json_object_put(object);
  }
}

void connection_ws_error_(struct evws_connection *connection,
                          const char *message)
{
  connection_ws_json_(connection, protocol_create_failure(message));
}

void connection_ws_payload_(struct evws_connection *connection,
                            struct json_object *payload)
{
  struct json_object *object;

  object = protocol_create_success(payload);
  if (!object) {
    json_object_put(payload);
    connection_ws_error_(connection, "failed to encode payload");
    return;
  }

  connection_ws_json_(connection, object);
}

void connection_queue_callback_wait_(struct queue_item *item, void *user)
{
  struct manager_queue_want *want = (struct manager_queue_want *)user;
  struct json_object *response = NULL;
  struct json_object *detail = NULL;

  if (manager_queue_want_is_cancelled(want)) {
    goto cleanup;
  }

  response = json_object_new_object();
  if (!response) {
    connection_ws_error_(manager_queue_want_get_connection(want),
                         "failed to create response");
    goto cleanup;
  }

  detail = json_object_new_string(manager_queue_want_get_identifier(want));
  if (!detail) {
    connection_ws_error_(manager_queue_want_get_connection(want),
                         "failed to add identifier");
    goto cleanup;
  }

  if (json_object_object_add_ex(response, "id", detail,
                                JSON_C_OBJECT_ADD_KEY_IS_NEW |
                                JSON_C_OBJECT_KEY_IS_CONSTANT) != 0) {
    connection_ws_error_(manager_queue_want_get_connection(want),
                         "failed to add identifier");
    goto cleanup;
  }

  detail = protocol_encode_item(item);
  if (!detail) {
    connection_ws_error_(manager_queue_want_get_connection(want),
                         "failed to add item");
    goto cleanup;
  }

  if (json_object_object_add_ex(response, "item", detail,
                                JSON_C_OBJECT_ADD_KEY_IS_NEW |
                                JSON_C_OBJECT_KEY_IS_CONSTANT) != 0) {
    connection_ws_error_(manager_queue_want_get_connection(want),
                         "failed to add item");
    goto cleanup;
  }

  connection_ws_payload_(manager_queue_want_get_connection(want), response);
  /* both already released by sending the payload */
  response = NULL;
  detail = NULL;

cleanup:
  manager_queue_want_free(want);
  queue_item_free(item);

  if (response) {
    json_object_put(response);
  }

  if (detail) {
    json_object_put(detail);
  }
}
