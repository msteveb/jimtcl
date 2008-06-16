# Jim stdlib - a pure-Jim extension library for Jim
#
# Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# A copy of the license is also included in the source distribution
# of Jim, as a TXT file name called LICENSE.
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# To use this library just do [package require stdlib]
# Make sure this file is in one directory specified in $jim_libpath

package provide stdlib 1.0

### Functional programming ###

proc curry {cmd args} {
    lambda args [list cmd [list pref $args]] {
        uplevel 1 [list $cmd {expand}$pref {expand}$args]
    }
}

proc memoize {} {{Memo {}}} {
    set cmd [info level -1]
    if {[info level] > 2 && [lindex [info level -2] 0] eq "memoize"} return
    if {![info exists Memo($cmd)]} {set Memo($cmd) [eval $cmd]}
    return -code return $Memo($cmd)
}

### Control structures ###

proc repeat {n body} {
    for {set i 0} {$i < $n} {incr i} {
        uplevel 1 $body
    }
}

### List procedures ###

proc first {list} {lindex $list 0}
proc rest {list} {lrange $list 1 end}
proc last {list} {lindex $list end}

### EOF ###
