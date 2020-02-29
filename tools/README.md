# Tools

building v8 library is somewhat complicated to docker files are provided here for building libv8_monolith.a. this can then be copied to deps/v8/libv8_monolith.a in the repo so linking can work.

## Dockerfile

dockerfile for building a tiny docker image with "just" the runtime

## Dockerfile.v8.alpine

dockerfile for building v8 library on alpine/musl

## Dockerfile.v8.debian

dockerfile for building v8 library on debian stretch
