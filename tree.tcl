package provide tree

# Conceptually compatible with tcllib ::struct::tree
# but uses an object based interface.
# To mimic tcllib, do:
#   rename [tree] mytree

# set pt [tree]
#
#   Create a tree
#   This automatically creates a node named "root"
#
# $pt destroy
#
#   Destroy the tree and all it's nodes
#
# $pt set <nodename> <key> <value>
#
#   Set the value for the given key
#
# $pt lappend <nodename> <key> <value> ...
#
#   Append to the (list) value(s) for the given key, or set if not yet set
#
# $pt keyexists <nodename> <key>
#
#   Returns 1 if the given key exists
#
# $pt get <nodename> <key>
#
#   Returns the value associated with the given key
# 
# $pt depth <nodename>
#
#   Returns the depth of the given node. The depth of "root" is 0.
#
# $pt parent <nodename>
#
#   Returns the name of the parent node, or "" for the root node.
# 
# $pt numchildren <nodename>
#
#   Returns the number of child nodes.
# 
# $pt children <nodename>
#
#   Returns a list of the child nodes.
# 
# $pt next <nodename>
#
#   Returns the next sibling node, or "" if none.
# 
# $pt insert <nodename> <index>
#
#   Add a new child node to the given node.
#   Currently the node is always added at the end (index=end)
#   Returns the name of the newly added node
#
# $pt walk <nodename> dfs|bfs {actionvar nodevar} <code>
#
#   Walks the tree starting from the given node, either breadth first (bfs)
#   depth first (dfs).
#   The value "enter" or "exit" is stored in variable $actionvar
#   The name of each node is stored in $nodevar.
#   The script $code is evaluated twice for each node, on entry and exit.

# Create a new tree
proc tree {} {
	# A tree is a dictionary of (name, noderef)
	# The name for the root node is always "root",
	# and other nodes are automatically named "node1", "node2", etc.

	# Create the root node
	lassign [_tree.makenode ""] dummy rootref

	# Create the tree containing one node
	set tree [dict create root $rootref]

	# And create a reference to a tree dictionary
	set treeref [ref $tree tree]

	lambda {command args} {treeref} {
		#puts "You invoked [list treehandle $command $args]"
		uplevel 1 [list tree.$command $treeref {*}$args]
	}
}

# treehandle insert node ?index?
#
proc tree.insert {treeref node {index end}} {
	# Get the parent node
	set parentref [_tree.getnoderef $treeref $node]

	# Make a new node
	lassign [_tree.makenode $parentref] childname childref

	# Add it to the list of children in the parent node
	_tree.update_node $treeref $node parent {
		lappend parent(.children) $childref
	}

	# Add it to the tree
	_tree.update_tree $treeref tree {
		set tree($childname) $childref
	}

	return $childname
}

# treehandle set node key value
#
proc tree.set {treeref node key value} {
	_tree.update_node $treeref $node n {
		set n($key) $value
	}
	return $value
}

# treehandle lappend node key value
#
proc tree.lappend {treeref node key args} {
	_tree.update_node $treeref $node n {
		lappend n($key) {expand}$args
		set result $n($key)
	}
	return $result
}

# treehandle get node key
#
proc tree.get {treeref node key} {
	set n [_tree.getnode $treeref $node]

	return $n($key)
}

# treehandle keyexists node key
#
proc tree.keyexists {treeref node key} {
	set n [_tree.getnode $treeref $node]
	info exists n($key)
}

# treehandle depth node
#
proc tree.depth {treeref node} {
	set n [_tree.getnode $treeref $node]
	return $n(.depth)
}

# treehandle parent node
#
proc tree.parent {treeref node} {
	set n [_tree.getnode $treeref $node]
	return $n(.parent)
}

# treehandle numchildren node
#
proc tree.numchildren {treeref node} {
	set n [_tree.getnode $treeref $node]
	llength $n(.children)
}

# treehandle children node
#
proc tree.children {treeref node} {
	set n [_tree.getnode $treeref $node]
	set result {}
	foreach child $n(.children) {
		set c [getref $child]
		lappend result $c(.name)
	}
	return $result
}

# treehandle next node
#
proc tree.next {treeref node} {
	set parent [tree.parent $treeref $node]
	set siblings [tree.children $treeref $parent] 
	set i [lsearch $siblings $node]
	incr i
	return [lindex $siblings $i]
}

# treehandle walk node bfs|dfs {action loopvar} <code>
#
proc tree.walk {treeref node type vars code} {
	set n [_tree.getnode $treeref $node]

	# set up vars
	lassign $vars actionvar namevar

	if {$type ne "child"} {
		upvar $namevar name
		upvar $actionvar action

		# Enter this node
		set name $node
		set action enter

		uplevel 1 $code
	}

	if {$type eq "dfs"} {
		# Depth-first so do the children
		foreach childref $n(.children) {
			set child [getref $childref]
			uplevel 1 [list tree.walk $treeref $child(.name) $type $vars $code]
		}
	} elseif {$type ne "none"} {
		# Breadth-first so do the children to one level only
		foreach childref $n(.children) {
			set child [getref $childref]
			uplevel 1 [list tree.walk $treeref $child(.name) none $vars $code]
		}

		# Now our grandchildren
		foreach childref $n(.children) {
			set child [getref $childref]
			uplevel 1 [list tree.walk $treeref $child(.name) child $vars $code]
		}
	}

	if {$type ne "child"} {
		# Exit this node
		set name $node
		set action exit

		uplevel 1 $code
	}
}

# Destroys the tree
#
proc tree.destroy {treeref} {
	set tree [getref $treeref]
	foreach {nodename noderef} $tree {
		setref $noderef {}
	}
	setref $treeref {}

	# Extract the name of the handle
	set t [lindex [info level 1] 0]
	rename $t ""
}

#
# INTERNAL procedures below this point
#


# Make a new child node of the parent
#
# Note that this does *not* add the node
# to the parent or to the tree
#
# Returns a list of {nodename noderef}
#
proc _tree.makenode {parent} {{nodeid 1}} {
	if {$parent eq ""} {
		# The root node
		set name root
		set depth 0
		set parentname ""
	} else {
		set parentnode [getref $parent]

		set name node$nodeid
		incr nodeid
		set depth $parentnode(.depth)
		incr depth
		set parentname $parentnode(.name)
	}

	# Return a list of name, reference
	list $name [ref [list .name $name .depth $depth .parent $parentname .children {}] node]
}

# Return the node (dictionary value) with the given name
#
proc _tree.getnode {treeref node} {
	getref [dict get [getref $treeref] $node]
}

# Return the noderef with the given name
#
proc _tree.getnoderef {treeref node} {
	dict get [getref $treeref] $node
}

# Set a dictionary value named $varname in the parent context,
# evaluate $code, and then store any changes to
# the node (via $varname) back to the node
#
proc _tree.update_node {treeref node varname code} {
	upvar $varname n

	# Get a reference to the node
	set ref [_tree.getnoderef $treeref $node]

	# Get the node itself
	set n [getref $ref]

	uplevel 1 $code

	# And update the reference
	setref $ref $n
}

# Set a dictionary value named $varname in the parent context,
# evaluate $code, and then store any changes to
# the tree (via $varname) back to the tree
#
proc _tree.update_tree {treeref varname code} {
	upvar $varname t

	# Get the tree value
	set t [getref $treeref]

	# Possibly modify it
	uplevel 1 $code

	# And update the reference
	setref $treeref $t
}
