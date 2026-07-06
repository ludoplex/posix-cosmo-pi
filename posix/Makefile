# posix/Makefile — build the two Cosmopolitan APE binaries.
# Requires the cosmocc toolchain on PATH: https://cosmo.zip/pub/cosmocc/cosmocc.zip

CC      ?= cosmocc
CFLAGS  ?= -O2 -Wall -Wextra
BIN     := bin
SRC     := src
BINS    := $(BIN)/jsonx.com $(BIN)/llm_bridge.com

.PHONY: all test clean
all: $(BINS)

$(BIN)/%.com: $(SRC)/%.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

test: all
	sh test/run-tests.sh

clean:
	rm -f $(BINS) $(BIN)/*.aarch64.elf $(BIN)/*.com.dbg
