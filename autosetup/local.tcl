global withinfo
global extdb

# Final determination of module status
dict set extdb status {}

# Returns 1 if the extension has the attribute
proc ext-has {ext attr} {
    expr {$attr in [dict get $::extdb attrs $ext]}
}

# Returns an entry from the extension 'info' table, or $default otherwise
proc ext-get {ext key {default {}}} {
    if {[dict exists $::extdb info $ext $key]} {
        return [dict get $::extdb info $ext $key]
    } else {
        return $default
    }
}

# Set the status of the extension to the given value, and returns the value
proc ext-set-status {ext value} {
    dict set ::extdb status $ext $value
    return $value
}

# Returns the status of the extension, or ? if unknown
proc ext-get-status {ext} {
    if {[dict exists $::extdb status $ext]} {
        return [dict get $::extdb status $ext]
    }
    return ?
}

proc check-extension-status {ext required {asmodule 0}} {
    global withinfo

    set status [ext-get-status $ext]

    if {$ext in $withinfo(without)} {
        # Disabled without further ado
        msg-result "Extension $ext...disabled"
        return [ext-set-status $ext n]
    }

    if {$status in {m y n}} {
        return $status
    }

    # required is "required" if this extension *must* be enabled
    # required is "wanted" if it is not fatal for this extension
    # not to be enabled

    array set depinfo {m 0 y 0 n 0}

    # Stash the current value of LIBS
    set LIBS [get-define LIBS]

    set use_pkgconfig 0
    set pkgconfig [ext-get $ext pkg-config]
    if {$pkgconfig ne ""} {
        # pkg-config support is optional, so explicitly initialise it here
        if {[pkg-config-init 0]} {
            # Check for at least one set of alternates
            foreach pinfo [split $pkgconfig |] {
                set ok 1
                set pkgs {}
                foreach pkg [split $pinfo ,] {
                    set args [lassign $pkg pkgname]
                    set pkg [string trim $pkg]
                    set optional 0
                    if {[string match {*[*]} $pkg]} {
                        # This package is optional
                        set optional 1
                        set pkg [string range $pkg 0 end-1]
                    }
                    if {![pkg-config $pkg {*}$args]} {
                        if {!$optional} {
                            set ok 0
                            break
                        }
                    } else {
                        lappend pkgs $pkg
                    }
                }
                if {$ok} {
                    set use_pkgconfig 1
                    break
                }
            }
        }
    }
    if {!$use_pkgconfig} {
        # Check direct dependencies
        if [ext-get $ext check 1] {
            # "check" conditions are met
        } else {
            # not met
            incr depinfo(n)
        }
    }

    # asmodule=1 means that the parent is a module so
    # any automatically selected dependencies should also be modules
    if {$asmodule == 0 && $ext in $withinfo(mod)} {
        set asmodule 1
    }

    if {$asmodule} {
        # This is a module, so ignore LIBS
        # LDLIBS_$ext will contain the appropriate libs for this module
        define LIBS $LIBS
    }

    if {$depinfo(n) == 0} {
        # Now extension dependencies
        foreach i [ext-get $ext dep] {
            set status [check-extension-status $i $required $asmodule]
            #puts "$ext: dep $i $required => $status"
            incr depinfo($status)
            if {$depinfo(n)} {
                break
            }
        }
    }

    #parray depinfo

    if {$depinfo(n)} {
        msg-checking "Extension $ext..."
        if {$required eq "required"} {
            user-error "dependencies not met"
        }
        msg-result "disabled (dependencies)"
        return [ext-set-status $ext n]
    }

    # Selected as a module directly or because of a parent dependency?
    if {$asmodule} {
        if {[ext-has $ext tcl]} {
            # Easy, a Tcl module
            msg-result "Extension $ext...tcl"
        } elseif {[ext-has $ext static]} {
            user-error "Extension $ext can't be a module"
        } else {
            msg-result "Extension $ext...module"
            if {$use_pkgconfig} {
                add-pkgconfig-deps $ext $pkgs $asmodule
            } else {
                foreach i [ext-get $ext libdep] {
                    define-append LDLIBS_$ext [get-define $i ""]
                }
            }
        }
        return [ext-set-status $ext m]
    }

    # Selected as a static extension?
    if {[ext-has $ext shared]} {
        user-error "Extension $ext can only be selected as a module"
    } elseif {$ext in $withinfo(ext) || $required eq "$required"} {
        msg-result "Extension $ext...enabled"
    } elseif {$ext in $withinfo(maybe)} {
        msg-result "Extension $ext...enabled (default)"
    } else {
        # Could be selected, but isn't (yet)
        return [ext-set-status $ext x]
    }
    if {$use_pkgconfig} {
        add-pkgconfig-deps $ext $pkgs $asmodule
    } else {
        foreach i [ext-get $ext libdep] {
            define-append LDLIBS [get-define $i ""]
        }
    }
    return [ext-set-status $ext y]
}

# Add dependencies for a pkg-config module to the extension
proc add-pkgconfig-deps {ext pkgs asmodule} {
    foreach pkg $pkgs {
        if {$asmodule} {
            define-append LDLIBS_$ext [pkg-config-get $pkg LIBS]
        } else {
            define-append LDLIBS [pkg-config-get $pkg LIBS]
        }
        define-append LDFLAGS [pkg-config-get $pkg LDFLAGS]
        define-append AS_CFLAGS [pkg-config-get $pkg CFLAGS]
        define-append PKG_CONFIG_REQUIRES $pkg
    }
}

# Examines the user options (the $withinfo array)
# and the extension database ($extdb) to determine
# what is selected, and in what way.
#
# If $allextmod is 1, extensions that would normally be disabled
# are enabled as modules if their prerequisites are met
#
# The results are available via ext-get-status
# And a dictionary is returned containing four keys:
#   static-c     extensions which are static C
#   static-tcl   extensions which are static Tcl
#   module-c     extensions which are C modules
#   module-tcl   extensions which are Tcl modules
proc check-extensions {allextmod} {
    global extdb withinfo

    # Check valid extension names
    foreach i [concat $withinfo(ext) $withinfo(mod)] {
        if {![dict exists $extdb attrs $i]} {
            user-error "Unknown extension: $i"
        }
    }

    set extlist [lsort [dict keys [dict get $extdb attrs]]]

    set withinfo(maybe) {}
    set withinfo(maybemod) {}

    # Now work out the default status. We have.
    # normal case, include !off, !optional if possible
    # --full, include !off if possible
    # --without=default, don't include optional or off
    if {$withinfo(nodefault)} {
        lappend withinfo(maybe) stdlib
    } else {
        foreach i $extlist {
            if {[ext-has $i off]} {
                if {$allextmod} {
                    lappend withinfo(maybemod) $i
                }
                continue
            }
            if {[ext-has $i optional] && !$withinfo(optional)} {
                if {$allextmod} {
                    lappend withinfo(maybemod) $i
                }
                continue
            }
            lappend withinfo(maybe) $i
        }
    }

    foreach i $extlist {
        define LDLIBS_$i ""
    }

    foreach i [concat $withinfo(ext) $withinfo(mod)] {
        check-extension-status $i required
    }
    foreach i $withinfo(maybe) {
        check-extension-status $i wanted
    }
    foreach i $withinfo(maybemod) {
        check-extension-status $i wanted 1
    }

    array set extinfo {static-c {} static-tcl {} module-c {} module-tcl {}}

    foreach i $extlist {
        set status [ext-get-status $i]
        set tcl [ext-has $i tcl]
        switch $status,$tcl {
            y,1 {
                define jim_ext_$i
                lappend extinfo(static-tcl) $i
            }
            y,0 {
                define jim_ext_$i
                lappend extinfo(static-c) $i
                # If there are any static C++ extensions, jimsh must be linked using
                # the C++ compiler
                if {[ext-has $i cpp]} {
                    define HAVE_CXX_EXTENSIONS
                }
            }
            m,1 { lappend extinfo(module-tcl) $i }
            m,0 { lappend extinfo(module-c) $i }
        }
    }
    return [array get extinfo]
}
