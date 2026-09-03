# Internally, open "|..." calls out to popen from tclcompat.tcl
#
# This code is compatible with Tcl

# Write to a pipe
set f [open |[list cat | sed -e "s/line/This is line/" >temp.out] w]
puts "Creating temp.out with pids: [pid $f]"
foreach n {1 2 3 4 5} {
	puts $f "line $n"
}
close $f

# Read from a pipe
set f [open "|cat temp.out"]
puts "Reading temp.out with pids: [pid $f]"
while {[gets $f buf] >= 0} {
	puts $buf
}
close $f
file delete temp.out
