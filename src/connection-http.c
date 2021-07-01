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

#include <event2/buffer.h>
#include "connection.h"
#include "connection-internal.h"
#include "protocol.h"

/* callback for the queue list operation, adds the name of each queue to the
   list */
int list_foreach_callback(struct manager_queue *queue, void *user)
{
  struct json_object *list = (json_object *)user;
  struct json_object *str;

  str = json_object_new_string(manager_queue_get_id(queue));
  if (!str) {
    return 0;
  }

  if (json_object_array_add(list, str) != 0) {
    json_object_put(str);
    return 0;
  }

  return 1;
}

void connection_http_callback_queues(struct evhttp_request *request,
                                     void *user)
{
  switch (evhttp_request_get_command(request)) {
  case EVHTTP_REQ_GET:
    connection_http_callback_list_(request, NULL);
    break;
  case EVHTTP_REQ_POST:
    connection_http_callback_new_(request, NULL);
    break;
  default:
    connection_http_error_(request, NULL, HTTP_BADMETHOD,
                           "method not supported");
    break;
  }
}

void connection_http_callback_queue(struct evhttp_request *request,
                                    void *user)
{
  switch (evhttp_request_get_command(request)) {
  case EVHTTP_REQ_POST:
    connection_http_callback_info_(request, NULL);
    break;
  case EVHTTP_REQ_DELETE:
    connection_http_callback_delete_(request, NULL);
    break;
  default:
    connection_http_error_(request, NULL, HTTP_BADMETHOD,
                           "method not supported");
    break;
  }
}

void connection_http_callback_take(struct evhttp_request *request,
                                   void *user)
{
  struct evkeyvalq params = {0};
  struct queue_item *item;
  struct json_object *object;
  struct manager_queue *queue;
  const char *key;

  if (connection_http_read_(request, &params) != 1 ||
      (queue = connection_http_validate_(request, 0, &params)) == NULL) {
    return;
  }

  key = evhttp_find_header(&params, "key");
  item = queue_take(manager_queue_get_queue(queue), key);
  if (!item) {
    connection_http_error_(request, &params, HTTP_NOTFOUND, "no item to take");
    return;
  }

  object = protocol_encode_item(item);
  if (!object) {
    connection_http_error_(request, &params, 0, "failed to encode item");
    goto cleanup;
  }

  connection_http_payload_(request, &params, object);

cleanup:
  if (item) {
    queue_item_free(item);
  }
}

void connection_http_callback_peek(struct evhttp_request *request,
                                   void *user)
{
  struct evkeyvalq params = {0};
  struct queue_item *item;
  struct json_object *object;
  struct manager_queue *queue;
  const char *key;

  if (connection_http_read_(request, &params) != 1 ||
      (queue = connection_http_validate_(request, 0, &params)) == NULL) {
    return;
  }

  key = evhttp_find_header(&params, "key");
  item = queue_peek(manager_queue_get_queue(queue), key);
  if (!item) {
    connection_http_error_(request, &params, HTTP_NOTFOUND, "no item to peek");
    return;
  }

  queue_item_lock(item);

  object = protocol_encode_item(item);
  if (!object) {
    connection_http_error_(request, &params, 0, "failed to encode item");
    goto cleanup;
  }

  connection_http_payload_(request, &params, object);

cleanup:
  if (item) {
    queue_item_unlock(item);
  }
}

void connection_http_callback_put(struct evhttp_request *request,
                                  void *user)
{
  struct evkeyvalq params = {0};
  struct manager_queue *queue;
  const char *key;
  const char *value;

  if (connection_http_read_(request, &params) != 1 ||
      (queue = connection_http_validate_(request, 0, &params)) == NULL) {
    return;
  }

  key = evhttp_find_header(&params, "key");
  value = evhttp_find_header(&params, "value");
  if (!value) {
    connection_http_error_(request, &params, HTTP_BADREQUEST,
                           "missing parameter 'value'");
    return;
  }

  if (queue_put(manager_queue_get_queue(queue), key, value) < 0) {
    connection_http_error_(request, &params, 0, "failed to put item");
    return;
  }

  connection_http_payload_(request, &params, NULL);
}

struct manager_queue *connection_http_validate_(struct evhttp_request *request,
                                                int create_new,
                                                struct evkeyvalq *params)
{
  const char *name;
  struct manager_queue *queue;

  name = evhttp_find_header(params, "name");
  if (name && strlen(name) != QUEUE_UUID_STR_LEN) {
    connection_http_error_(request, params, HTTP_BADREQUEST,
                           "invalid queue id");
    return NULL;
  }

  queue = manager_queue_get(name, create_new);
  if (!queue) {
    if (create_new) {
      connection_http_error_(request, params, 0, "failed to create new queue");
    } else {
      connection_http_error_(request, params, HTTP_NOTFOUND,
                             "queue does not exist");
    }
    return NULL;
  }

  return queue;
}

int connection_http_read_(struct evhttp_request *request,
                          struct evkeyvalq *params)
{
  struct evbuffer *inbuffer;
  char *body;
  size_t bodylength;

  inbuffer = evhttp_request_get_input_buffer(request);
  bodylength = evbuffer_get_length(inbuffer);
  body = calloc(1, bodylength + 1/*NULL*/);
  if (!body) {
    goto error;
  }

  if (evbuffer_copyout(inbuffer, body, bodylength) != bodylength) {
    goto error;
  }

  if (evhttp_parse_query_str(body, params) != 0) {
    goto error;
  }

  free(body);
  return 1;

error:
  if (body) {
    free(body);
  }

  connection_http_error_(request, params, 0, "failed to read post body");
  return 0;
}

void connection_http_json_(struct evhttp_request *request,
                           struct evkeyvalq *params,
                           int code, struct json_object *object)
{
  const char *repr;
  size_t length;
  struct evbuffer *buffer;
  struct evkeyvalq *headers;

  /* we cannot have a null object - it should always be wrapped in a payload
     so a null object means payload encoding failed or encoding the error
     failed so we can use the fallback string */
  repr = json_object_to_json_string_length(object, JSON_C_TO_STRING_PLAIN,
                                           &length);
  if (!repr || !object) {
    code = HTTP_INTERNAL;
    repr = protocol_failure_fallback();
    length = strlen(repr);
  }

  headers = evhttp_request_get_output_headers(request);
  buffer = evhttp_request_get_output_buffer(request);

  /* nothing can be done if these fail, so no point checking */
  evhttp_add_header(headers, "Content-Type", "application/json");
  evbuffer_add(buffer, repr, length);
  evhttp_send_reply(request, code, NULL, NULL);

  if (object) {
    json_object_put(object);
  }

  if (params) {
    evhttp_clear_headers(params);
  }
}

void connection_http_error_(struct evhttp_request *request,
                            struct evkeyvalq *params,
                            int code, const char *message)
{
  if (code == 0) {
    code = HTTP_INTERNAL;
  }

  connection_http_json_(request, params, code,
                        protocol_create_failure(message));
}

void connection_http_payload_(struct evhttp_request *request,
                              struct evkeyvalq *params,
                              struct json_object *payload)
{
  struct json_object *object;

  object = protocol_create_success(payload);
  if (!object) {
    json_object_put(payload);
    connection_http_error_(request, params, 0, "failed to encode payload");
    return;
  }

  connection_http_json_(request, params, HTTP_OK, object);
}

void connection_http_callback_list_(struct evhttp_request *request,
                                    void *user)
{
  struct json_object *list;
  struct evkeyvalq params = {0};

  if (connection_http_read_(request, &params) != 1) {
    return;
  }

  list = json_object_new_array();
  if (!list) {
    connection_http_error_(request, &params, 0, "failed to create list");
    return;
  }

  if (manager_queue_foreach(list_foreach_callback, list) != 1) {
    json_object_put(list);
    connection_http_error_(request, &params, 0, "failed to list queues");
    return;
  }

  connection_http_payload_(request, &params, list);
}

void connection_http_callback_new_(struct evhttp_request *request,
                                   void *user)
{
  struct json_object *name;
  struct evkeyvalq params = {0};
  struct manager_queue *queue;

  if (connection_http_read_(request, &params) != 1 ||
      (queue = connection_http_validate_(request, 1, &params)) == NULL) {
    return;
  }

  name = json_object_new_string(manager_queue_get_id(queue));
  if (!name) {
    manager_queue_free(queue);
    connection_http_error_(request, &params, 0, "failed to encode queue name");
    return;
  }

  connection_http_payload_(request, &params, name);
}

void connection_http_callback_info_(struct evhttp_request *request,
                                    void *user)
{
  struct json_object *detail = NULL;
  struct json_object *info = NULL;
  struct evkeyvalq params = {0};
  struct manager_queue *queue;

  if (connection_http_read_(request, &params) != 1 ||
      (queue = connection_http_validate_(request, 0, &params)) == NULL) {
    return;
  }

  detail = json_object_new_string(manager_queue_get_id(queue));
  if (!detail) {
    goto cleanup;
  }

  info = json_object_new_object();
  if (!info) {
    goto cleanup;
  }

  if (json_object_object_add_ex(info, "name", detail,
                                JSON_C_OBJECT_ADD_KEY_IS_NEW |
                                JSON_C_OBJECT_KEY_IS_CONSTANT) != 0) {
    goto cleanup;
  }

  connection_http_payload_(request, &params, info);
  return;

cleanup:
  if (info) {
    json_object_put(info);
  }

  if (detail) {
    json_object_put(detail);
  }

  connection_http_error_(request, &params, 0, "failed to encode queue info");
}

void connection_http_callback_delete_(struct evhttp_request *request,
                                      void *user)
{
  struct evkeyvalq params = {0};
  struct manager_queue *queue;

  if (connection_http_read_(request, &params) != 1 ||
      (queue = connection_http_validate_(request, 1, &params)) == NULL) {
    return;
  }

  manager_queue_want_remove(queue);
  manager_queue_free(queue);

  connection_http_payload_(request, &params, NULL);
}
