# Environment

## count.js

counts the bytes piped to stdin. uses the event loop and non blocking sockets
- 40 Gbit/sec on stdin

## count-sync.js

synchronous version of count.js. no event loop. just standard blocking calls
- 40 Gbit/sec on stdin

## server.js

dumb tcp web server for benchmarking. uses event loop, sockets, timers
- 245k non pipelined rps

## unixserver.js

dumb unix domain socket web server for benchmarking. uses event loop, sockets, timers
- 417k non pipelined rps

## client.js

tcp http client to stress the server with 128 clients
- 245k non pipelined rps

## unixclient.js

unix domain socket http client to stress the server with 128 clients
- 417k non pipelined rps

## read.js

synchronous reading of /dev/zero from filesystem
- 147 Gbit/

## starttime.js

displays the time to js being active
- 4ms for initial isolate
- 1.5ms for subsequent isolates

## httpd.js

tcp http server using picohttpparser
- 235k non pipelined rps with headers parsed
- 130k non pipelined rps with headers fully converted to JS

## httpc.js

tcp http client using picohttpparser
- 235k non pipelined rps with headers parsed
- 130k non pipelined rps with headers fully converted to JS

## thread.js

- spawns and waits for as many threads as possible in a tight loop
- processes event loop between runs

## thread-ipc.js

- spawns a thread with a shared buffer and a socketpair fd
- sends data over the pipe to the thread
- makes atomic reads and writes on the shared buffer

## child.js

- spawn a child process and read stdin and stderr

## loop.js

- run multiple event loops

## http-simple-client.js

- synchronous http get request

## udp.js

- sending and receiving inet dgrams