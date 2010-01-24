all: Tcl.html

Tcl.html: jim_man.txt
	asciidoc -o $@ -d manpage $^ || cp Tcl.html.supplied $@
