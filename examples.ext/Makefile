# Note that if cross compiling, build with:
#
#   make NOTEST=1
#
# to avoid trying to load the resulting module.
# Also note that you will need a build-host version of jimsh in the
# PATH in order to build the extension.

# Prefer jimsh in the PATH because it is more likely to be built
# for the build-host rather than the target.

ifdef NOTEST
BUILDOPTS := --notest
endif

export PATH := $(PATH):..

all: helloworld.so

helloworld.so: helloworld.c
	../build-jim-ext -I.. -L.. $(BUILDOPTS) $^

# Note: Currently we don't attempt to set LD_LIBRARY_PATH or equivalent

test:
	JIMLIB=. ../jimsh -e 'package require helloworld; hello'

clean:
	rm -f *.o *.so
