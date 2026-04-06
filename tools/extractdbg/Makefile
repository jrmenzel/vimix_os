CC = gcc
BASE_CFLAGS = -Wall -Wextra -I../../kernel/include -I../../kernel/ -DBUILD_ON_HOST -D__USE_REAL_STDC
CFLAGS_RELEASE = $(BASE_CFLAGS) -O2
CFLAGS_DEBUG = $(BASE_CFLAGS) -O0 -g3
LDFLAGS = -ldw -lelf

SRC = extractdbg.c
BIN = extractdbg

.PHONY: all release debug clean

all: release

release: CFLAGS = $(CFLAGS_RELEASE)
release: $(BIN)

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: $(BIN)

$(BIN): $(SRC)
	@printf "Compile: $(@)\n"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(BIN)
