#!/bin/env tclsh
#
# Ensure the act/http-{vsn}.tm module is on your module path
# then invoke like this:
#
# $ tclsh hello_world.tcl -host 127.0.0.1 -port 8080
#

package require act::http

act::http configure -get {list 200 "hello world" "text/plain"}
act::http configure {*}$argv

#
# Query individual config options:
#
puts "Host: [act::http configure -host]"
puts "Port: [act::http configure -port]"

#
# Query all options:
#
# puts ""
# puts "Config: [http configure]"

puts ""
puts "Starting server... Press Ctrl-C to exit."

act::http run
