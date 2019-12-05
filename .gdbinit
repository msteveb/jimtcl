db -output /home/x/debug-dashboard
db source -output /home/x/debug-source

db -layout breaks threads stack source
db source -style context 20
db assembly -style context 5
db stack -style limit 8

file ./jimsh
set args examples/debugscript.tcl
b jim.c:10506
r

db
db

