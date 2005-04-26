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
.PHONY: jim-aio-1.0.so

SHELL   = /bin/sh
RM      = rm -f
OPT     = -Os
LDFLAGS = $(PROFILE)
CFLAGS  = -Wall -Wwrite-strings -W $(OPT) -g $(PROFILE)
AR      = /usr/bin/ar
RANLIB  = /usr/bin/ranlib
LIBPATH =-L.
INSTALL = /usr/bin/install
INSTALL_PROGRAM= $(INSTALL)
INSTALL_DATA= $(INSTALL) -m 644
DESTDIR = /usr/local/bin/

PROGRAMS    = jim jim.exe
JIM_OBJECTS = jim.o jimsh.o
LIBS        = -ldl

stopit:
	@echo "Use:"
	@echo "make jim       - to build the Jim interpreter"
	@echo "---"
	@echo "make eventloop - to build only the event loop extension (.SO)"
	@echo "make aio       - to build only the ANSI I/O extension (.SO)"
	@echo "make aio-dll   - to build only the ANSI I/O extension (.DLL)"
	@echo "---"
	@echo "make unix-ext  - to build the AIO, POSIX and SDL extensions"
	@echo "make posix     - to build only the POSIX extension"
	@echo "make sdl       - to build only the SDL extension"
	@echo "make readline  - to build only the READLINE extension"
	@echo "---"
	@echo "make win32-ext - to build the WIN32 and WIN32COM extensions"
	@echo "make win32     - to build only the WIN32 extension"
	@echo "make win32com  - to build only the WIN32COM extension"
	@echo ""
	@echo "Note, if 'make jim' does not work try 'make jim LIBS=\"\"'"
	@echo ""
	@echo "For default Jim is compiled with -Os, if you need more"
	@echo "speed try: 'make OPT=\"-O3 -fomit-frame-pointer\"' but"
	@echo "this will result in a much larger binary."

all: $(DEFAULT_BUILD)

profile:
	@$(MAKE) clean jim PROFILE=-pg

.c.o:
	$(CC) -I. $(CFLAGS) $(DEFS) -c $< -o $@

.c.xo:
	$(CC) -I. $(CFLAGS) $(DEFS) -fPIC -c $< -o $@

jim-win32-1.0.dll: jim-win32.o
	$(CC) -shared -o $@ $<

jim-aio-1.0.dll: jim-aio.o
	$(CC) -shared -o $@ $<

jim-win32com-1.0.dll: jim-win32com.o
	$(CC) -shared -o $@ $< -lole32 -luuid -loleaut32

jim-aio-1.0.so: jim-aio.xo
	$(LD) -G -z text -o $@ $< $(LIBS) -lc

jim-posix-1.0.so: jim-posix.xo
	$(LD) -G -z text -o $@ $< $(LIBS) -lc

jim-eventloop-1.0.so: jim-eventloop.xo
	$(LD) -G -z text -o $@ $< $(LIBS) -lc

jim-udp-1.0.so: jim-udp.xo
	$(LD) -G -z text -o $@ $< $(LIBS) -lc

jim-sqlite-1.0.so: jim-sqlite.xo
	$(LD) -G -z text -o $@ $< $(LIBS) -lc -lsqlite

jim-readline-1.0.so: jim-readline.xo
	$(LD) -G -z text -o $@ $< $(LIBS) -lc -lreadline

jim-sdl.xo: jim-sdl.c
	$(CC)  `sdl-config --cflags` -I. $(CFLAGS) $(DEFS) -fPIC -c $< -o $@

jim-sdl-1.0.so: jim-sdl.xo
	rm -f $@
	$(LD) -G -z text -o $@ $< $(LIBS) -lc -L/usr/local/lib -lSDL -lSDL_gfx -lpthread

jim: $(JIM_OBJECTS)
	$(CC) $(LDFLAGS) -o jim $(JIM_OBJECTS) $(LIBS)

readline: jim-readline-1.0.so
posix: jim-posix-1.0.so
eventloop: jim-eventloop-1.0.so
udp: jim-udp-1.0.so
sqlite: jim-sqlite-1.0.so
aio: jim-aio-1.0.so
aio-dll: jim-aio-1.0.dll
sdl: jim-sdl-1.0.so
win32: jim-win32-1.0.dll
win32com: jim-win32com-1.0.dll
unix-extensions: posix aio sdl
win32-extensions: win32 win32com

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
