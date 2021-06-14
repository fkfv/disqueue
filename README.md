# Disqueue
> Simple distributed queue service.

## Requirements
* libevent - the latest master branch of libevent does not yet support taking
   ownership of a evhttp_connections' bufferevent, so for now a fork of libevent
   must be used to build disqueue - https://github.com/fkfv/libevent

* wslay - wslay is used to provide websocket support -
    https://github.com/tatsuhiro-t/wslay

To instruct the build script on where to find these libraries, you can set
`-DLIBEVENT_ROOT=/opt/libevent -DWSLAY_ROOT=/opt/wslay` to specify the install
prefix of the libraries.

## Compatibility
disqueue is tested on Microsoft Windows and Ubuntu Linux, however all operating
systems supporting libevent and the standard C library should be able to run
disuque. Bug reports for compatiblity issues with any systems meeting these
requirements are welcome.

## License
Disqueue is licensed under the MIT license. The full license text is available
in the LICENSE file. Parts of the build script are under PHP/Zend license,
details of this are in the LICENSE file.
