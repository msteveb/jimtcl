TCLSH := $(shell which jimsh)
ifeq ($(TCLSH),)
TCLSH := $(shell which tclsh)
endif

all: Tcl.html

Tcl.html: jim_tcl.txt make-index
	$(TCLSH) make-index < jim_tcl.txt | asciidoc -o $@ -d manpage -
