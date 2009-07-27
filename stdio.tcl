# (c) 2008 Steve Bennett <steveb@workware.net.au>
#
# Implements Tcl-compatible IO commands based on the aio package
#
# Provides puts, gets, open, close, eof, flush, seek, tell

package provide stdio 1.0
catch {package require aio 1.0}

# Remove the builtin puts
rename puts ""

set stdio::stdin  [aio.open standard input]
set stdio::stdout [aio.open standard output]
set stdio::stderr [aio.open standard error]
set stdio::std_channel_map [list stdin ${stdio::stdin} stdout ${stdio::stdout} stderr ${stdio::stderr}]

proc stdio::std_channel {channel} {
	return [string map ${::stdio::std_channel_map} $channel]
}

proc puts {channel args} {
	set nonewline 0
	if {$channel eq "-nonewline"} {
		set nonewline 1
		set channel [lindex $args 0]
		set args [lrange $args 1 end]
	}
	if {[llength $args] == 0} {
		set args [list $channel]
		set channel stdout
	}

	set channel [stdio::std_channel $channel]

	if {$nonewline} {
		$channel puts -nonewline {expand}$args
	} else {
		$channel puts {expand}$args
	}
}

proc gets {channel args} {
	set channel [stdio::std_channel $channel]
	return [uplevel 1 [list $channel gets {expand}$args]]
}

proc open {file args} {
	return [aio.open $file {expand}$args]
}

proc close {channel} {
	[stdio::std_channel $channel] close
}

proc eof {channel} {
	[stdio::std_channel $channel] eof
}

proc flush {channel} {
	[stdio::std_channel $channel] flush
}

proc read {channel args} {
	[stdio::std_channel $channel] read {expand}$args
}

proc seek {channel args} {
	[stdio::std_channel $channel] seek {expand}$args
}

proc tell {channel} {
	[stdio::std_channel $channel] tell
}
