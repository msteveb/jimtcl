jimsh ?= ../jimsh
tclsh ?= tclsh

test:
	@LD_LIBRARY_PATH=..:$(LD_LIBRARY_PATH) $(jimsh) runall.tcl

tcl:
	@rc=0; for i in *.test; do LD_LIBRARY_PATH=..:$(LD_LIBRARY_PATH) $(tclsh) -encoding utf-8 $$i || rc=$?; done; exit $$rc

clean:
	rm -f gorp.file2 gorp.file sleepx test1 exec.tmp1
