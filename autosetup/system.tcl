# Copyright (c) 2010 WorkWare Systems http://www.workware.net.au/
# All rights reserved

# @synopsis:
#
# This module supports common system interrogation and options
# such as --host, --build, --prefix, and setting srcdir, builddir, and EXEXT.
#
# It also support the 'feature' naming convention, where searching
# for a feature such as sys/type.h defines HAVE_SYS_TYPES_H
#
module-options {
	host:host-alias =>		{a complete or partial cpu-vendor-opsys for the system where
							the application will run (defaults to the same value as --build)}
	build:build-alias =>	{a complete or partial cpu-vendor-opsys for the system
							where the application will be built (defaults to the
							result of running config.guess)}
	prefix:dir =>			{the target directory for the build (defaults to /usr/local)}

	# These (hidden) options are supported for autoconf/automake compatibility
	exec-prefix:
	bindir:
	sbindir:
	includedir:
	mandir:
	infodir:
	libexecdir:
	datadir:
	libdir:
	sysconfdir:
	sharedstatedir:
	localstatedir:
	maintainer-mode=0
	dependency-tracking=0
}

# Returns 1 if exists, or 0 if  not
#
proc check-feature {name code} {
	msg-checking "Checking for $name..."
	set r [uplevel 1 $code]
	define-feature $name $r
	if {$r} {
		msg-result "ok"
	} else {
		msg-result "not found"
	}
	return $r
}

# @have-feature name ?default=0?
#
# Returns the value of the feature if defined, or $default if not.
# See 'feature-define-name' for how the feature name
# is translated into the define name.
#
proc have-feature {name {default 0}} {
	get-define [feature-define-name $name] $default
}

# @define-feature name ?value=1?
#
# Sets the feature 'define' to the given value.
# See 'feature-define-name' for how the feature name
# is translated into the define name.
#
proc define-feature {name {value 1}} {
	define [feature-define-name $name] $value
}

# @feature-checked name
#
# Returns 1 if the feature has been checked, whether true or not
#
proc feature-checked {name} {
	is-defined [feature-define-name $name]
}

# @feature-define-name name ?prefix=HAVE_?
#
# Converts a name to the corresponding define,
# e.g. sys/stat.h becomes HAVE_SYS_STAT_H.
#
# Converts * to P and all non-alphanumeric to underscore.
#
proc feature-define-name {name {prefix HAVE_}} {
	string toupper $prefix[regsub -all {[^a-zA-Z0-9]} [regsub -all {[*]} $name p] _]
}

# If $file doesn't exist, or it's contents are different than $buf,
# the file is written and $script is executed.
# Otherwise a "file is unchanged" message is displayed.
proc write-if-changed {file buf {script {}}} {
	set old [readfile $file ""]
	if {$old eq $buf && [file exists $file]} {
		msg-result "$file is unchanged"
	} else {
		writefile $file $buf\n
		uplevel 1 $script
	}
}

# @make-template template ?outfile?
#
# Reads the input file <srcdir>/$template and writes the output file $outfile.
# If $outfile is blank/omitted, $template should end with ".in" which
# is removed to create the output file name.
#
# Each pattern of the form @define@ is replaced the the corresponding
# define, if it exists, or left unchanged if not.
# 
# The special value @srcdir@ is subsituted with the relative
# path to the source directory from the directory where the output
# file is created. Use @top_srcdir@ for the absolute path.
#
proc make-template {template {out {}}} {
	set infile [file join $::autosetup(srcdir) $template]

	if {![file exists $infile]} {
		user-error "Template $template is missing"
	}

	# Define this as late as possible
	define AUTODEPS $::autosetup(deps)

	if {$out eq ""} {
		if {[file ext $template] ne ".in"} {
			autosetup-error "make_template $template has no target file and can't guess"
		}
		set out [file rootname $template]
	}

	set outdir [file dirname $out]

	# Make sure the directory exists
	file mkdir $outdir

	# Set up srcdir to be relative to the target dir
	define srcdir [relative-path [file join $::autosetup(srcdir) $outdir] $outdir]

	set mapping {}
	foreach {n v} [array get ::define] {
		lappend mapping @$n@ $v
	}
	writefile $out [string map $mapping [readfile $infile]]\n

	msg-result "Created [relative-path $out] from [relative-path $template]"
}

# build/host tuples and cross-compilation prefix
set build [opt-val build]
define build_alias $build
if {$build eq ""} {
	define build [config_guess]
} else {
	define build [config_sub $build]
}

set host [opt-val host]
define host_alias $host
if {$host eq ""} {
	define host [get-define build]
	set cross ""
} else {
	define host [config_sub $host]
	set cross $host-
}
define cross [get-env CROSS $cross]

set prefix [opt-val prefix /usr/local]

# These are for compatibility with autoconf
define target [get-define host]
define prefix $prefix
define builddir $autosetup(builddir)
define srcdir $autosetup(srcdir)
# Allow this to come from the environment
define top_srcdir [get-env top_srcdir [get-define srcdir]]

# autoconf supports all of these
define exec_prefix [opt-val exec-prefix [get-env exec-prefix \${prefix}]]
foreach {name defpath} {
	bindir \${exec_prefix}/bin
	sbindir \${exec_prefix}/sbin
	libexecdir \${exec_prefix}/libexec
	libdir \${exec_prefix}/lib
	datadir \${prefix}/share
	sysconfdir \${prefix}/etc
	sharedstatedir \${prefix}/com
	localstatedir \${prefix}/var
	infodir \${prefix}/share/info
	mandir \${prefix}/share/man
	includedir \${prefix}/include
} {
	define $name [opt-val $name [get-env $name $defpath]]
}

define SHELL [get-env SHELL [find-an-executable sh bash ksh]]

# Windows vs. non-Windows
switch -glob -- [get-define host] {
	*-*-ming* - *-*-cygwin {
		define-feature windows
		define EXEEXT .exe
	}
	default {
		define EXEEXT ""
	}
}

# Display
msg-result "Host System...[get-define host]"
msg-result "Build System...[get-define build]"
