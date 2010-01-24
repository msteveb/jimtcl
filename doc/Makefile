all: Tcl.html

Tcl.html: jim_tcl.txt make-index
	./make-index < jim_tcl.txt | asciidoc -o $@ -d manpage -
