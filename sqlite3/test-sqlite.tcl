# A simple test of the "big" sqlite extension

set auto_path [list . {*}$auto_path]

package require sqlite

# Create an in-memory database and add some data
sqlite db :memory:
db eval {CREATE TABLE history (type, time, value)}
foreach t [range 1 50] {
    set temp [rand 100]
    db eval {INSERT INTO history (type, time, value) VALUES ('temp', :t, :temp)}
}
foreach t [range 2 50 2] {
    set v $([rand 200] / 10.0 + 5)
    db eval {INSERT INTO history (type, time, value) VALUES ('voltage', :t, :v)}
}

# Output some data in SVG format.
puts "\nSVG Example\n"

set points {}
db eval {SELECT time,value FROM history
         WHERE (time >= 10 and time <= 30) and type = 'voltage'
         ORDER BY time DESC} row {
    lappend points $row(time),$row(value)
}
puts "<polyline points=\"$points\" />"

# And tabular format with a self outer join
puts "\nTabular Self Outer Join Example\n"

proc showrow {args} {
    puts [format "%-12s %-12s %-12s" {*}$args]
}

showrow Time Temp Voltage
showrow ---- ---- -------
db eval {SELECT * FROM (SELECT time, value AS temp FROM history WHERE type = 'temp') AS A
        LEFT OUTER JOIN (SELECT time, value AS voltage FROM history WHERE type = 'voltage') AS B
        USING (time)
        WHERE time >= 10 AND time <= 30
        ORDER BY time} row {
    showrow $row(time) $row(temp) $row(voltage)
}
set maxtemp [db eval {SELECT max(value) FROM history WHERE type = 'temp'}]
set maxvolt [db eval {SELECT max(value) AS maxvolt FROM history WHERE type = 'voltage'}]
showrow ---- ---- -------
showrow max $maxtemp $maxvolt

db close
