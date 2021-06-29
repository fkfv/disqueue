import asyncio
import functools
import json
import typing
from autobahn.asyncio.websocket import WebSocketClientFactory
from hashlib import sha1
from urllib.parse import urlparse
from uuid import uuid4

from .client import QueueClient
from .protocol import ProtocolMessage, DisqueueWebSocketProtocol


# Types that are returned by the queue_handler decorator function.
QueueHandler = typing.Callable[[typing.Optional[str], str], None]
QueueDecorator = typing.Callable[[QueueHandler], QueueHandler]
QueueWaitingItem = typing.List[typing.Tuple[str, QueueHandler]]
QueueRescheduleItem = typing.Tuple[str, str, QueueHandler]


class Disqueue:
    """
    Disqueue client application. Register queue handlers and then wait for
    responses on the queue to the handlers.
    """

    # Mapping of queue_id -> [key, queue_handler]
    waiting_handlers: typing.Dict[str, QueueWaitingItem] = dict()

    # Mapping of identifier->queue_id, key, queue_handler
    waiting_identifiers: typing.Dict[str, QueueRescheduleItem] = dict()

    # Cached queue clients
    cached_clients: typing.Dict[str, QueueClient] = dict()

    # WebSocket connection
    factory: WebSocketClientFactory

    # Connection URLs
    url: str = ''
    connection_tuple: typing.Tuple[str, int] = tuple()

    def __init__(self, url):
        ws_url, connection_tuple = Disqueue._create_websocket_address(url)
        self.url = url
        self.factory = WebSocketClientFactory(ws_url)
        self.factory.protocol = DisqueueWebSocketProtocol
        self.connection_tuple = connection_tuple

        # The protocol will be able to access this client using the same name -
        # self.factory.client
        self.factory.client = self

    def run(self):
        """
        Connect to the websocket and run the asyncio loop waiting for responses
        on the websocket.
        """
        loop = asyncio.get_event_loop()
        connection_routine = loop.create_connection(self.factory,
                                                    *self.connection_tuple)

        loop.run_until_complete(connection_routine)
        loop.run_forever()
        loop.close()

    def queue_handler(self, queue_id: str,
                      queue_key: typing.Optional[str] = None) -> QueueDecorator:
        """
        Decorate a queue handler function. The queue handler will be added to
        the list of wants for the specified queue, and optionally also the
        specified key. The signature of a queue handler looks like this:
            @client.queue_handler('00000000-0000-0000-0000-000000000000', 'key')
            def queue_handler(key: typing.Optional[str], value: str):
                if key is not None:
                    print(key)
                print(value)

        The queue handler will be called whenever an item matching the specified
        queue and key arrives, the queue handler will be executed, and then the
        want for the queue ID and key will be added back.

        The queue handler decorator must be invoked before run() for the handler
        to be registered.
        """
        def queue_handler_decorator(func):
            self._append_or_create(
                queue_id,
                (queue_key, func))

            return func

        return queue_handler_decorator

    def queue(self, queue_id: str) -> QueueClient:
        """Create a client for the specified queue ID and verify it is valid."""
        if queue_id in self.cached_clients:
            return self.cached_clients[queue_id]

        client = QueueClient(self.url, queue_id)
        client.info()

        # Only cache the client if the queue was valid
        self.cached_clients[queue_id] = client
        return client

    def notify_open(self, sendCallack):
        """Create new queues for the first time"""
        for queue_id, key_and_handlers in self.waiting_handlers.items():
            for key_and_handler in key_and_handlers:
                key, handler = key_and_handler
                self.resend_want(queue_id, key, handler, sendCallack)

        self.waiting_handlers.clear()

    def notify_message(self, message: ProtocolMessage, sendCallack):
        """Handle response messages from the server"""
        payload = message.payload
        queue_id, key, queue_handler = self.waiting_identifiers.pop(payload['id'])

        # Run the queue handler and then requeue after completion
        def run_and_reschedule():
            queue_handler(payload['item']['key'], payload['item']['value'])
            self.resend_want(queue_id, key, queue_handler, sendCallack)

        # Run in a separate thread since the handler may take a long time to
        # run.
        asyncio.get_event_loop().create_task(asyncio.to_thread(run_and_reschedule))


    def resend_want(self, queue_id: str, key: typing.Optional[str], \
                    queue_handler: QueueHandler, sendCallack):
        """
        Send a want request for the specified queue_id/key and use queue_handler
        to handle the response.
        """
        new_id = self._create_identifier()
        self.waiting_identifiers[new_id] = queue_id, key, queue_handler
        params = {
            'queue': queue_id,
            'identifier': new_id
        }

        if key:
            params.update({
                'key': key
            })

        sendCallack(json.dumps(params).encode('utf-8'))

    def _append_or_create(self, hashkey: str, handler: QueueWaitingItem):
        """Add a handler to the dictionary."""
        if hashkey not in self.waiting_handlers:
            self.waiting_handlers[hashkey] = []

        self.waiting_handlers[hashkey].append(handler)

    @staticmethod
    def _create_identifier() -> str:
        """Create an identifier used for handlers."""
        return str(uuid4())

    @staticmethod
    def _create_websocket_address(url) -> typing.Tuple[str, typing.Tuple[str, int]]:
        """Create websocket URI from HTTP URL."""
        parse = urlparse(url)
        hostname = parse.hostname
        netloc = parse.netloc
        port = parse.port or 80 if parse.scheme == 'http' else 443
        new_scheme = 'ws' if parse.scheme == 'http' else 'wss'

        return f'{new_scheme}://{netloc}/take/ws', (hostname, port)
