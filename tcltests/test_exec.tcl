if {[info commands exec] eq ""} {
	return "noimpl"
}
if {[info commands verbose] eq ""} {
	proc verbose {msg} {puts $msg}
}

set infile [open Makefile]
set outfile [open exec.out w]

exec cat <@$infile >@$outfile
close $infile
close $outfile

exec cmp -s Makefile exec.out

file delete exec.out
