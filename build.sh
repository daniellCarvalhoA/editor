#!/bin/bash
# mkdir -p build
# pushd build

LOCKFILE="/tmp/e.lock"

touch $LOCKFILE

CommonFlags="-DDEBUG -Wall -Werror -g -Wextra -Wno-unused-function -fsanitize=address -Wno-unused-parameter  -DCOMPILER_GCC=1 -std=gnu99 -D_GNU_SOURCE"

# -fsanitize=address 


gcc $CommonFlags -fPIC -shared e.c -lutf8proc -o build/e.so
rm -f $LOCKFILE

export ASAN_OPTIONS=abort_on_error=1
export ASAN_OPTIONS=handle_segv=0

gcc $CommonFlags e_platform.c -o build/e 
gcc $CommonFlags -DTESTS=1 -fPIC -shared -lutf8proc tests.c -o build/tests.so
gcc $CommonFlags -DTESTS=1 test_runner.c  -pthread -ldl -o build/tests 

# popd



