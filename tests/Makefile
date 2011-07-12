jimsh ?= ../jimsh
export LD_LIBRARY_PATH := ..:$(LD_LIBRARY_PATH)

test:
	@set -e; for i in *.test; do $(jimsh) $$i; done

clean:
	rm -f gorp.file2 cat gorp.file sleep exit wc sh echo test1
