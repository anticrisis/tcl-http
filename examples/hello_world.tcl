#!/bin/env tclsh
#
# Ensure act_http directory is on your auto_path or TCLLIBPATH, 
# then invoke like this:
#
# $ tclsh hello_world.tcl -host 127.0.0.1 -port 8080
#
package require act_http
namespace import act::*

http configure -get {list 200 "hello world" "text/plain"}
http configure {*}$argv

#
# Query individual config options:
#
puts "Host: [http configure -host]"
puts "Port: [http configure -port]"

#
# Query all options:
#
# puts ""
# puts "Config: [http configure]"

puts ""
puts "Starting server... Press Ctrl-C to exit."

http run 

