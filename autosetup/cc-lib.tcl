# Copyright (c) 2011 WorkWare Systems http://www.workware.net.au/
# All rights reserved

# @synopsis:
#
# Provides a library of common tests on top of the 'cc' module.

use cc

module-options {}

# @cc-check-lfs
#
# The equivalent of the AC_SYS_LARGEFILE macro
# 
# defines 'HAVE_LFS' if LFS is available,
# and defines '_FILE_OFFSET_BITS=64' if necessary
#
# Returns 1 if 'LFS' is available or 0 otherwise
#
proc cc-check-lfs {} {
	cc-check-includes sys/types.h
	msg-checking "Checking if -D_FILE_OFFSET_BITS=64 is needed..."
	set lfs 1
	if {[msg-quiet cc-with {-includes sys/types.h} {cc-check-sizeof off_t}] == 8} {
		msg-result no
	} elseif {[msg-quiet cc-with {-includes sys/types.h -cflags -D_FILE_OFFSET_BITS=64} {cc-check-sizeof off_t}] == 8} {
		define _FILE_OFFSET_BITS 64
		msg-result yes
	} else {
		set lfs 0
		msg-result none
	}
	define-feature lfs $lfs
	return $lfs
}
