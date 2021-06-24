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
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <stdio.h>
#include <stdlib.h>

#include "manager.h"
#include "manager-internal.h"
#include "hostnet.h"
#include "strcase.h"

struct manager_context manager_context_;

struct json_object *wrap_json_object(struct json_object *json, const char *name)
{
  struct json_object *wrapper;

  wrapper = json_object_new_object();
  if (!wrapper) {
    json_object_put(json);
    return NULL;
  }

  if (json_object_object_add_ex(wrapper, name, json,
                                JSON_C_OBJECT_ADD_KEY_IS_NEW) != 0) {
    json_object_put(json);
    json_object_put(wrapper);
    return NULL;
  }

  return wrapper;
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
    manager_internal_error_(request);
    return 0;
  }

  if (evbuffer_copyout(inbuffer, body, bodylength) != bodylength) {
    free(body);
    manager_internal_error_(request);
    return 0;
  }

  if (evhttp_parse_query_str(body, params) != 0) {
    free(body);
    manager_internal_error_(request);
    return 0;
  }

  free(body);
  return 1;
}

void manager_send_json(struct evhttp_request *request, const char *name,
                       struct json_object *json)
{
  struct evbuffer *outbuffer;
  const char *encoded_list;
  size_t encoded_length;

  json = wrap_json_object(json, name);
  if (!json) {
    manager_internal_error_(request);
    return;
  }

  outbuffer = evhttp_request_get_output_buffer(request);
  encoded_list = json_object_to_json_string_length(json, 0/*PLAIN*/,
                                                   &encoded_length);
  if (!encoded_list || evbuffer_add(outbuffer, encoded_list, encoded_length)) {
    json_object_put(json);
    manager_internal_error_(request);
    return;
  }

  json_object_put(json);
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
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

void manager_bad_request_cb_(struct evhttp_request *request, void *user)
{
  evhttp_send_error(request, HTTP_BADREQUEST, NULL);
}

void manager_internal_error_(struct evhttp_request *request)
{
  evhttp_send_error(request, HTTP_INTERNAL, NULL);
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
    manager_bad_request_cb_(request, user);
    break;
  }
}

void manager_queues_list_cb_(struct evhttp_request *request, void *user)
{
  struct manager_queue *q;
  struct json_object *queue_list;
  struct json_object *stringobject;

  queue_list = json_object_new_array();
  if (!queue_list) {
    manager_internal_error_(request);
    return;
  }

  TAILQ_FOREACH(q, &manager_context_.queues, next) {
    stringobject = json_object_new_string(q->id);
    if (!stringobject) {
      json_object_put(queue_list);
      manager_internal_error_(request);
    }

    if (json_object_array_add(queue_list, stringobject) != 0) {
      json_object_put(stringobject);
      json_object_put(queue_list);
      manager_internal_error_(request);
    }
  }

  manager_send_json(request, "queues", queue_list);
}

void manager_queues_new_cb_(struct evhttp_request *request, void *user)
{
  struct evkeyvalq params = {0};
  struct manager_queue *q;
  struct json_object *name;

  if (manager_read_post(request, &params) != 1) {
    manager_internal_error_(request);
    return;
  }

  if (manager_validate_(request, &q, 1, &params) != 1) {
    evhttp_clear_headers(&params);
    return;
  }
  evhttp_clear_headers(&params);

  name = json_object_new_string(q->id);
  if (!name) {
    manager_queue_free_(q);
    manager_internal_error_(request);
    return;
  }

  manager_send_json(request, "queue_name", name);
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
    manager_bad_request_cb_(request, user);
    break;
  }
}

void manager_queue_info_cb_(struct evhttp_request *request, void *user)
{
  struct evkeyvalq params = {0};
  struct manager_queue *q;
  struct json_object *info;
  struct json_object *name;

  if (manager_read_post(request, &params) != 1) {
    manager_internal_error_(request);
    return;
  }

  if (manager_validate_(request, &q, 0, &params) != 1) {
    evhttp_clear_headers(&params);
    return;
  }
  evhttp_clear_headers(&params);

  info = json_object_new_object();
  if (!info) {
    manager_internal_error_(request);
    return;
  }

  name = json_object_new_string(q->id);
  if (!name) {
    json_object_put(info);
    manager_internal_error_(request);
    return;
  }

  if (json_object_object_add_ex(info, "name", name,
                                JSON_C_OBJECT_ADD_KEY_IS_NEW) != 0) {
    json_object_put(name);
    json_object_put(info);
    manager_internal_error_(request);
    return;
  }

  manager_send_json(request, "queue", info);
}

void manager_queue_delete_cb_(struct evhttp_request *request, void *user)
{
  struct evkeyvalq params = {0};
  struct manager_queue *q;
  struct json_object *status;

  if (manager_read_post(request, &params) != 1) {
    manager_internal_error_(request);
    return;
  }

  if (manager_validate_(request, &q, 0, &params) != 1) {
    evhttp_clear_headers(&params);
    return;
  }
  evhttp_clear_headers(&params);

  manager_queue_free_(q);
  status = json_object_new_boolean(1);
  if (!status) {
    manager_queue_free_(q);
    manager_internal_error_(request);
    return;
  }

  manager_send_json(request, "delete", status);
}

void manager_take_item_cb_(struct evhttp_request *request, void *user)
{}

void manager_peek_item_cb_(struct evhttp_request *request, void *user)
{}

void manager_wait_item_cb_(struct evws_message *message, void *user)
{}

void manager_wait_close_cb_(struct evws_connection *connection, void *user)
{}

void manager_put_item_cb_(struct evhttp_request *request, void *user)
{}

void manager_wait_receive_cb_(struct queue_item *item, void *user)
{}

int manager_validate_(struct evhttp_request *request,
                      struct manager_queue **q, int create_new,
                      struct evkeyvalq *params)
{
  char *name;

  /* get the name of the new queue */
  name = evhttp_find_header(params, "name");

  /* verify the name - it must be a uuid */
  if (name && strlen(name) != QUEUE_UUID_STR_LEN) {
    manager_internal_error_(request);
    return 0;
  }

  *q = manager_get_queue_(name, create_new);
  if (!*q) {
    manager_internal_error_(request);
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
                                                   struct evws_connection *con)
{}

void manager_queue_want_free_(struct manager_queue_want *want)
{}
