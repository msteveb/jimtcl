# Find and load the Jim tcltest wrapper
if {[catch {info version}]} {
	# Tcl
	source [file dirname [info script]]/../tcltest.tcl
} else {
	# Jim
	package require tcltest
}
