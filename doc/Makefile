all: Tcl.html

Tcl.html: jim_tcl.txt
	asciidoc -o $@ -d manpage $^ || cp Tcl.html.supplied $@
