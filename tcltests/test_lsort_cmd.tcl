set list {b d a c z}

proc sorter {a v1 v2} {
	set ::arg $a
	return [string compare $v1 $v2]
}

proc test_lsort_cmd {test cmd list exp} {
	lsort -command $cmd $list
	if {$::arg != $exp} {
		error "$test: Failed"
	}
}
test_lsort_cmd lsort.cmd.1 "sorter arg1" $list "arg1"
test_lsort_cmd lsort.cmd.2 {sorter "arg with space"} $list "arg with space"
test_lsort_cmd lsort.cmd.3 [list sorter [list arg with list "last with spaces"]] $list [list arg with list "last with spaces"]
