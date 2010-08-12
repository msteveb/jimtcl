lassign [socket pipe] r w

# Note, once the exec has the fh (via dup), close it
# so that the pipe data is accessible
exec ps aux >@$w &
$w close

$r readable {
	puts [$r gets]
	if {[eof $r]} {
		$r close
		set done 1
	}
}

vwait done
