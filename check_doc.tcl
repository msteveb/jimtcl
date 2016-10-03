# Extracts the md5sum tag attached to the end of Tcl_shipped.html
#

set fp [open "Tcl_shipped.html" r]
set content [read $fp]
close $fp

if {[regexp {<!-- md5 ([A-Za-z0-9]+  jim_tcl.txt) -->} $content matched md5sum_value]} {
	puts $md5sum_value
	exit 0
}

exit -1