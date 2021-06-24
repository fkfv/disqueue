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

#include "queue-compat.h"

#define QUEUE_UUID_LEN 16
#define QUEUE_UUID_STR_LEN (QUEUE_UUID_LEN * 2) + 4

struct queue;
struct queue_item;

struct queue *queue_new(const unsigned char id[16]);
void queue_free(struct queue *q);

/* put an item in the queue. returns -1 on failure, 0 if the item was added to
   the queue and 1 if the item was immediately consumed */
int queue_put(struct queue *q, const char *key, const char *value);

/* take an item from the queue. if the item does not exist, this function will
   fail by returning NULL
   @see queue_wait() */
struct queue_item *queue_take(struct queue *q, const char *key);

/* peek at an item in the queue. serves the same function as queue_take, but
   the item will not be removed. the item can be deleted at any time, so it
   should always be locked using queue_item_lock before trying to perform any
   actions on it */
struct queue_item *queue_peek(struct queue *q, const char *key);

/* wait for an item to become available in the queue, and then invoke the
   callback. the context argument will be provided to the callback. returns -1
   on failure, 0 if the item is not yet present and 1 if the callback was
   immediately triggered */
int queue_wait(struct queue *q, const char *key,
               void(*cb)(struct queue_item *, void *), void *arg);

/* get the uuid of a queue as a printable string */
void queue_get_uuid(struct queue *q, char uuid[QUEUE_UUID_STR_LEN + 1/*NULL*/]);

/* get the key and value of the item. this is only valid if:
     the item has been locked OR
     the item has been taken OR
     the callback the item was given to has not returned */
const char *queue_item_get_key(struct queue_item *item);
const char *queue_item_get_value(struct queue_item *item);

/* lock a queue item. this should only be used on items found using queue_peek,
   queue_take does not need to do this as the item is always available if it
   was taken. the item should be unlocked after finishing, otherwise it will
   leak memory */
int queue_item_lock(struct queue_item *item);
void queue_item_unlock(struct queue_item *item);

/* free an item. this should only be called on items that have been taken.
   while the item may not be freed immediately if it was locked, the caller
   should not try to access the item after this function returns */
void queue_item_free(struct queue_item *item);
