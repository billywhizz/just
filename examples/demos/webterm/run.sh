#!/bin/bash
#docker run -it --rm -v $(pwd):/webterm -p 80:8888 -v $(pwd)/../../../just:/bin/just --workdir=/webterm busybox:latest /bin/just server.js
docker run -it --rm -v $(pwd):/webterm -p 80:8888 -v $(pwd)/../../../just:/bin/just --workdir=/webterm gcr.io/distroless/static:latest /bin/just server.js