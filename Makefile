# Jim makefile
#
# This is a simple Makefile as it is expected that most users are likely
# to embed Jim directly into their current build system. Jim is able to
# make use of dynamically loaded extensions on unix provided you have the
# dl library available. If not, set JIM_LIBS= on the make command line.
#
# make CC=gcc jim         builds a standard Jim binary using gcc.
# make CC=gcc LIBS= jim   avoids attempts to link in libdl.a
#
#

.SUFFIXES:
.SUFFIXES: .c .so .xo .o .dll

SHELL   = /bin/sh
RM      = rm -f
OPT     = -O2
LDFLAGS = $(PROFILE)
CFLAGS  = -Wall -Wwrite-strings -W $(OPT) -g $(PROFILE)
AR      = /usr/bin/ar
RANLIB  = /usr/bin/ranlib
LIBPATH =-L.
INSTALL = /usr/bin/install
INSTALL_PROGRAM= $(INSTALL)
INSTALL_DATA= $(INSTALL) -m 644
DESTDIR = /usr/local/bin/

PROGRAMS    = jim
JIM_OBJECTS = jim.o jimsh.o
LIBS        = -ldl

stopit:
	@echo "Use:"
	@echo "make jim        - to build the Jim interpreter"
	@echo "make extensions - to build the aio,posix and sdl extensions"
	@echo "make posix      - to build only the posix extension"
	@echo "make aio        - to build only the ANSI I/O extension"
	@echo "make sdl        - to build only the SDL extension"

all: $(DEFAULT_BUILD)

profile:
	@$(MAKE) clean jim PROFILE=-pg

.c.o:
	$(CC) -I. $(CFLAGS) $(DEFS) -c $< -o $@

.xo.so:
	rm -f $@
	$(LD) -G -z text -o $@ $< -ldl -lc

.c.xo:
	$(CC) -I. $(CFLAGS) $(DEFS) -fPIC -c $< -o $@

.o.dll:
	$(CC) -shared -o $@ $<

jim-win32com.dll: jim-win32com.o
	$(CC) -shared -o $@ $< -lole32 -luuid -loleaut32

jim-sdl.xo: jim-sdl.c
	$(CC)  `sdl-config --cflags` -I. $(CFLAGS) $(DEFS) -fPIC -c $< -o $@

jim-sdl.so: jim-sdl.xo
	rm -f $@
	$(LD) -G -z text -o $@ $< -ldl -lc -L/usr/local/lib -lSDL -lSDL_gfx -lpthread

jim: $(JIM_OBJECTS)
	$(CC) $(LDFLAGS) -o jim $(JIM_OBJECTS) $(LIBS)

posix: jim-posix.so
aio: jim-aio.so
sdl: jim-sdl.so
win32: jim-win32.dll jim-win32com.dll
extensions: posix aio sdl

clean:
	$(RM) *.o *.so *.dll *.xo core .depend .*.swp gmon.out $(PROGRAMS)

test: jim
	./jim test.tcl
	./jim regtest.tcl

bench: jim
	./jim bench.tcl

dep:
	gcc -MM *.[ch] 2> /dev/null

TAGS: jim.h jim.c jim-posix.c jim-win32.c jim-win32com.c
	etags -o $@ $^

wc:
	wc -l jim.[ch]
	wc -l *.[ch]

clog:
	cvs2cl

commit:
	cvs2cl
	cvs commit

update:
	cvs update
	cvs2cl

bak:
	cp -f jim.c jim.c.orig
	cp -f jimsh.c jimsh.c.orig
	cp -f jim.h jim.h.orig

# Dependences
jim-aio.o: jim-aio.c jim.h
jim-posix.o: jim-posix.c jim.h
jim-sdl.o: jim-sdl.c jim.h
jim-win32com.o: jim-win32com.c jim.h
jim.o: jim.c jim.h
jimsh.o: jimsh.c jim.h
