class QueueException extends Error {}

class ProtocolMessage {
  success: boolean;
  message?: string;
  _payload?: unknown;

  constructor(success: boolean, props: {message?: string, payload?: unknown}) {
    this.success = success;

    if (this.success) {
      this._payload = props.payload;
    } else {
      this.message = props.message;
    }
  }

  get isSuccessful(): boolean {
    return this.success;
  }

  get errorMessage(): string {
    if (this.success) {
      throw new QueueException('Attempted to access an undefined message.');
    }

    return this.message;
  }

  get payload(): unknown {
    if (!this.success) {
      throw new QueueException('Attempted to access an undefined payload.');
    }

    return this._payload;
  }

  raiseOnError(): void|never {
    if (!this.success) {
      throw new QueueException(this.message);
    }
  }
}

function parse(data: string): ProtocolMessage {
  let protocol;
  try {
    protocol = JSON.parse(data);
  } catch (e) {
    if (e instanceof SyntaxError) {
      throw new QueueException('Invalid protocol data: ' + e.message);
    } else {
      throw e;
    }
  }

  const success = protocol.success;
  let message, payload;
  if (success) {
    payload = protocol.payload;
  } else {
    message = protocol.message;
  }

  return new ProtocolMessage(success, {
    message, payload
  });
}

export {QueueException, ProtocolMessage, parse};
