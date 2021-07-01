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

#include <openssl/rand.h>
#include <stdio.h>

#include "queue.h"
#include "queue-internal.h"
#include "strcase.h"
#include "hostnet.h"

struct queue *queue_new(const unsigned char id[QUEUE_UUID_LEN])
{
  struct queue *q;

  q = calloc(1, sizeof(struct queue));
  if (!q) {
    return NULL;
  }

  /* create new ID if one was not provided */
  if (id != NULL) {
    memcpy(q->uuid, id, QUEUE_UUID_LEN);
  } else {
    if (RAND_bytes(q->uuid, QUEUE_UUID_LEN) != 1) {
      free(q);
      return NULL;
    }

    /* make id a valid uuid4 */
    q->uuid[6] = q->uuid[6] & 0x0f | 0x40; /* version = 4 */
    q->uuid[8] = q->uuid[8] & 0x3f | 0x80; /* variant = dce */
  }

  TAILQ_INIT(&q->items);
  TAILQ_INIT(&q->callbacks);

  return q;
}

void queue_free(struct queue *q)
{
  struct queue_callback *callback;
  struct queue_item *item;

  /* cancel any callbacks */
  while ((callback = TAILQ_FIRST(&q->callbacks)) != NULL) {
    queue_callback_free_(callback);
  }

  /* clean up the remaining items */
  while ((item = TAILQ_FIRST(&q->items)) != NULL) {
    queue_item_free_(item);
  }

  free(q);
}

int queue_put(struct queue *q, const char *key, const char *value)
{
  struct queue_item *item;
  struct queue_callback *callback;
  void (*cb)(struct queue_item *, void *);
  void *cbarg;

  item = calloc(1, sizeof(struct queue_item));
  if (!item) {
    return -1;
  }

  /* add the item key */
  if (key) {
    item->key = strdup(key);
    if (!item->key) {
      goto error;
    }
  }

  item->owner = q;
  item->value = strdup(value);
  if (!item->value) {
    goto error;
  }

  /* check if we can immediately consume the item */
  if (q->callback_count > 0) {
    if ((!key && q->keyed_callback_count != q->callback_count) || key) {
      TAILQ_FOREACH(callback, &q->callbacks, next) {
        /* check if we want all items in the callback, or if the callback key
           matches the item key */
        if (!callback->key || (key && !strcasecmp(callback->key, key))) {
          /* take the callback out as soon as possible. */
          cb = callback->addcb;
          cbarg = callback->addcbarg;
          queue_callback_free_(callback);

          if (cb) {
            cb(item, cbarg);
          }

          queue_item_free(item);
          return 1;
        }
      }
    }
  }

  item->inserted = 1;
  q->item_count++;
  if (key) {
    q->keyed_count++;
  }

  TAILQ_INSERT_TAIL(&q->items, item, next);

  return 0;
error:
  /* cleanup after an error */
  if (item->key) {
    free(item->key);
  }

  if (item->value) {
    free(item->value);
  }

  free(item);
  return -1;
}

struct queue_item *queue_take(struct queue *q, const char *key)
{
  struct queue_item *item;

  if (q->item_count == 0) {
    return NULL;
  }

  if (!key || (key && q->keyed_count > 0)) {
    TAILQ_FOREACH(item, &q->items, next) {
      /* check if the item matches the key */
      if (!key || (item->key && !strcasecmp(item->key, key))) {
        /* take the item out as soon as possible */
        TAILQ_REMOVE(&q->items, item, next);
        item->inserted = 0;
        q->item_count--;
        if (item->key) {
          q->keyed_count--;
        }

        return item;
      }
    }
  }

  return NULL;
}

struct queue_item *queue_peek(struct queue *q, const char *key)
{
  struct queue_item *item;

  if (q->item_count == 0) {
    return NULL;
  }

  if (!key || (key && q->keyed_count > 0)) {
    TAILQ_FOREACH(item, &q->items, next) {
      /* check if the item matches the key */
      if (!key || (item->key && !strcasecmp(item->key, key))) {
        return item;
      }
    }
  }

  return NULL;
}

int queue_wait(struct queue *q, const char *key,
               void(*cb)(struct queue_item *, void *), void *arg)
{
  struct queue_item *item;
  struct queue_callback *callback;

  /* try to take the item - we might not need to set the callback up in the
     table */
  item = queue_take(q, key);
  if (item) {
    cb(item, arg);
    queue_item_free(item);
    return 1;
  }

  callback = queue_callback_new_(key);
  if (!callback) {
    return -1;
  }

  /* add the callback */
  callback->owner = q;
  callback->addcb = cb;
  callback->addcbarg = arg;
  q->callback_count++;
  if (key) {
    q->keyed_callback_count++;
  }
  TAILQ_INSERT_TAIL(&q->callbacks, callback, next);

  return 0;
}

void queue_get_uuid(struct queue *q, char uuid[QUEUE_UUID_STR_LEN + 1/*NULL*/])
{
  /* uuid is network byte order numbers - this matters since the version and
     variant are in octet 6 and 8 */
  sprintf(uuid, "%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x",
          htonl(*(unsigned long *)&q->uuid[0]),
          htons(*(unsigned short *)&q->uuid[4]),
          htons(*(unsigned short *)&q->uuid[6]),
          htons(*(unsigned short *)&q->uuid[8]),
          q->uuid[10],
          q->uuid[11],
          q->uuid[12],
          q->uuid[13],
          q->uuid[14],
          q->uuid[15]);
}

const char *queue_item_get_key(struct queue_item *item)
{
  return item->key;
}

const char *queue_item_get_value(struct queue_item *item)
{
  return item->value;
}

int queue_item_lock(struct queue_item *item)
{
  /* TODO: how do we guarantee the item is still valid? */
  item->lockcount++;

  return 0;
}

void queue_item_unlock(struct queue_item *item)
{
  item->lockcount--;

  /* handle if this item is now ready to be freed */
  if (item->lockcount == 0 && !item->inserted) {
    queue_item_free(item);
  }
}

void queue_item_free(struct queue_item *item)
{
  if (item->lockcount != 0) {
    return;
  }

  queue_item_free_(item);
}

struct queue_item *queue_item_new_(const char *key, const char *value)
{
  struct queue_item *item;

  item = calloc(1, sizeof(struct queue_item));
  if (!item) {
    return NULL;
  }

  item->value = strdup(value);
  if (!item->value) {
    free(item);
    return NULL;
  }

  if (key) {
    item->key = strdup(key);
    if (!item->key) {
      free(item->value);
      free(item);
      return NULL;
    }
  }

  return item;
}

void queue_item_free_(struct queue_item *item)
{
  if (item->inserted) {
    TAILQ_REMOVE(&item->owner->items, item, next);
  }

  if (item->key) {
    free(item->key);
  }
  free(item->value);
  free(item);
}

struct queue_callback *queue_callback_new_(const char *key)
{
  struct queue_callback *callback;

  callback = calloc(1, sizeof(struct queue_callback));
  if (!callback) {
    return NULL;
  }

  if (key) {
    callback->key = strdup(key);
    if (!callback->key) {
      free(callback);
      return NULL;
    }
  }

  return callback;
}

void queue_callback_free_(struct queue_callback *cb)
{
  if (cb->key) {
    free(cb->key);
    cb->owner->keyed_callback_count--;
  }
  cb->owner->callback_count--;
  TAILQ_REMOVE(&cb->owner->callbacks, cb, next);

  free(cb);
}
