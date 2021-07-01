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

#include "protocol.h"

struct json_object *protocol_create_success(json_object *payload)
{
  return protocol_create_base(1, NULL, payload);
}

struct json_object *protocol_create_failure(const char *error_message)
{
  return protocol_create_base(0, error_message, NULL);
}

struct json_object *protocol_create_base(int success, const char *message,
                                         json_object *payload)
{
  struct json_object *container = NULL;
  struct json_object *msg = NULL;
  struct json_object *status = NULL;

  if (message) {
    msg = json_object_new_string(message);
    if (!msg) {
      goto cleanup;
    }
  }

  status = json_object_new_boolean(success);
  if (!status) {
    goto cleanup;
  }

  container = json_object_new_object();
  if (!container) {
    goto cleanup;
  }

  if (json_object_object_add_ex(container, "message", msg,
                                JSON_C_OBJECT_ADD_KEY_IS_NEW |
                                JSON_C_OBJECT_KEY_IS_CONSTANT) != 0) {
    goto cleanup;
  }
  msg = NULL;

  if (json_object_object_add_ex(container, "success", status,
                                JSON_C_OBJECT_ADD_KEY_IS_NEW |
                                JSON_C_OBJECT_KEY_IS_CONSTANT) != 0) {
    goto cleanup;
  }
  status = NULL;

  if (payload) {
    if (json_object_object_add_ex(container, "payload", payload,
                                  JSON_C_OBJECT_ADD_KEY_IS_NEW |
                                  JSON_C_OBJECT_KEY_IS_CONSTANT) != 0) {
      goto cleanup;
    }
    payload = NULL;
  }

  return container;

cleanup:
  if (msg) {
    json_object_put(payload);
  }
  if (payload) {
    json_object_put(payload);
  }
  if (status) {
    json_object_put(status);
  }
  if (container) {
    json_object_put(container);
  }

  return NULL;
}

const char *protocol_failure_fallback(void)
{
  return "{\"success\": false, \"message\": \"cannot describe error\"}";
}

const char *protocol_failure_message(void)
{
  return "cannot describe error";
}

struct json_object *protocol_encode_item(struct queue_item *item)
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

