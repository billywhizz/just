CC             = clang++
C              = clang

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

zlib: ## zlib library
	cd deps/zlib
	./configure --static
	cd ../../
	make -C deps/zlib/ clean
	make -C deps/zlib/ static

mbedtls: ## mbedtls library
	make -C deps/mbedtls/ lib

runtime: builtins.h deps/mbedtls/library/libmbedcrypto.a ## build runtime
	$(C) -c -DV8_COMPRESS_POINTERS -I./deps/picohttpparser -O3 -Wall -Wextra -march=native -mtune=native -msse4 deps/picohttpparser/picohttpparser.c
	$(CC) -c -DV8_COMPRESS_POINTERS -I. -I ./deps/zlib -I./deps/v8/include -I./deps/picohttpparser -I./deps/mbedtls/include -O3 -march=native -mtune=native -Wall -Wextra -flto -Wno-unused-parameter just.cc
	$(CC) -s -static -flto -pthread -m64 -Wl,--start-group ./deps/v8/libv8_monolith.a deps/mbedtls/library/libmbedcrypto.a deps/zlib/libz.a picohttpparser.o just.o -Wl,--end-group -o just

runtime-debug: builtins.h deps/mbedtls/library/libmbedcrypto.a ## build runtime
	$(C) -c -DV8_COMPRESS_POINTERS -I./deps/picohttpparser -g -Wall -Wextra -march=native -mtune=native -msse4 deps/picohttpparser/picohttpparser.c
	$(CC) -c -DV8_COMPRESS_POINTERS -I. -I ./deps/zlib -I./deps/v8/include -I./deps/picohttpparser -I./deps/mbedtls/include -g -march=native -mtune=native -Wall -Wextra -flto -Wno-unused-parameter just.cc
	$(CC) -flto -pthread -m64 -Wl,--start-group ./deps/v8/libv8_monolith.a deps/mbedtls/library/libmbedcrypto.a deps/zlib/libz.a picohttpparser.o just.o -Wl,--end-group -o just

clean: ## tidy up
	rm -f builtins.h
	rm -f *.o
	rm -f just

clean-all: ## clean deps too
	rm -f builtins.h
	rm -f *.o
	rm -f just
	make -C deps/zlib clean
	make -C deps/mbedtls clean
	
.DEFAULT_GOAL := help
