#!/bin/bash
# mkdir -p build
# pushd build

LOCKFILE="/tmp/e.lock"

touch $LOCKFILE

CommonFlags="-DDEBUG -Wall -Werror -g -Wextra -Wno-unused-function -Wno-unused-parameter -DCOMPILER_GCC=1 -std=gnu99 -D_GNU_SOURCE"
# -fsyntax-only 
# -fsanitize=address 


gcc $CommonFlags -fPIC -shared e.c -lutf8proc -o build/e.so -lm
rm -f $LOCKFILE

export ASAN_OPTIONS=abort_on_error=1
export ASAN_OPTIONS=handle_segv=0

GLFlags="$(pkg-config --cflags --libs glfw3)  $(pkg-config --cflags --libs freetype2)"

gcc $CommonFlags gl.c e_glfw_linux_platform.c -o build/glfw_e -lm $GLFlags
gcc $CommonFlags e_linux_terminal_platform.c -o build/e  -lm
gcc $CommonFlags -DTESTS=1 -fPIC -shared  tests.c -lutf8proc -o build/tests.so -lm
gcc $CommonFlags -DTESTS=1 test_runner.c  -pthread -ldl -o build/tests  -lm

# popd



