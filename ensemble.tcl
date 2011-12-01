# Implement the ensemble command

proc ensemble {command args} {
	set autoprefix "$command "
	set badopts "should be \"ensemble command ?-automap prefix?\""
	if {[llength $args] % 2 != 0} {
		return -code error "wrong # args: $badopts"
	}
	foreach {opt value} $args {
		switch -- $opt {
			-automap { set autoprefix $value }
			default { return -code error "wrong # args: $badopts" }
		}
	}
	proc $command {subcmd args} {autoprefix {mapping {}}} {
		if {![dict exists $mapping $subcmd]} {
			# Not an exact match, so check for specials, then lookup normally
			if {$subcmd in {-commands -help}} {
				# Need to remove $autoprefix from the front of these
				set prefixlen [string length $autoprefix]
				set subcmds [lmap p [lsort [info commands $autoprefix*]] {
					string range $p $prefixlen end
				}]
				if {$subcmd eq "-commands"} {
					return $subcmds
				}
				set command [lindex [info level 0] 0]
				return "Usage: \"$command command ... \", where command is one of: [join $subcmds ", "]"
			}
			# cache the mapping
			dict set mapping $subcmd ${autoprefix}$subcmd
		}
		# tailcall here we don't add an extra stack frame, e.g. for uplevel
		tailcall [dict get $mapping $subcmd] {*}$args
	}
}
