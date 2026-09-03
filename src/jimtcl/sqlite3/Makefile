# Builds the full sqlite3 extension for Jim Tcl with the sqlite3 amalgamation

all: sqlite.so

SQLITE3_OPTS := -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_THREADSAFE=0 \
	-DSQLITE_ENABLE_LOCKING_STYLE=0 -DSQLITE_OMIT_INCRBLOB -DSQLITE_OMIT_SHARED_CACHE

sqlite.so: jim-sqlite.c sqlite3.c
	./build-ext -Wall -o $@ -I.. -L.. $(SQLITE3_OPTS) $(BUILDOPTS) $^

clean:
	rm -f *.o *.so

# Note that this will only work when not cross compiling
test: sqlite.so
	../jimsh test-sqlite.tcl
