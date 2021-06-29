import json
import typing


class QueueException(Exception):
    def __init__(self, message):
        self.message = message


class ProtocolMessage:
    """
    Class representing a received message in the Disqueue JSON format.
    """

    success: bool
    message: typing.Optional[str]
    _payload: typing.Optional[typing.Any]

    def __init__(self, success: bool, *, message: str = None, \
                 payload: typing.Any = None):
        self.success = success

        if message:
            self.message = message
        else:
            self._payload = payload

    @property
    def is_successful(self) -> bool:
        return self.success

    @property
    def error_message(self) -> str:
        if self.success:
            raise EXCEPTION_CLASS('Attempted to access an undefined message.')

        return self.message

    @property
    def payload(self):
        if not self.success:
            raise EXCEPTION_CLASS('Attempted to access an undefined payload.')

        return self._payload

    def raise_on_error(self):
        """Raise an exception if the message does not indicate success"""
        if not self.success:
            raise EXCEPTION_CLASS(self.message)


def parse(data: str) -> ProtocolMessage:
    """Attempt to parse JSON data as a protocol message"""
    try:
        protocol = json.loads(data)
    except json.JSONDecodeError as e:
        raise EXCEPTION_CLASS('Invalid protocol data: ' + str(e))
    else:
        try:
            success = protocol['success']
            message = None
            payload = None

            if success:
                payload = protocol.get('payload')
            else:
                message = protocol['message']
        except KeyError as e:
            raise EXCEPTION_CLASS('Protocol missing key: ' + str(e))
        else:
            return ProtocolMessage(success, message=message, payload=payload)


# Class used for protocol exceptions
EXCEPTION_CLASS = QueueException
