#!/bin/bash
# mkdir -p build
# pushd build

LOCKFILE="/tmp/e.lock"

touch $LOCKFILE

CommonFlags="-DDEBUG -Wall -Werror -g -Wextra  -fsanitize=address -Wno-unused-function   -DCOMPILER_GCC=1 -std=gnu99 -D_GNU_SOURCE"

# -fsanitize=address 


gcc $CommonFlags -fPIC -shared  e.c -lutf8proc -o build/e.so
rm -f $LOCKFILE

gcc $CommonFlags e_platform.c -o build/e 
# gcc $CommonFlags -fPIC -shared -lutf8proc tests.c -o build/tests.so
# gcc $CommonFlags test_runner.c -pthread -ldl -o build/tests 

# popd



