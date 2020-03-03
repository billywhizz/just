#!/bin/bash
#docker build -t v8-build -f tools/Dockerfile.v8.debian ./tools
docker run -d --rm --name v8-build v8-build /bin/sleep 600
docker cp v8-build:/build/v8/out.gn/x64.release/obj/libv8_monolith.a ../deps/v8/libv8_monolith.a
docker cp v8-build:/build/v8/include ../deps/v8/
docker cp v8-build:/build/v8/src ../deps/v8/
docker cp v8-build:/build/v8/third_party/googletest/src/googletest/include/gtest ../deps/v8/third_party/googletest/src/googletest/include/
docker cp v8-build:/build/v8/testing ../deps/v8/
docker cp v8-build:/build/v8/out.gn/x64.release/gen ../deps/v8/