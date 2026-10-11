# Maculator — build & test
#
# Cross-compile from macOS:   brew install mingw-w64
# Cross-compile from Linux:   apt install mingw-w64
# Override the compiler:      make MINGW_CC=zig\ cc\ -target\ x86_64-windows

MINGW_CC ?= x86_64-w64-mingw32-gcc
CC       ?= cc

Calculator.exe: src/calculator.c
	$(MINGW_CC) -Os -s -mwindows -o $@ $< -lgdi32 -luser32

test: build/calc_test
	./build/calc_test

# The test harness compiles the real source with Win32 stubs (tests/calc_stub.h)
# and replays the behavioral matrix observed on the genuine app.
build/calc_test: tests/calc_test.c src/calculator.c tests/calc_stub.h | build
	$(CC) -DCALC_TEST -Isrc -Itests -o $@ tests/calc_test.c

# Tiny probe binary used during reverse engineering:
# traced under LLDB with `rea observe-native-calls` to confirm % semantics.
tools/percent_probe: tools/percent_probe.c
	$(CC) -O2 -o $@ $<

build:
	mkdir -p build

clean:
	rm -rf build Calculator.exe tools/percent_probe

.PHONY: test clean
