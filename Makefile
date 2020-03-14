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
	xxd -i lib/wasm.js >> builtins.h
	xxd -i lib/libwabt.min.js >> builtins.h
	sed -i 's/unsigned char/const char/g' builtins.h
	sed -i 's/unsigned int/unsigned int/g' builtins.h

zlib: ## zlib library
	cd deps/zlib &&	./configure --static && cd ../../
	make -C deps/zlib/ clean
	make -C deps/zlib/ static

openssl: ## openssl library
	cd deps/openssl-1.1.1d &&	./config --release --with-zlib-include=$(pwd)/deps/zlib --with-zlib-lib=$(pwd)/deps/zlib/ no-autoload-config no-autoerrinit no-autoalginit no-afalgeng no-deprecated no-capieng no-cms no-comp no-dgram no-dynamic-engine enable-ec_nistp_64_gcc_128 no-err no-filenames no-gost no-hw-padlock no-makedepend no-multiblock no-nextprotoneg no-pic no-rfc3779 no-shared no-srp no-srtp no-static-engine no-stdio no-tests threads no-ui zlib no-ssl no-tls1 no-dtls no-aria no-bf no-blake2 no-camellia no-cast no-chacha no-cmac no-des no-idea no-mdc2 no-ocb no-poly1305 no-rc2 no-scrypt no-seed no-siphash no-sm3 no-sm4 no-whirlpool
	make -C deps/openssl-1.1.1d/ build_generated
	make -C deps/openssl-1.1.1d/ libssl.a libcrypto.a

runtime: builtins.h deps/openssl-1.1.1d/libcrypto.a deps/openssl-1.1.1d/libssl.a ## build runtime
	$(C) -c -DV8_COMPRESS_POINTERS -I./deps/picohttpparser -O3 -Wall -Wextra -march=native -mtune=native -msse4 deps/picohttpparser/picohttpparser.c
	$(CC) -c -DV8_COMPRESS_POINTERS -I. -I ./deps/zlib -I./deps/v8/include -I./deps/picohttpparser -I./deps/openssl-1.1.1d/include -O3 -march=native -mtune=native -Wall -Wextra -flto -Wno-unused-parameter just.cc
	$(CC) -s -static-libgcc -flto -pthread -m64 -Wl,--start-group ./deps/v8/libv8_monolith.a deps/openssl-1.1.1d/libssl.a deps/openssl-1.1.1d/libcrypto.a deps/zlib/libz.a picohttpparser.o just.o -Wl,--end-group -ldl -o just

runtime-debug: builtins.h deps/openssl-1.1.1d/libcrypto.a deps/openssl-1.1.1d/libssl.a ## build runtime
	$(C) -c -DV8_COMPRESS_POINTERS -I./deps/picohttpparser -g -Wall -Wextra -march=native -mtune=native -msse4 deps/picohttpparser/picohttpparser.c
	$(CC) -c -DV8_COMPRESS_POINTERS -I. -I ./deps/zlib -I./deps/v8/include -I./deps/picohttpparser -I./deps/openssl-1.1.1d/include -g -march=native -mtune=native -Wall -Wextra -flto -Wno-unused-parameter just.cc
	$(CC) -flto -pthread -m64 -Wl,--start-group ./deps/v8/libv8_monolith.a  deps/openssl-1.1.1d/libcrypto.a deps/openssl-1.1.1d/libssl.a deps/zlib/libz.a picohttpparser.o just.o -Wl,--end-group -ldl -o just

clean: ## tidy up
	rm -f builtins.h
	rm -f *.o
	rm -f just

clean-all: ## clean deps too
	rm -f builtins.h
	rm -f *.o
	rm -f just
	make -C deps/openssl-1.1.1d clean
	
.DEFAULT_GOAL := help
