jimsh ?= ../jimsh

test:
	@for i in *.test; do LD_LIBRARY_PATH=..:$(LD_LIBRARY_PATH) $(jimsh) $$i; done

clean:
	rm -f gorp.file2 gorp.file sleepx test1 exec.tmp1
