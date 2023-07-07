# OO support for Jim Tcl, with multiple inheritance

# Create a new class $classname, with the given
# dictionary as class variables. These are the initial
# variables which all newly created objects of this class are
# initialised with.
#
# If a list of baseclasses is given,
# methods and instance variables are inherited.
# The *last* baseclass can be accessed directly with [super]
# Later baseclasses take precedence if the same method exists in more than one
proc class {classname {baseclasses {}} classvars} {
	set baseclassvars {}
	foreach baseclass $baseclasses {
		# Start by mapping all methods to the parent class
		foreach method [$baseclass methods] { alias "$classname $method" "$baseclass $method" }
		# Now import the base class classvars
		set baseclassvars [dict merge $baseclassvars [$baseclass classvars]]
		# The last baseclass will win here
		proc "$classname baseclass" {} baseclass { return $baseclass }
	}

	# Merge in the baseclass vars with lower precedence
	set classvars [dict merge $baseclassvars $classvars]

	# This is the class dispatcher for $classname
	# It simply dispatches 'classname cmd' to a procedure named {classname cmd}
	# with a nice message if the class procedure doesn't exist
	proc $classname {{cmd new} args} classname {
		if {![exists -command "$classname $cmd"]} {
			return -code error "$classname, unknown command \"$cmd\": should be [join [$classname methods] ", "]"
		}
		tailcall "$classname $cmd" {*}$args
	}

	# Constructor
	proc "$classname new" {args} {classname classvars} {
		# This is the object dispatcher for $classname.
		# Store the classname in both the ref value and tag, for debugging
		set obj ::[ref $classname $classname "$classname finalize"]
		set instvars $classvars
		proc $obj {method args} {classname instvars} {
			if {![exists -command "$classname $method"]} {
				if {![exists -command "$classname unknown"]} {
					return -code error "$classname, unknown method \"$method\": should be [join [$classname methods] ", "]"
				}
				return ["$classname unknown" $method {*}$args]
			}
			"$classname $method" {*}$args
		}
		$obj constructor {*}$args
		return $obj
	}
	# Finalizer to invoke destructor during garbage collection
	proc "$classname finalize" {ref classname} { $ref destroy }
	# Method creator
	proc "$classname method" {method arglist __body} classname {
		proc "$classname $method" $arglist {__body} {
			# Make sure this isn't incorrectly called without an object
			if {![uplevel exists instvars]} {
				return -code error -level 2 "\"[lindex [info level 0] 0]\" method called with no object"
			}
			set self [lindex [info level -1] 0]
			# Note that we can't use 'dict with' here because
			# the dict isn't updated until the body completes.
			foreach __ [$self vars] {upvar 1 instvars($__) $__}
			unset -nocomplain __
			eval $__body
		}
	}
	# Other simple class procs
	proc "$classname vars" {} classvars { lsort [dict keys $classvars] }
	proc "$classname classvars" {} classvars { return $classvars }
	proc "$classname classname" {} classname { return $classname }
	proc "$classname methods" {} classname {
		lsort [lmap p [info commands -all "$classname *"] {
			lindex [split $p " "] 1
		}]
	}
	# Pre-define some instance methods
	$classname method defaultconstructor {{__vars {}}} {
		set __classvars [$self classvars]
		foreach __v [dict keys $__vars] {
			if {![dict exists $__classvars $__v]} {
				# level 3 because defaultconstructor is called by new
				return -code error -level 3 "[lindex [info level 0] 0], $__v is not a class variable"
			}
			set $__v [dict get $__vars $__v]
		}
	}
	alias "$classname constructor" "$classname defaultconstructor"
	$classname method destroy {} { rename $self "" }
	$classname method get {var} { set $var }
	$classname method eval {{__locals {}} __body} {
		foreach __ $__locals { upvar 2 $__ $__ }
		unset -nocomplain __
		eval $__body
	}
	return $classname
}

# From within a method, invokes the given method on the base class.
# Note that this will only call the last baseclass given
proc super {method args} {
	# If we are called from "class method", we want to call "[$class baseclass] method"
	set classname [lindex [info level -1] 0 0]
	uplevel 2 [list [$classname baseclass] $method {*}$args]
}
