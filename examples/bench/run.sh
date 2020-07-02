#!/bin/bash
concurrency="16 32 64 128 256 512"
pipeline="256 1024 4096 16384"
for c in $concurrency
do
  for p in $pipeline
  do
  echo "conn $c pipeline $p"
  wrk -H "Host: 127.0.0.1:3000" -H "Accept: text/plain,text/html;q=0.9,application/xhtml+xml;q=0.9,application/xml;q=0.8,*/*;q=0.7" -H "Connection: keep-alive" --latency -d 30 -c $c --timeout 8 -t 4 http://127.0.0.1:3000/plaintext -s pipeline.lua -- $p
  done
done