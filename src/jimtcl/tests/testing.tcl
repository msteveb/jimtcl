# Find and load the Jim tcltest wrapper
source [file dirname [info script]]/../tcltest.tcl

# If jimsh is not installed we may also need to include top_srcdir for Tcl modules (.. from this script)
set auto_path [list [file dirname [info script]]/.. {*}$auto_path]
