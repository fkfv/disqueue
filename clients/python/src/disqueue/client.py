import requests
import typing
from requests.exceptions import HTTPError
from urllib.parse import urljoin

from .protocol import ProtocolMessage, parse as protocol_parse


QueueItem = typing.Optional[typing.Tuple[str, str]]


class QueueClient:
    """
    Client for interacting with the HTTP interface of the specified queue
    """
    
    url: str
    queue_id: str

    def __init__(self, url: str, queue_id: str):
      self.url = url
      self.queue_id = queue_id

    def put(self, key: typing.Optional[str], value: str):
        """Put a message in the queue."""
        message = self._request('/put', params=QueueClient._with_key({
            'value': value
            }, key))

    def take(self, key: typing.Optional[str] = None) -> QueueItem:
        """Take a message from the queue."""
        message = self._request('/take', params=QueueClient._with_key({}, key))
        message.raise_on_error()

        return message.payload

    def peek(self, key: typing.Optional[str] = None) -> QueueItem:
        """Peek at the queue without taking an item."""
        message = self._request('/peek', params=QueueClient._with_key({}, key))
        message.raise_on_error()

        return message.payload

    def delete(self):
        """Delete the queue."""
        message = self._request('/queue', 'DELETE')
        message.raise_on_error()

    def info(self) -> dict[str, str]:
        """Get info about the queue."""
        message = self._request('/queue')
        message.raise_on_error()

        return message.payload

    def verify(self):
        """Verify the queue exists."""
        self.info()

    def _request(self, path: str, method: str = 'POST', \
                 params: typing.Dict[str, str] = {}) -> ProtocolMessage:
        params = params.copy()
        params.update({
            'name': self.queue_id
            })

        return _request(self.url, path, method, params)

    @staticmethod
    def _with_key(params: typing.Dict[str, str], \
                  key: typing.Optional[str]) -> typing.Dict[str, str]:
        params = params.copy()
        if key is not None:
            params.update({
                'key': key
                })

        return params


def queue_create(url, name: str = None) -> str:
    """Create a new queue and return the name."""
    return _request(url, '/queues', 'POST', {}).payload


def queue_list(url) -> typing.List[str]:
    """Get a list the names of all queues."""
    return _request(url, '/queues', 'GET', {}).payload

def _request(url: str, path: str, method: str, \
             params: typing.Dict[str, str]) -> ProtocolMessage:
    try:
        response = requests.request(method, urljoin(url, path),
                                    data=params)
        return protocol_parse(response.text)
    except HTTPError as error:
        return protocol_parse(error.response.text)
