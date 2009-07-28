lappend ::auto_path [pwd]

set v [package require testmod]

check "package version" $v 2.0
check "testmod #1" [testmod 1] 1
check "testmod #2" [testmod 2] 2
