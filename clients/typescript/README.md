# Disqueue
> Disqueue TypeScript client

## Requirements

## License
Disqueue is licensed under the MIT license. The full license text is available
in the LICENSE file.

## Example
```typescript
import {Disqueue, queueList, queueCreate} from 'disqueue';

(async function() {
  const url = 'http://127.0.0.1:3682';
  const auth = ['test', 'test'];

  const queues = await queueList(url, auth);
  let queueId;
  if (queues.length > 0) {
      queueId = queues[0];
  } else {
      queueId = await queueCreate(url, undefined, auth);
  }
  console.log(queueId);

  const client = new Disqueue(url, auth);
  client.queueHandler(queueId)(function (key, value) {
    console.log(key, value);
  });

  client.run();
})();
```

CURL can be used to test that the client is working:
`curl -X POST --data "name=queue-id&key=key&value=value" --basic --user
  test:test http://127.0.0.1:3682/put`
