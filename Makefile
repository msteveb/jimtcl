.SUFFIXES:
.SUFFIXES: .c .so .xo .o .dll

SHELL= /bin/sh
LD= ld
LDFLAGS= $(PROFILE)
CFLAGS= -Wall -W -O2 -g $(PROFILE)
AR=/usr/bin/ar
RANLIB=/usr/bin/ranlib
LIBPATH=-L.
INSTALL= /usr/bin/install
INSTALL_PROGRAM= $(INSTALL)
INSTALL_DATA= $(INSTALL) -m 644
DESTDIR= /usr/local/bin/

PROGRAMS= 		jim
JIM_OBJECTS=		jim.o jimsh.o

stopit:
	@echo "Use:"
	@echo "make jim        - to build the Jim interpreter"
	@echo "make extensions - to build all the dynamic loadable extensions"
	@echo "make posix      - to build only the posix extension"

all: $(DEFAULT_BUILD)

profile:
	@$(MAKE) jim PROFILE=-p

.c.o:
	$(CC) -I. $(CFLAGS) $(DEFS) -c $< -o $@

.xo.so:
	rm -f $@
	$(LD) -G -z text -o $@ $< -ldl -lc

.c.xo:
	$(CC) -I. $(CFLAGS) $(DEFS) -fPIC -c $< -o $@

.o.dll:
	$(CC) -shared -o $@ $<

jim: $(JIM_OBJECTS)
	$(CC) $(LDFLAGS) -o jim $(JIM_OBJECTS) -ldl

posix: jim-posix.so
win32: jim-win32.dll jim-win32com.dll
extensions: posix

clean:
	rm -f *.o *.so *.dll core .depend .*.swp gmon.out $(PROGRAMS)

test: jim
	./jim test.tcl
	./jim regtest.tcl

bench: jim
	./jim bench.tcl

# Dependences
jim.o: jim.c jim.h
