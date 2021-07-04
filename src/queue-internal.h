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

#ifndef QUEUE_INTERNAL_H
#define QUEUE_INTERNAL_H

struct queue;
struct queue_item;

#include "queue-compat.h"

#ifndef QUEUE_UUID_LEN
#define QUEUE_UUID_LEN 16
#endif

struct queue_item {
  TAILQ_ENTRY(queue_item) next;

  char *key;
  char *value;

  /* has the item actually been inserted into the queue item list */
  int inserted;

  /* which queue this item belongs to */
  struct queue *owner;

  /* the lock for this item. while the item is locked, it guarantees that it
     will not be deleted. TODO: should this lock be atomic?*/
  int lockcount;
};

/* callback for someone waiting to see items added to the queue */
struct queue_callback {
  TAILQ_ENTRY(queue_callback) next;

  /* key that is being waited on */
  char *key;

  /* callback + user data for when the matching item is added */
  void (*addcb)(struct queue_item *, void *);
  void *addcbarg;

  /* queue that the item belongs to */
  struct queue *owner;
};

struct queue {
  /* queue uuid as bytes */
  unsigned char uuid[QUEUE_UUID_LEN];

  /* number of keyed entries/callbacks - will allow the item check to be easily
     skipped if no keyed entries are present */
  size_t keyed_count;
  size_t keyed_callback_count;

  /* number of items */
  size_t item_count;

  /* number of callbacks */
  size_t callback_count;

  /* queue items */
  TAILQ_HEAD(qihead, queue_item) items;

  /* queue callbacks */
  TAILQ_HEAD(qchead, queue_callback) callbacks;
};

struct queue_item *queue_item_new_(const char *key, const char *value);
/* this free function will ignore the lock count and delete anyway */
void queue_item_free_(struct queue_item *item);

struct queue_callback *queue_callback_new_(const char *key);
void queue_callback_free_(struct queue_callback *cb);

#endif
