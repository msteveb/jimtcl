# Find and load the Jim tcltest wrapper
if {[catch {info version}]} {
	# Tcl
	source [file dirname [info script]]/../tcltest.tcl
} else {
	# Jim
	if {[exists env(TOPSRCDIR)]} {
		set auto_path [list $env(TOPSRCDIR) {*}$auto_path]
	}

	package require tcltest
}
