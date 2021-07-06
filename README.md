# Disqueue
> Simple distributed queue service.

## Requirements
* libevent - the latest master branch of libevent does not yet support taking
   ownership of a evhttp_connections' bufferevent, so for now a fork of libevent
   must be used to build disqueue - https://github.com/fkfv/libevent

* wslay - wslay is used to provide websocket support -
    https://github.com/tatsuhiro-t/wslay

* json-c - json-c is used to parse and generate JSON -
    https://github.com/json-c/json-c

To instruct the build script on where to find these libraries, you can set
`-DLIBEVENT_ROOT=/opt/libevent -DWSLAY_ROOT=/opt/wslay -DJSONC_ROOT=/opt/json-c`
to specify the install prefix of the libraries.

## Configuration
An example configuration is provided in `src/config.json` and will be copied to
the build directory. You can use a configuration file with the `-c` option.
`~/disqueue$ disqueue -c config.json`

The configuration structure is as follows:
```javascript
{
  "servers": [
    {
      "hostname": "server hostname",
      "port": 3682,
      "security": {
        "certificate": "PEM certificate path",
        "privatekey": "PEM private key path"
      },
      "authentication": "authentication-name"
    }
  ],
  "authentication": {
    "authentication-name": {
      "type": "authentication type (plaintext)",
      "file": "authentication file"
    }
  }
}
```

## Security
See [Security.md](Security.md) for secure configurations of the server.

## Client
Interacting with the Disqueue API is possible using regular HTTP/1.1 and
WebSockets, see the [API.md](API.md) file. Using a prebuilt client will make
using Disqueue much easier. Try one of the following:
  * [clients/python](clients/python)
  * [clients/typescript](clients/typescript)

## Compatibility
disqueue is tested on Microsoft Windows and Ubuntu Linux, however all operating
systems supporting libevent and the standard C library should be able to run
disuque. Bug reports for compatiblity issues with any systems meeting these
requirements are welcome.

## License
Disqueue is licensed under the MIT license. The full license text is available
in the LICENSE file. Parts of the build script are under PHP/Zend license,
details of this are in the LICENSE file.

The following files are BSD Licensed and from the NetBSD project:
  src/queue-compat.h.in
  src/getopt.h.in
  src/getopt.c
