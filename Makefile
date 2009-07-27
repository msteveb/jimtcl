RANLIB ?= ranlib

# Configuration

jim_nofork := 

# Defines the extensions to include
EXTENSIONS := 

ALL_EXTENSIONS := aio clock eventloop exec file posix readdir readline regexp sdl sqlite sqlite3

# Set an initial, default library and auto_path
CFLAGS += -DTCL_LIBRARY=\"/lib/tcl6\"

STATIC_LIBTCL := libtcl6.a
CFLAGS += -Wall -g -Os -I.  -DHAVE_UALARM

.EXPORT_ALL_VARIABLES:

OBJS := jim-load.o jim-package.o jim-subcmd.o jim-interactive.o jim.o

SDKHDRS := jim.h jim-subcmd.h

EXTENSIONS_OBJS := $(patsubst %,jim-%.o,$(EXTENSIONS))

# Emulate tinytcl
LIBJIM := libtcl6.a

ifdef jim_nofork
	CFLAGS += -DNO_FORK
endif

OBJS += load_extensions.o

TARGETS += jimsh $(LIBJIM)

all: $(TARGETS)
	#$(MAKE) -C doc all

jimsh: $(LIBJIM) jimsh.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBJIM) $(LDLIBS) $(LIBDL)

$(LIBJIM): $(OBJS) $(EXTENSIONS_OBJS)
	$(AR) cr $@ $^
	$(RANLIB) $@

load_extensions.c:
	sh make-load-extensions.sh $@ $(EXTENSIONS)

install:
	install -d $(DESTDIR)/lib/tcl6
	install -m 644 array.tcl glob.tcl stdio.tcl tcl6.tcl $(DESTDIR)/lib/tcl6

# REVISIT: Should we just provide source and no binaries?
sdk: install
	install -d $(DESTDIR)/lib $(DESTDIR)/include
	install -m 644 $(STATIC_LIBTCL) $(DESTDIR)/lib
	install -m 644 $(SDKHDRS) $(DESTDIR)/include

clean:
	rm -f *.o lib*.a $(TARGETS) load_extensions.c
