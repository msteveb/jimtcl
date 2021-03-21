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
	json::subencode [lindex $schema 0] $value [lrange $schema 1 end]
}

# encode the value according to to the given type
proc json::subencode {type value {schema {}}} {
	switch -exact -- $type {
		str - "" {
			# Strictly we should be converting \x00 through \x1F to unicode escapes
			# And anything outside the BMP to a UTF-16 surrogate pair
			return \"[string map [list \\ \\\\ \" \\" \f \\f \n \\n / \\/ \b \\b \r \\r \t \\t] $value]\"
		}
		num {
			if {$value in {Inf -Inf}} {
				append value inity
			}
			return $value
		}
		bool {
			if {$value} {
				return true
			}
			return false
		}
		obj {
			set result "\{"
			set sep " "
			foreach k [lsort [dict keys $value]] {
				if {[dict exists $schema $k]} {
					set subtype [dict get $schema $k]
				} elseif {[dict exists $schema *]} {
					set subtype [dict get $schema *]
				} else {
					set subtype str
				}
				append result $sep\"$k\":

				append result [json::subencode [lindex $subtype 0] [dict get $value $k] [lrange $subtype 1 end]]
				set sep ", "
			}
			append result " \}"
			return $result
		}
		list {
			set result "\["
			set sep " "
			foreach l $value {
				append result $sep
				append result [json::subencode [lindex $schema 0] $l [lrange $schema 1 end]]
				set sep ", "
			}
			append result " \]"
			return $result
		}
		mixed {
			set result "\["
			set sep " "
			foreach l $value subtype $schema {
				append result $sep
				append result [json::subencode [lindex $subtype 0] $l [lrange $subtype 1 end]]
				set sep ", "
			}
			append result " \]"
		}
		default {
			error "bad type $type"
		}
	}
}

# vim: se ts=4:
