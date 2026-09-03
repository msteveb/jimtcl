#!/bin/sh -e
all: vi

CFLAGS := \
       -pedantic -Wall -Wextra \
       -Wno-implicit-fallthrough \
       -Wno-missing-field-initializers \
       -Wno-unused-parameter \
       -Wfatal-errors \
       ${CFLAGS}

CFLAGS += -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
CFLAGS += -MMD

CC = c99

prefix = /usr/local
bindir := ${prefix}/bin

install: vi
	mkdir -p "${DESTDIR}${bindir}/"
	cp -f vi "${DESTDIR}${bindir}/vi"

-include vi.d
vi: vi.c
	${CC} ${CFLAGS} -o $@ vi.c

clean:
	-rm -f vi vi.d

pgobuild:
	${CC} ${CFLAGS} -fprofile-generate=. -o vi vi.c
	echo "qq" \
		| ./vi -v ./vi.c >/dev/null
	${CC} ${CFLAGS} -fprofile-use=. -o vi vi.c
	rm *.gcda

fetch:
	@ if ! git diff --quiet HEAD; then \
		echo "Please stash changes before fetching." ; \
		exit 1; \
	fi
	git switch -c upstream-temp
	git pull https://github.com/kyx0r/nextvi
	git switch master
	git rebase --rebase-merges upstream-temp
	git branch -D upstream-temp
	echo "Successfully fetched from upstream."

.PHONY: all clean install pgobuild fetch
