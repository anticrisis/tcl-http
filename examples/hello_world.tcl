#!/bin/env tclsh
#
# Ensure the act/http-{vsn}.tm module is on your module path
# then invoke like this:
#
# $ tclsh hello_world.tcl -host 127.0.0.1 -port 8080
#

#
# source user's init script, if present, to pick up user-defined module and
# package paths
#
if {[file exists ~/.tcl/init.tcl]} {source ~/.tcl/init.tcl}

package require act::http
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

