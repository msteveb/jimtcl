jimsh ?= ../jimsh

test:
	@set -e; for i in *.test; do LD_LIBRARY_PATH=..:$(LD_LIBRARY_PATH) $(jimsh) $$i; done

clean:
	rm -f gorp.file2 cat gorp.file sleep exit wc sh echo test1
