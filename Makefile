CC             = clang++
C              = clang
CCFLAGS        = -march=native -mtune=native -I./deps/v8/include -I./deps/picohttpparser -O3 -Wall -Wextra -flto -Wno-unused-parameter
LDADD          = -s -static -flto -pthread -m64 -Wl,--start-group ./deps/v8/libv8_monolith.a picohttpparser.o just.o -Wl,--end-group
CCFLAGSDBG     = -I./deps/v8/include -I./deps/picohttpparser -g -Wall -Wextra -flto -Wno-unused-parameter
LDADDDBG       = -flto -pthread -m64 -Wl,--start-group ./deps/v8/libv8_monolith.a picohttpparser.o just.o -Wl,--end-group

.PHONY: help clean

help:
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z_-]+:.*?## / {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

builtins.h: just.js ## compile builtin js
	xxd -i just.js > builtins.h
	sed -i 's/unsigned char/static const char/g' builtins.h
	sed -i 's/unsigned int/static unsigned int/g' builtins.h
	sed -i 's/examples_//g' builtins.h

runtime: builtins.h ## build runtime
	$(C) -c $(CCFLAGS) -msse4 deps/picohttpparser/picohttpparser.c
	$(CC) -c $(CCFLAGS) just.cc
	$(CC) $(LDADD) -o just

runtime-debug: builtins.h ## build runtime debug version
	$(C) -c $(CCFLAGSDBG) -msse4 deps/picohttpparser/picohttpparser.c
	$(CC) -c $(CCFLAGSDBG) just.cc
	$(CC) $(LDADDDBG) -o just

clean: ## tidy up
	rm -f builtins.h
	rm -f *.o
	rm -f just

.DEFAULT_GOAL := help
