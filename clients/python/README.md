# Disqueue
> Disqueue Python client

## Requirements
* Python >= 3.4 (for asyncio)
* autobahn (MIT License)
* requests (Apache License 2.0)

Requirements can be installed using pip: `pip install -r requirements.txt`

## License
Disqueue is licensed under the MIT license. The full license text is available
in the LICENSE file.

## Example
```python
from disqueue import Disqueue, queue_create, queue_list


# Create the client - 127.0.0.1 instead of localhost because Python is slow to
# resolve localhost.
# username: test password: test
# can be left out if auth is not enabled
auth = 'test', 'test'
url = 'http://127.0.0.1:3682'


queues = queue_list(url, auth=auth)
if len(queue):
    queue_id = queues[0]
else:
    queue_id = queue_create(url, auth=auth)
print(queue_id)

client = Disqueue('http://127.0.0.1:3682', auth=auth)
@client.queue_handler(queue_id, key=None)
def queue_handler(key: typing.Optional[str], value: str):
    print(key, value)


client.run()
```


CURL can be used to test that the client is working:
`curl -X POST --data "name=queue-id&key=key&value=value" --basic --user
  test:test http://127.0.0.1:3682/put`
