# Just

A v8 javascript runtime for linux that aims to be small, fast and simple to understand

## Key Points and Goals

- currently only tested and building on x64/ubuntu 18.04 or debian:stretch docker image
- static binary that should run on most modern linuxes. tested on
  - alpine 3.10, 3.11
  - debain:stretch
  - ubuntu 18.04
- no shared library support. all binary dependencies must be baked in at compile time
- commonjs require support for js only
- 13 MB binary
- 8-9 MB RSS usage on startup
- 12-13 MB RSS usage under heavy load on minimal network server
- minimal heap allocations. standard library should not cause gc

## Files

### just.cc

main entry point for building the runtime. embedders can roll their own and include just.h

### just.h

the just runtime as a single header. any non-essential apis are in separate modules

## Line Count

```
===============================================================================
count time : 2020-02-29 05:21:22
count workspace : /home/andrew/Documents/source/cpp/just
total files : 6
total code lines : 2092
total comment lines : 46
total blank lines : 172

just.cc, code is 39, comment is 5, blank is 7.
just.h, code is 1503, comment is 40, blank is 120.
just.js, code is 319, comment is 1, blank is 19.
signal.h, code is 77, comment is 0, blank is 9.
thread.h, code is 81, comment is 0, blank is 10.
udp.h, code is 73, comment is 0, blank is 7.
===============================================================================
```
