CC             = clang++
C              = clang
CCFLAGS        = -DV8_COMPRESS_POINTERS -I. -I./deps/v8/include -I./deps/picohttpparser -O3 -Wall -Wextra -flto -Wno-unused-parameter -march=native -mtune=native
LDADD          = -s -static -flto -pthread -m64 -Wl,--start-group ./deps/v8/libv8_monolith.a picohttpparser.o just.o -Wl,--end-group
CCFLAGSDBG     = -DV8_COMPRESS_POINTERS -I. -I./deps/v8/include -I./deps/picohttpparser -g -Wall -Wextra -flto -Wno-unused-parameter
LDADDDBG       = -flto -pthread -m64 -Wl,--start-group ./deps/v8/libv8_monolith.a picohttpparser.o just.o -Wl,--end-group

.PHONY: help clean

help:
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z_-]+:.*?## / {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

builtins.h: ## compile builtin js
	xxd -i just.js > builtins.h
	xxd -i lib/websocket.js >> builtins.h
	xxd -i lib/inspector.js >> builtins.h
	xxd -i lib/loop.js >> builtins.h
	xxd -i lib/require.js >> builtins.h
	xxd -i lib/path.js >> builtins.h
	xxd -i lib/repl.js >> builtins.h
	xxd -i lib/fs.js >> builtins.h
	sed -i 's/unsigned char/const char/g' builtins.h
	sed -i 's/unsigned int/unsigned int/g' builtins.h

mbedtls:
	make -C deps/mbedtls/ lib

runtime: builtins.h ## build runtime
	$(C) -c $(CCFLAGS) -msse4 deps/picohttpparser/picohttpparser.c
	$(CC) -c $(CCFLAGS) just.cc
	$(CC) $(LDADD) -o just

runtime-tls: builtins.h deps/mbedtls/library/libmbedcrypto.a ## build runtime
	$(C) -c $(CCFLAGS) -msse4 deps/picohttpparser/picohttpparser.c
	$(CC) -c $(CCFLAGS) -I./deps/mbedtls/include just.cc
	$(CC) $(LDADD) deps/mbedtls/library/libmbedcrypto.a -o just

runtime-debug: builtins.h ## build runtime debug version
	$(C) -c $(CCFLAGSDBG) -msse4 deps/picohttpparser/picohttpparser.c
	$(CC) -c $(CCFLAGSDBG) just.cc
	$(CC) $(LDADDDBG) -o just

runtime-tls-debug: builtins.h deps/mbedtls/library/libmbedcrypto.a ## build runtime
	$(C) -c $(CCFLAGSDBG) -msse4 deps/picohttpparser/picohttpparser.c
	$(CC) -c $(CCFLAGSDBG) -I./deps/mbedtls/include just.cc
	$(CC) $(LDADDDBG) deps/mbedtls/library/libmbedcrypto.a -o just

clean: ## tidy up
	rm -f builtins.h
	rm -f *.o
	rm -f just

.DEFAULT_GOAL := help
