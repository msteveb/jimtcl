package require mk

# These will become subcommands of every view handle

# Looping using cursors
proc {mk.view each} {view arrayVar script} {
    upvar 1 $arrayVar array
    for {set cur $view!0} {[cursor valid $cur]} {cursor incr cur} {
        set array [cursor get $cur]
        uplevel 1 $script
    }
}

# Shortcuts to avoid cursors for one-time operations
proc {mk.view set} {view pos args} {
    tailcall cursor set $view!$pos {*}$args
}
proc {mk.view append} {view args} {
    tailcall cursor set $view!end+1 {*}$args
}
proc {mk.view insert} {view pos args} {
    # Note that this only inserts fresh rows and doesn't set any data
    tailcall cursor insert $view!$pos {*}$args
}

# Dump a view to stdout
proc {mk.view dump} {view} {
    $view each row {puts "  $row"}
}

# -----------------------------------------------------------------------------

# Open an in-memory database
set db [storage]

# Specify the view structure, creating new views and restructuring existing
# ones as necessary
$db structure firstview  {key string first string}
$db structure secondview {key string second string}

# Open them.
[$db view firstview] as fstview
# Or equivalently (using pipeline notation)
$db view secondview | as sndview

# Use the helpers defined above to populate the first view
$fstview set 0 key foo first bar
$fstview append key hello first world
$fstview insert 0
$fstview set 0 key metakit first example

# Or use cursors directly. A end-X/end+X cursor moves automatically when
# the view size changes.
set cur $sndview!end+1
cursor set $cur key foo second baz
cursor set $cur key hello second goodbye
cursor set $cur key silly second examples

puts "First view:"
$fstview dump
puts "Second view:"
$sndview dump

puts "\nNow trying view operations. Note that all the binary operations"
puts "are left-biased when it comes to conflicting property values.\n"

puts "Join on key:" ;# Common subset of the two outer joins below
$fstview join $sndview key | dump
puts "Outer join on key:" ;# Will yield more rows than an inner join
$fstview join $sndview -outer key | dump
puts "Outer join on key, in reverse order:"
$sndview join $fstview -outer key | dump

puts "Cartesian product:"
$fstview product $sndview | dump

puts "Pairing:"
$fstview pair $sndview | dump
puts "Pairing, in reverse order:"
$sndview pair $fstview | dump

puts "Complex pipeline (fetch rows 3,5,.. from the cartesian product and sort"
puts "them on the 'first' property):"
$fstview product $sndview | range 3 end 2 | sort first | dump
# Slice step defaults to 1. Sorting may be performed on several properties at
# a time, prepending a "-" (minus sign) will cause the sort order to be reversed.

puts "Another one (fetch the unique key values from the cartesian product):"
$fstview product $sndview | project key | unique | dump
# Use "without" to remove certain properties.

puts "Keys in the cartesian product not in the reverse pairing:"
[$fstview product $sndview | project key | unique] minus [$sndview pair $fstview | unique] | dump
# Union "union", intersection "intersect" and symmetric difference "different"
# are also available. They all work only if the rows are unique.

puts "Create a subview:"
$fstview product $sndview | group subv key | as complexview | dump
# Not so informative as subviews are not displayed properly. Several grouping
# properties may be specified.
puts "Get its values for row #0:"
cursor get $complexview!0 subv | dump
puts "And flatten it back:"
$complexview flatten subv | dump

puts "Remove a row:"
cursor remove $sndview!1
$sndview dump
# Several rows may be removed at once by specifying a row count
puts "Clear the view:"
$sndview resize 0
$sndview dump
