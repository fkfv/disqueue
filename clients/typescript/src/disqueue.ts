import {randomBytes} from 'crypto';
import WebSocket from 'ws';

import {QueueAuthentication, QueueClient,
        _authenticationHeader} from './client';
import {ProtocolMessage, parse as parseMessage} from './protocol';

type QueueHandler = (key: string | null, value: string) => void;
type QueueDecorator = (handler: QueueHandler) => void;
type QueueWaitingItem = Array<[string, QueueHandler]>;
type QueueRescheduleItem = [string, string | null, QueueHandler];

interface QueueScheduleResponse {
  id: string,
  item: {
    key: string | null,
    value: string
  }
}

class Disqueue {
  waitingHandlers: {[key: string]: QueueWaitingItem} = {};
  waitingIdentifiers: {[key: string]: QueueRescheduleItem} = {};
  cachedClients: {[key: string]: QueueClient} = {};

  url: string;
  auth?: QueueAuthentication;

  socket: WebSocket;

  constructor(url: string, auth?: QueueAuthentication) {
    this.url = url;
    this.auth = auth;
  }

  run(): void {
    this.socket = new WebSocket(Disqueue._createWebSocketAddress(this.url), {
      headers: _authenticationHeader(this.auth)
    });

    this.socket.on('open', () => this.notifyOpen());
    this.socket.on('message', (message: string) => {
      const payload = parseMessage(message);
      payload.raiseOnError();

      this.notifyMessage(payload);
    });
  }

  queueHandler(queueId: string, queueKey: string | null): QueueDecorator {
    return (func: QueueHandler): QueueHandler => {
      if (!Object.prototype.hasOwnProperty.call(this.waitingHandlers,
                                                queueId)) {
        this.waitingHandlers[queueId] = [];
      }

      this.waitingHandlers[queueId].push([queueKey, func]);

      return func;
    };
  }

  queue(queueId: string): QueueClient {
    if (Object.prototype.hasOwnProperty.call(this.cachedClients, queueId)) {
      return this.cachedClients[queueId];
    }

    const client = new QueueClient(this.url, queueId, this.auth);
    client.info();

    this.cachedClients[queueId] = client;
    return client;
  }

  notifyOpen(): void {
    Object.entries(this.waitingHandlers).forEach(([queueId, keyAndHandlers]) => {
      keyAndHandlers.forEach(([key, handler]) => {
        this.resendWant(queueId, key, handler);
      });
    });

    this.waitingHandlers = {}
  }

  notifyMessage(message: ProtocolMessage): void {
    const payload = (message.payload as QueueScheduleResponse);
    const [queueId, key, queueHandler] = this.waitingIdentifiers[payload.id];
    delete this.waitingIdentifiers[payload.id];

    const runAndReschedule = () => {
      queueHandler(payload.item.key, payload.item.value);
      this.resendWant(queueId, key, queueHandler);
    };

    runAndReschedule();
    /* schedule_in_thread(runAndReschedule) */
  }

  resendWant(queueId: string, key: string | null,
             queueHandler: QueueHandler): void {
    const newId = Disqueue._createIdentifier();
    this.waitingIdentifiers[newId] = [queueId, key, queueHandler];
    const params = {
      queue: queueId,
      identifier: newId,
      key: undefined
    };

    if (key !== null) {
      params.key = key;
    }

    this.socket.send(JSON.stringify(params));
  }

  static _createIdentifier(): string {
    /* create version 4 uuid */
    const uuid = randomBytes(16);

    uuid[6] = uuid[6] & 0x0f | 0x40; /* version = 4 */
    uuid[8] = uuid[8] & 0x3f | 0x80; /* variant = dce */

    const hexPattern = [4, 2, 2, 2, 6];
    let index = 0;

    return hexPattern.map(count => {
      index += count;
      return uuid.toString('hex', index - count, index);
    }).join('-');
  }

  static _createWebSocketAddress(url: string): string {
    const parsed = new URL(url);
    const {host, protocol} = parsed;
    const new_scheme = protocol === 'http:' ? 'ws' : 'wss';

    return `${new_scheme}://${host}/take/ws`;
  }
}

export {Disqueue};
