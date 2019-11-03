# Implements 'json::encode'
#
# (c) 2019 Steve Bennett <steveb@workware.net.au>
#
# See LICENCE in this directory for licensing.

# Encode Tcl objects as JSON
# dict -> object
# list -> array
# numeric -> number
# string -> string
#
# The schema provides the type information for the value.
# str = string
# num = numeric (or null)
# bool = boolean
# obj ... = object. parameters are 'name' 'subschema' where the name matches the dict.
# list ... = array. parameters are 'subschema' for the elements of the list/array.
# mixed ... = array of mixed types. parameters are types for each element of the list/array.

# Top level JSON encoder which encodes the given
# value based on the schema
proc json::encode {value {schema str}} {
	json::encode.[lindex $schema 0] $value [lrange $schema 1 end]
}

# Encode a string
proc json::encode.str {value {dummy {}}} {
	# Strictly we should be converting \x00 through \x1F to unicode escapes
	# And anything outside the BMP to a UTF-16 surrogate pair
	return \"[string map [list \\ \\\\ \" \\" \f \\f \n \\n / \\/ \b \\b \r \\r \t \\t] $value]\"
}

# If no type is given, also encode as a string
proc json::encode. {args} {
	tailcall json::encode.str {*}$args
}

# Encode a number
proc json::encode.num {value {dummy {}}} {
	if {$value in {Inf -Inf}} {
		append value inity
	}
	return $value
}

# Encode a boolean
proc json::encode.bool {value {dummy {}}} {
	if {$value} {
		return true
	}
	return false
}

# Encode an object (dictionary)
proc json::encode.obj {obj {schema {}}} {
	set result "\{"
	set sep " "
	foreach k [lsort [dict keys $obj]] {
		if {[dict exists $schema $k]} {
			set type [dict get $schema $k]
		} elseif {[dict exists $schema *]} {
			set type [dict get $schema *]
		} else {
			set type str
		}
		append result $sep\"$k\":

		append result [json::encode.[lindex $type 0] [dict get $obj $k] [lrange $type 1 end]]
		set sep ", "
	}
	append result " \}"
}

# Encode an array (list)
proc json::encode.list {list {type str}} {
	set result "\["
	set sep " "
	foreach l $list {
		append result $sep
		append result [json::encode.[lindex $type 0] $l [lrange $type 1 end]]
		set sep ", "
	}
	append result " \]"
}

# Encode a mixed-type array (list)
# Must be as many types as there are elements of the list
proc json::encode.mixed {list types} {
	set result "\["
	set sep " "
	foreach l $list type $types {
		append result $sep
		append result [json::encode.[lindex $type 0] $l [lrange $type 1 end]]
		set sep ", "
	}
	append result " \]"
}

# vim: se ts=4:
