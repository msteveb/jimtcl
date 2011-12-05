# Simple example of how the history extension
# can be used to provide line editing and history

# Build jimsh with the history extension and enable line editing (the default)
# ./configure --with-ext=history

package require history

set histfile [env HOME]/.jtclsh
history load $histfile
while 1 {
	if {[history getline "jim> " cmd] < 0} {
		break
	}
	if {$cmd eq "h"} {
		history show
		continue
	}
	# Don't bother adding single char commands to the history
	if {[string length $cmd] > 1} {
		history add $cmd
		history save $histfile
	}
	# jimsh also does:
	# - check for a complete command: [info complete]
	# - handle other non-error return codes and changes the prompt: [info returncodes]
	# - displays the complete error message: [errorInfo]
	try {
		set result [eval $cmd]
		if {$result ne {}} {
			puts $result
		}
	} on error msg {
		puts $msg
	}
}
