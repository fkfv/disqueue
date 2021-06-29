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
from disqueue import Disqueue


# Create the client - 127.0.0.1 instead of localhost because Python is slow to
# resolve localhost.
client = Disqueue('http://127.0.0.1:3682')

@client.queue_handler('queue-id', key=None)
def queue_handler(key: typing.Optional[str], value: str):
    pass


client.run()
```
