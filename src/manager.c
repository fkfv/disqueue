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

#include <json-c/json_object.h>
#include <json-c/json_tokener.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <stdio.h>
#include <stdlib.h>

#include "manager.h"
#include "manager-internal.h"
#include "hostnet.h"
#include "strcase.h"
#include "protocol.h"

struct manager_context manager_context_;

void manager_send_http(struct evhttp_request* request, int code, const char* message, size_t len)
{
  struct evbuffer* outbuffer;
  struct evkeyvalq *headers;

  if (len == 0) {
    len = strlen(message);
  }

  headers = evhttp_request_get_output_headers(request);
  evhttp_add_header(headers, "Content-Type", "application/json");

  outbuffer = evhttp_request_get_output_buffer(request);
  evbuffer_add(outbuffer, message, len);
  evhttp_send_reply(request, code, NULL, NULL);
}

void manager_send_json(struct evhttp_request* request, int code, struct json_object* json)
{
  const char *encoded;
  size_t encoded_length;

  encoded = json_object_to_json_string_length(json, JSON_C_TO_STRING_PLAIN,
                                              &encoded_length);
  if (!encoded) {
    manager_send_http(request, HTTP_INTERNAL, protocol_failure_fallback(), 0);
    return;
  }

  manager_send_http(request, code, encoded, encoded_length);
  json_object_put(json);
}

void manager_send_http_error(struct evhttp_request *request, int code, const char *error)
{
  struct json_object *err;

  if (code == 0) {
    code = HTTP_INTERNAL;
  }

  err = protocol_create_failure(error);
  if (!err) {
    manager_send_http(request, HTTP_INTERNAL, protocol_failure_fallback(), 0);
    return;
  }

  manager_send_json(request, code, err);
}

void manager_ws_send(struct evws_connection* conn, struct json_object* obj)
{
  const char* encoded;

  encoded = json_object_to_json_string_ext(obj, 0/*PLAIN*/);
  if (encoded) {
    evws_connection_send(conn, encoded);
  } else {
    evws_connection_send(conn, protocol_failure_fallback());
  }

  json_object_put(obj);
}

void manager_send_ws_error(struct evws_connection* connection,
                           const char* error)
{
  struct json_object* err;

  err = protocol_create_failure(error);
  if (!err) {
    evws_connection_send(connection, protocol_failure_fallback());
    return;
  }

  manager_ws_send(connection, err);
}

void manager_ws_error(struct manager_queue_want *want)
{
  manager_send_ws_error(want->client, "failed to get item");
}

void manager_send_payload_http(struct evhttp_request* request,
                               struct json_object* obj)
{
  struct json_object* wrapper;

  wrapper = protocol_create_success(obj);
  if (!wrapper) {
    json_object_put(obj);
    manager_send_http_error(request, 0, "failed to create payload");
    return;
  }

  manager_send_json(request, HTTP_OK, wrapper);
}

int manager_read_post(struct evhttp_request *request, struct evkeyvalq *params)
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

  manager_send_http_error(request, 0, "failed to read post body");
  return 0;
}

struct json_object *manager_encode_item(struct queue_item *item)
{
  struct json_object *base = NULL;
  struct json_object *key = NULL;
  struct json_object *value = NULL;
  const char *item_key;

  base = json_object_new_object();
  if (!base) {
    goto error;
  }

  item_key = queue_item_get_key(item);
  if (item_key) {
    key = json_object_new_string(item_key);
    if (!key) {
      goto error;
    }
  } else {
    key = NULL;
  }

  if (json_object_object_add_ex(base, "key", key,
                                JSON_C_OBJECT_ADD_KEY_IS_NEW) != 0) {
      goto error;
  }
  key = NULL;

  value = json_object_new_string(queue_item_get_value(item));
  if (!value || json_object_object_add_ex(base, "value", value,
                                          JSON_C_OBJECT_ADD_KEY_IS_NEW) != 0) {
    goto error;
  }

  return base;

error:
  if (base) {
    json_object_put(base);
  }

  if (value) {
    json_object_put(value);
  }

  if (key) {
    json_object_put(key);
  }

  return NULL;
}

struct json_object *manager_parse_ws(struct evws_message *message)
{
  struct evbuffer *inbuffer;
  char *body;
  size_t bodylength;
  struct json_object *obj = NULL;

  inbuffer = evws_message_get_buffer(message);
  bodylength = evbuffer_get_length(inbuffer);
  body = calloc(1, bodylength + 1/*NULL*/);
  if (!body || evbuffer_copyout(inbuffer, body, bodylength) != bodylength) {
    goto done;
  }

  obj = json_tokener_parse(body);

done:
  if (body) {
    free(body);
  }
  
  return obj;
}

void manager_startup(struct evhttp *http, struct evws *ws)
{
  manager_context_.http = http;
  manager_context_.ws = ws;

  TAILQ_INIT(&manager_context_.queues);
  TAILQ_INIT(&manager_context_.wants);

  /* create http callbacks */
  evhttp_set_cb(http, "/queues", manager_queues_cb_, NULL);
  evhttp_set_cb(http, "/queue", manager_queue_cb_, NULL);
  evhttp_set_cb(http, "/take", manager_take_item_cb_, NULL);
  evhttp_set_cb(http, "/peek", manager_peek_item_cb_, NULL);
  evhttp_set_cb(http, "/put", manager_put_item_cb_, NULL);

  /* create ws callbacks */
  evws_bind_path(ws, "/take/ws");
  evws_set_cb(ws, manager_wait_item_cb_, NULL);
  evws_set_close_cb(ws, manager_wait_close_cb_, NULL);
}

void manager_shutdown(void)
{
  evhttp_del_cb(manager_context_.http, "/queues");
  evhttp_del_cb(manager_context_.http, "/queue");
  evhttp_del_cb(manager_context_.http, "/take");
  evhttp_del_cb(manager_context_.http, "/peek");
  evhttp_del_cb(manager_context_.http, "/put");

  evws_unbind_path(manager_context_.ws, "/take/ws");

  /* remove wants */
  /* remove queues */
}

void manager_queues_cb_(struct evhttp_request *request, void *user)
{
  switch (evhttp_request_get_command(request)) {
  case EVHTTP_REQ_GET:
    manager_queues_list_cb_(request, user);
    break;
  case EVHTTP_REQ_POST:
    manager_queues_new_cb_(request, user);
    break;
  default:
    manager_send_http_error(request, HTTP_BADMETHOD, "method not supported");
    break;
  }
}

void manager_queues_list_cb_(struct evhttp_request *request, void *user)
{
  struct manager_queue *q;
  struct json_object *queue_list = NULL;
  struct json_object *stringobject = NULL;

  queue_list = json_object_new_array();
  if (!queue_list) {
    goto error;
  }

  TAILQ_FOREACH(q, &manager_context_.queues, next) {
    stringobject = json_object_new_string(q->id);
    if (!stringobject || json_object_array_add(queue_list, stringobject) != 0) {
      goto error;
    }
  }

  manager_send_payload_http(request, queue_list);
  return;

error:
  if (stringobject) {
    json_object_put(stringobject);
  }

  if (queue_list) {
    json_object_put(queue_list);
  }

  manager_send_http_error(request, 0, "failed to list queues");
}

void manager_queues_new_cb_(struct evhttp_request *request, void *user)
{
  struct evkeyvalq params = {0};
  struct manager_queue *q = NULL;
  struct json_object *name;

  if (manager_read_post(request, &params) != 1 ||
      manager_validate_(request, &q, 1, &params) != 1) {
    goto error;
  }

  name = json_object_new_string(q->id);
  if (!name) {
    goto error;
  }

  evhttp_clear_headers(&params);
  manager_send_payload_http(request, name);
  return;

error:
  evhttp_clear_headers(&params);

  if (q) {
    manager_queue_free_(q);
  }

  manager_send_http_error(request, 0, "failed to create queue");
}

void manager_queue_cb_(struct evhttp_request *request, void *user)
{
  switch (evhttp_request_get_command(request)) {
  case EVHTTP_REQ_POST:
    manager_queue_info_cb_(request, user);
    break;
  case EVHTTP_REQ_DELETE:
    manager_queue_delete_cb_(request, user);
    break;
  default:
    manager_send_http_error(request, HTTP_BADMETHOD, "method not supported");
    break;
  }
}

void manager_queue_info_cb_(struct evhttp_request *request, void *user)
{
  struct evkeyvalq params = {0};
  struct manager_queue *q;
  struct json_object *info = NULL;
  struct json_object *name = NULL;

  if (manager_read_post(request, &params) != 1 ||
      manager_validate_(request, &q, 0, &params) != 1) {
    goto error;
  }

  info = json_object_new_object();
  if (!info) {
    goto error;
  }

  name = json_object_new_string(q->id);
  if (!name) {
    goto error;
  }

  if (json_object_object_add_ex(info, "name", name,
                                JSON_C_OBJECT_ADD_KEY_IS_NEW |
                                JSON_C_OBJECT_KEY_IS_CONSTANT) != 0) {
    goto error;
  }

  evhttp_clear_headers(&params);
  manager_send_payload_http(request, info);
  return;

error:
  evhttp_clear_headers(&params);

  if (info) {
    json_object_put(info);
  }

  if (name) {
    json_object_put(name);
  }

  manager_send_http_error(request, 0, "failed to get queue info");
}

void manager_queue_delete_cb_(struct evhttp_request *request, void *user)
{
  struct evkeyvalq params = {0};
  struct manager_queue *q;
  struct manager_queue_want *want;
  struct manager_queue_want *next;
  struct json_object *status;

  if (manager_read_post(request, &params) != 1 ||
      manager_validate_(request, &q, 0, &params) != 1) {
    goto error;
  }

  status = json_object_new_boolean(1);
  if (!status) {
    goto error;
  }

  /* remove the queue */
  manager_queue_free_(q);

  /* remove any wants that where waiting on this queue */
  want = TAILQ_FIRST(&manager_context_.wants);
  while (want != NULL) {
    next = TAILQ_NEXT(want, next);
    if (want->queue == q) {
      TAILQ_REMOVE(&manager_context_.wants, want, next);
      manager_queue_want_free_(want);
    }

    want = next;
  }

  evhttp_clear_headers(&params);
  manager_send_payload_http(request, status);
  return;

error:
  evhttp_clear_headers(&params);

  manager_send_http_error(request, 0, "failed to delete queue");
}

void manager_take_item_cb_(struct evhttp_request *request, void *user)
{
  struct evkeyvalq params = {0};
  struct queue_item *item = NULL;
  struct manager_queue *q;
  struct json_object *encoded;
  const char *key;

  if (manager_read_post(request, &params) != 1 ||
      manager_validate_(request, &q, 0, &params) != 1) {
    goto error;
  }

  key = evhttp_find_header(&params, "key");
  item = queue_take(q->q, key);
  if (!item) {
    evhttp_clear_headers(&params);
    manager_send_payload_http(request, NULL);
    return;
  }

  encoded = manager_encode_item(item);
  if (!encoded) {
    goto error;
  }

  queue_item_free(item);
  evhttp_clear_headers(request);
  manager_send_payload_http(request, encoded);
  return;

error:
  evhttp_clear_headers(&params);

  if (item) {
    queue_item_free(item);
  }

  manager_send_http_error(request, 0, "failed to take item");
}

void manager_peek_item_cb_(struct evhttp_request *request, void *user)
{
  struct evkeyvalq params = {0};
  struct queue_item *item;
  struct manager_queue *q;
  struct json_object *encoded;
  int locked = 0;
  const char *key;

  if (manager_read_post(request, &params) != 1 ||
      manager_validate_(request, &q, 0, &params) != 1) {
    goto error;
  }

  key = evhttp_find_header(&params, "key");
  item = queue_peek(q->q, key);
  if (!item) {
    evhttp_clear_headers(&params);
    manager_send_payload_http(request, NULL);
    return;
  }
  queue_item_lock(item);
  locked = 1;

  encoded = manager_encode_item(item);
  if (!encoded) {
    goto error;
  }

  queue_item_unlock(item);
  manager_send_payload_http(request, encoded);
  return;

error:
  evhttp_clear_headers(&params);

  if (locked) {
    queue_item_unlock(item);
  }

  manager_send_http_error(request, 0, "failed to peek item");
}

/* TODO: rework */
void manager_wait_item_cb_(struct evws_message *message, void *user)
{
  json_object *request;
  json_object *value;
  const char *identifier;
  const char *key;
  const char *queue;
  struct manager_queue_want *want;
  struct manager_queue *q;

  request = manager_parse_ws(message);
  if (!request) {
    return;
  }

  if (!json_object_object_get_ex(request, "queue", &value)) {
    goto free;
  }

  queue = json_object_get_string(value);
  if (!queue) {
    goto free;
  }

  q = manager_get_queue_(queue, 0);
  if (!q) {
    goto free;
  }

  if (!json_object_object_get_ex(request, "identifier", &value)) {
    goto free;
  }

  identifier = json_object_get_string(value);
  if (!identifier) {
    goto free;
  }

  if (!json_object_object_get_ex(request, "key", &value)) {
    key = NULL;
  } else {
    key = json_object_get_string(value);
    if (!key) {
      goto free;
    }
  }

  want = manager_queue_want_new_(identifier,
                                 evws_message_get_connection(message), q);
  if (!want) {
    goto free;
  }

  if (queue_wait(q->q, key, manager_wait_receive_cb_, want) < 0) {
    manager_queue_want_free_(want);
  }

free:
  json_object_put(request);
}

void manager_wait_close_cb_(struct evws_connection *connection, void *user)
{
  struct manager_queue_want *want;

  /* cancel any wants for the disconnected client - when the callback finishes
     the want will clean itself up */
  TAILQ_FOREACH(want, &manager_context_.wants, next) {
    if (want->client == connection) {
      want->cancelled = 1;
    }
  }
}

void manager_put_item_cb_(struct evhttp_request *request, void *user)
{
  struct evkeyvalq params = {0};
  struct manager_queue *q;
  struct json_object *status;
  const char *key;
  const char *value;

  if (manager_read_post(request, &params) != 1 || 
      manager_validate_(request, &q, 0, &params) != 1) {
    goto error;
  }

  status = json_object_new_boolean(1);
  if (!status) {
    goto error;
  }

  key = evhttp_find_header(&params, "key");
  value = evhttp_find_header(&params, "value");
  if (queue_put(q->q, key, value) < 0) {
    goto error;
  }

  evhttp_clear_headers(&params);
  manager_send_payload_http(request, status);
  return;

error:
  evhttp_clear_headers(&params);

  manager_send_http_error(request, 0, "failed to put item");
}

void manager_wait_receive_cb_(struct queue_item *item, void *user)
{
  struct manager_queue_want *want = (struct manager_queue_want *)user;
  struct json_object *response;
  struct json_object *id;
  struct json_object *encoded;

  if (want->cancelled) {
    manager_queue_want_free_(want);
    return;
  }

  response = json_object_new_object();
  if (!response) {
    manager_ws_error(want);
    return;
  }

  id = json_object_new_string(want->identifier);
  if (!id) {
    json_object_put(response);
    manager_ws_error(want);
    return;
  }

  if (json_object_object_add_ex(response, "id", id,
                                JSON_C_OBJECT_ADD_KEY_IS_NEW |
                                JSON_C_OBJECT_KEY_IS_CONSTANT) != 0) {
    json_object_put(response);
    manager_ws_error(want);
    return;
  }

  encoded = manager_encode_item(item);
  if (!encoded) {
    json_object_put(response);
    manager_ws_error(want);
    return;
  }

  if (json_object_object_add_ex(response, "item", encoded,
                                JSON_C_OBJECT_ADD_KEY_IS_NEW |
                                JSON_C_OBJECT_KEY_IS_CONSTANT) != 0) {
    json_object_put(encoded);
    json_object_put(response);
    manager_ws_error(want);
    return;
  }

  manager_ws_send(want->client, response);
}

int manager_validate_(struct evhttp_request *request,
                      struct manager_queue **q, int create_new,
                      struct evkeyvalq *params)
{
  char *name;

  /* get the name of the new queue */
  name = evhttp_find_header(params, "name");

  /* verify the name - it must be a uuid */
  if (name && strlen(name) != QUEUE_UUID_STR_LEN) {
    manager_send_http_error(request, HTTP_BADREQUEST, "invalid queue id");
    return 0;
  }

  *q = manager_get_queue_(name, create_new);
  if (!*q) {
    manager_send_http_error(request, HTTP_NOTFOUND, "queue does not exist");
    return 0;
  }

  return 1;
}

struct manager_queue *manager_get_queue_(const char name[QUEUE_UUID_STR_LEN],
                                         int create_new)
{
  unsigned char r[QUEUE_UUID_LEN];
  unsigned char *used_r = r;
  struct manager_queue *q;

  if (name) {
    /* always fail if an invalid ID was given */
    if (strlen(name) != QUEUE_UUID_STR_LEN) {
      return NULL;
    }

    /* see if an existing queue is found */
    TAILQ_FOREACH(q, &manager_context_.queues, next) {
      if (!strcasecmp(q->id, name)) {
        return q;
      }
    }

    /* name wasn't found and not creating new - fail */
    if (!create_new) {
      return NULL;
    }

    /* the name will be in string format, it needs to be converted into byte
     format. since some of the bytes are read as integers, they need to be
     swapped to network byte order */
    if (sscanf(name, "%8llx-%4lx-%4lx-%4lx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
               (unsigned long long *)&r[0],
               (unsigned long *)&r[4],
               (unsigned long *)&r[6],
               (unsigned long *)&r[8],
               &r[10],
               &r[11],
               &r[12],
               &r[13],
               &r[14],
               &r[15]) != 10) {
      return NULL;
    }

    *(unsigned long *)&r[0] = htonl(*(unsigned long *)&r[0]);
    *(unsigned short *)&r[4] = htons(*(unsigned short *)&r[4]);
    *(unsigned short *)&r[6] = htons(*(unsigned short *)&r[6]);
    *(unsigned short *)&r[8] = htons(*(unsigned short *)&r[8]);
  } else {
    /* no name and create new not specified - always fails */
    if (!create_new) {
      return NULL;
    }

    /* set r null so a random id is generated for this queue */
    used_r = NULL;
  }

  /* try to create a new manager queue */
  q = calloc(1, sizeof(struct manager_queue));
  if (!q) {
    return NULL;
  }

  /* create the queue object */
  q->q = queue_new(used_r);
  if (!q->q) {
    free(q);
    return NULL;
  }

  /* cache the id of the new queue */
  queue_get_uuid(q->q, q->id);

  TAILQ_INSERT_TAIL(&manager_context_.queues, q, next);
  return q;
}

void manager_queue_free_(struct manager_queue *queue)
{
  TAILQ_REMOVE(&manager_context_.queues, queue, next);
  queue_free(queue->q);
  free(queue);
}

struct manager_queue_want *manager_queue_want_new_(const char *id,
                                                   struct evws_connection *con,
                                                   struct manager_queue *queue)
{
  struct manager_queue_want *want;

  want = calloc(1, sizeof(struct manager_queue_want));
  if (!want) {
    return NULL;
  }

  want->identifier = strdup(id);
  if (!want->identifier) {
    free(want);
    return NULL;
  }

  want->client = con;
  want->queue = queue;

  TAILQ_INSERT_TAIL(&manager_context_.wants, want, next);

  return want;
}

void manager_queue_want_free_(struct manager_queue_want *want)
{
  TAILQ_REMOVE(&manager_context_.wants, want, next);
  free(want->identifier);
  free(want);
}
