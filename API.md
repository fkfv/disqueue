# Disqueue API
## Format
All responses are in JSON. They all use the following format:
```javascript
{
  "success": true, /* request succeeded */
  "message": null, /* contains an error message if success is false */
  "payload": null,  /* contains the response if success is true */
}
```

All responses can have a 500 error code, this means that the server has failed.

## Endpoints
 * [/queues  [GET]       - List all queues](#get-queues)
 * [/queues  [POST]      - Create new queue](#post-queues)
 * [/queue   [POST]      - List queue information](#post-queue)
 * [/queue   [DELETE]    - Delete queue](#delete-queue)
 * [/take    [POST]      - Take item from queue](#post-take)
 * [/peek    [POST]      - Peek at item in queue](#post-peek)
 * [/put     [POST]      - Put item in queue](#post-put)
 * [/take/ws [WebSocket] - Wait for item in queue](#websocket-takews)

---

### GET /queues
> List all of the queues currently available
#### Response
```javascript
{
  "success": true,
  "message": null,
  "payload": [
    "8a672d01-0daa-47c5-8d2e-c55c93fc71c2",
    "e2e0b44e-e636-48d7-9602-178e7403ef77"
    /* additional queue ids */
  ]
}
```
---
### POST /queues
> Create a new queue
#### Request
* name - optional, name of the new queue to create
#### Response
```javascript
{
  "success": true,
  "message": null, 
  "payload": "e2e0b44e-e636-48d7-9602-178e7403ef77" /* name of the new queue */
}
````
#### Additional Error Codes
* 400 - `name` parameter is not a uuid
---
### POST /queue
> Get information about a queue
#### Request
* name - name of the queue
#### Response
```javascript
{
  "success": true,
  "message": null,
  "payload": {
    "name": "e2e0b44e-e636-48d7-9602-178e7403ef77"
  }
}
```
#### Additional Error Codes
* 400 - `name` parameter is not a uuid
* 404 - `name` is not an existing queue
---
### DELETE /queue
> Delete a queue
#### Request
* name - name of the queue
#### Response
```javascript
{
  "success": true,
  "message": null,
  "payload": null
}
```
#### Additional Error Codes
* 400 - `name` parameter is not a uuid
* 404 - `name` is not an existing queue
---
### POST /take
> Take an item from the queue
#### Request
* name - name of the queue
* key - optional, key required to take item
#### Response
```javascript
{
  "success": true,
  "message": null,
  "payload": {
    "key": null, /* item key if present */
    "value": ""  /* item value */
  }
}
```
### Additional Error Codes
* 400 - `name` parameter is not a uuid
* 404 - `name` is not an existing queue, or no item was found in the queue
---
### POST /peek
> Peek at an item in the queue without removing it
#### Request
* name - name of the queue
* key - optional, key required to peek item
#### Response
```javascript
{
  "success": true,
  "message": null,
  "payload": {
    "key": null, /* item key if present */
    "value": ""  /* item value */
  }
}
```
### Additional Error Codes
* 400 - `name` parameter is not a uuid
* 404 - `name` is not an existing queue, or no item was found in the queue
---
### POST /put
> Put a new item in the queue
#### Request
* name - name of the queue
* value - value of the item to add to the queue
* key - optional, key of the item to add to the queue
#### Response
```javascript
{
  "success": true,
  "message": null,
  "payload": null
}
```
#### Additional Error Codes
* 400 - `name` parameter is not a uuid or `value` parameter is missing
* 404 - `name` is not an existing queue
---
### WebSocket /take/ws
#### Client->Server Messages
> Request notification for queue item
```javascript
{
  "identifier": "random-string", /* uniqueue identifier for this request */
  "queue": "e2e0b44e-e636-48d7-9602-178e7403ef77", /* queue id */
  "key": "mykey" /* optional, key required to match */
}
```
#### Server->Client Messages
> Error (Sent at any time)
```javascript
{
  "success": false,
  "message": "failed to create want", /* one possible error message */
  "payload": null
}
```
> Item Received (One to one response to requests, not necessarily in order)
```javascript
{
  "success": true,
  "message": null,
  "payload": {
    "id": "random-string", /* unique identifier from request */
    "item": {
      "key": null, /* item key */
      "value": "" /* item value */
    }
  }
}
```