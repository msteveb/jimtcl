JIMSH ?= ../jimsh
LD_LIBRARY_PATH := ..:$(LD_LIBRARY_PATH)

test: $(JIMSH)
	@set -e; for i in *.test; do $(JIMSH) $$i; done

../jimsh: ../*.c
	make -C .. all

clean:
	rm -f gorp.file2 cat gorp.file sleep exit wc sh echo
