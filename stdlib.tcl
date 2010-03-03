# Create a single word alias (proc) for one or more words
# e.g. alias x info exists
# if {[x var]} ...
proc alias {name args} {
	set prefix $args
	proc $name args prefix {
		uplevel 1 $prefix $args
	}
}

# Creates an anonymous procedure
proc lambda {arglist args} {
	set name [ref {} function lambda.finalizer]
	uplevel 1 [list proc $name $arglist {*}$args]
	return $name
}

proc lambda.finalizer {name val} {
	rename $name {}
}

# Like alias, but creates and returns an anonyous procedure
proc curry {args} {
	set prefix $args
	lambda args prefix {
		uplevel 1 $prefix $args
	}
}

# Returns the given argument.
# Useful with 'local' as follows:
#   proc a {} {...}
#   local function a 
#
#   set x [lambda ...]
#   local function $x
#
proc function {value} {
	return $value
}
