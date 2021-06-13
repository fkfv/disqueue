# Disqueue
> Simple distributed queue service.

## Requirements
The event loop of the program is powered by libevent. A system installation
of libevent can be used, it must be found by CMake. Set LIBEVENT_ROOT to help
CMake find the libevent library.

An example build is presented below, assuming libevent is installed to
/opt/libevent. disqueue will be installed to /usr/local/ by default

```
disqueue       $ mkdir build && cd build
disqueue/build $ cmake -DLIBEVENT_ROOT=/opt/libevent ..
disqueue/build $ cmake --build .
disqueue/build $ cmake --install .
disqueue/build $ where disqueued
/usr/local/bin/disqueued
```

## Compatibility
disqueue is tested on Microsoft Windows and Ubuntu Linux, however all operating
systems supporting libevent and the standard C library should be able to run
disuque. Bug reports for compatiblity issues with any systems meeting these
requirements are welcome.

## License
Disqueue is licensed under the MIT license. The full license text is available
in the LICENSE file. Parts of the build script are under PHP/Zend license,
details of this are in the LICENSE file.

libevent is required for disqueue, the license for libevent is available at
https://github.com/libevent/libevent/blob/master/LICENSE 
