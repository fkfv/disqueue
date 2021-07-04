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

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <json-c/json_object.h>
#include "queue.h"

/**
 * JSON protocol. json_object returns must be freed using json_object_put if
 * they do not return NULL. If a parameter is a json_object it will either be
 * owned by the returned object or json_object_put will be called if the
 * function fails / the function does not return a json_object.
 */

/* create success/failure messages */
struct json_object *protocol_create_success(json_object *payload);
struct json_object *protocol_create_failure(const char *error_message);

/* create the base layout */
struct json_object *protocol_create_base(int success, const char *message,
                                         json_object *payload);

/* fallback json if a proper error cannot be created */
const char *protocol_failure_fallback(void);
const char *protocol_failure_message(void);

struct json_object *protocol_encode_item(struct queue_item *item);

#endif
