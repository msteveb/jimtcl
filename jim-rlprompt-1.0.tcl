# Readline-based interactive shell for Jim
# Copyright(C) 2005 Salvatore Sanfilippo <antirez@invece.org>
#
# In order to automatically have readline-editing features
# put this in your $HOME/.jimrc
#
# if {$jim_interactive} {
#    if {[catch {package require rlprompt}] == 0} {
#       rlprompt.shell
#    }
# }

package require readline
package provide rlprompt 1.0

proc rlprompt.shell {} {
    puts "Readline shell loaded"
    puts "Welcome to Jim [info version]!"
    set prompt ". "
    while 1 {
        set line [readline.readline $prompt]
        if {[string length $line] == 0} {
            continue
        }
        readline.addhistory $line
        set errCode [catch {uplevel #0 $line} err]
        if {$err ne {}} {
            puts $err
        }
    }
}
