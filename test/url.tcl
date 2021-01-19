package require tcltest
namespace import ::tcltest::*

source ./test-util.tcl
load [find_lib act_http ../build]
namespace import ::act::*

source ./test-util.tcl

test url_encode_decode {Round trip
} -body {
    set s "!@#$%^&*()_+-/'\"[]abcdef ABCDEF`!\\~{}0987654321\r\n\t\f\v"
    expr {[url decode [url encode $s]] eq $s}
} -result 1

test url_encode_whitespace {Whitespace encoding
} -body {
    url encode " \t\r\n\f\v"
} -result {%20%09%0D%0A%0C%0B}

cleanupTests
