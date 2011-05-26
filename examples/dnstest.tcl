lappend auto_path [pwd]
package require dns

# Use google's DNS
dns::configure -nameserver 8.8.8.8

puts "Resolve with udp"
set tok [dns::resolve www.tcl.tk]
puts status=[dns::status $tok]
puts address=[dns::address $tok]
puts names=[dns::name $tok]
dns::cleanup $tok

# Now with tcp
dns::configure -protocol tcp

puts "Resolve with tcp"
set tok [dns::resolve www.google.com]
puts status=[dns::status $tok]
puts address=[dns::address $tok]
puts names=[dns::name $tok]
dns::cleanup $tok
