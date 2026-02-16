CROSS ?= m68k-atari-mintelf-
CC := $(CROSS)gcc
STRIP := $(CROSS)strip

CFLAGS ?= -std=c11 -O2 -Wall -Wextra
LDFLAGS ?=

SOURCES := $(wildcard *.c) $(wildcard syscalls/*.c)
PROGRAMS := $(SOURCES:.c=.tos)

.PHONY: all clean check-toolchain

all: check-toolchain $(PROGRAMS)

check-toolchain:
	@command -v "$(CC)" >/dev/null 2>&1 || { echo "Missing compiler: $(CC)"; exit 1; }
	@command -v "$(STRIP)" >/dev/null 2>&1 || { echo "Missing strip: $(STRIP)"; exit 1; }

%.tos: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
	$(STRIP) $@

clean:
	rm -f $(PROGRAMS)
