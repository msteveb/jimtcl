set ret2 ""
set res2 ""

set progress ""

set ret1 [catch -signal {
	append progress a
	set ret2 [catch {
		append progress b
		signal handle TERM
		signal throw -TERM
		append progress c
	} res2]
	append progress d
} res1]

check signal.1 $progress ab
check signal.2 $ret1 5
check signal.3 $ret2 ""
check signal.4 $res1 SIGTERM
check signal.5 $res2 ""

set result 0
catch -signal {
	signal handle ALRM
	alarm 1
	sleep 2
	set result 1
} ret

check signal.7 $result 0
check signal.6 $ret SIGALRM
