import fetch from 'node-fetch';
import {ProtocolMessage, QueueException,
        parse as protocolParse} from './protocol';

interface ParamsType {
  [key: string]: string
}

interface QueueItem {
  key: string | null,
  value: string
}

interface QueueInfo {
  name: string
}

type QueueAuthentication = [string, string];

class QueueClient {
  url: string;
  queueId: string;
  auth?: QueueAuthentication;

  constructor(url: string, queueId: string, auth?: QueueAuthentication) {
    this.url = url;
    this.queueId = queueId;
    this.auth = auth;
  }

  async put(key: string | null, value: string): Promise<void> {
    const message = await this._request('/put',
                                        QueueClient._with_key({value}, key));
    message.raiseOnError();
  }

  async take(key: string | null = null): Promise<QueueItem> {
    const message = await this._request('/take',
                                        QueueClient._with_key({}, key));
    message.raiseOnError();

    return (message.payload as QueueItem);
  }

  async peek(key: string | null = null): Promise<QueueItem> {
    const message = await this._request('/peek',
                                        QueueClient._with_key({}, key));
    message.raiseOnError();

    return (message.payload as QueueItem);
  }

  async delete(): Promise<void> {
    const message = await this._request('/queue', {}, 'DELETE');
    message.raiseOnError();
  }

  async info(): Promise<QueueInfo> {
    const message = await this._request('/queue', {});
    message.raiseOnError();

    return (message.payload as QueueInfo);
  }

  async verify(): Promise<void> {
    await this.info();
  }

  async _request(path: string, params: ParamsType,
                 method = 'POST'): Promise<ProtocolMessage> {
    return _request(this.url, path, method, {
      ...params,
      name: this.queueId
    }, this.auth);
  }

  static _with_key(params: ParamsType, key: string | null): ParamsType {
    if (key !== null) {
      return {
        key,
        ...params
      };
    }

    return params;
  }
}

async function queueCreate(url: string, name?: string,
                           auth?: QueueAuthentication): Promise<string> {
  const message = await _request(url, '/queues', 'POST', {}, auth);
  message.raiseOnError();

  return (message.payload as string);
}

async function queueList(url: string,
                         auth?: QueueAuthentication): Promise<Array<string>> {
  const message = await _request(url, '/queues', 'GET', {}, auth);
  message.raiseOnError();

  return (message.payload as Array<string>);
}

async function _request(url: string, path: string, method: string,
                        params: ParamsType, auth?: QueueAuthentication) {
  let body;
  if (method !== 'GET') {
    body = Object.entries(params).map(entry =>
      entry.map(encodeURIComponent).join('=')).join('&');
  }

  return fetch(_urlJoin(url, path), {
    method,
    body,
    headers: _authenticationHeader(auth)
  })
    .then(response => response.text())
    .then(protocol => protocolParse(protocol))
    .catch(error => {throw new QueueException(error.message);});
}

function _authenticationHeader(auth?: QueueAuthentication): ParamsType {
  if (auth !== undefined) {
    const encodedAuth = Buffer.from(`${auth[0]}:${auth[1]}`).toString('base64');

    return {
      'Authorization': `Basic ${encodedAuth}`
    };
  }

  return {};
}

function _urlJoin(url: string, path: string): string {
  const last = url.length - 1

  if (last <= 0 || path.length == 0) {
    return url + path;
  } else if (url[last] == '/' && path[0] == '/') {
    path = path.slice(1);
  } else if (url[last] != '/' && path[0] != '/') {
    path = '/' + path;
  }

  return url + path;
}

export {QueueClient, QueueItem, QueueInfo, QueueAuthentication, queueCreate,
        queueList, _authenticationHeader};
