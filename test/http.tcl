package require tcltest
namespace import ::tcltest::*

source ./test-util.tcl

set tclsh        [info nameofexecutable]
set test_addr    {-host 127.0.0.1}
set test_server  [list {*}$test_addr -exittarget "/die"]

set load_http "load [find_lib act_http ../build]"

{*}$load_http
namespace import ::act::*

proc kill {port} {
    global test_addr
    act::http client {*}$test_addr -port $port -method options -target "/die"
}

proc background {port prog} {
    global tclsh load_http test_addr test_server
    exec $tclsh << [subst -nocommands $prog] &
}

proc without_headers {res} {list [lindex $res 0] [lindex $res 2]}


test get_hello {Sanity check: hello world} -body {
    set port [rand_port]

    background $port {
        $load_http
        namespace import ::act::*
        http configure -get {list 200 "hello, world" "text/plain"} \
            {*}$test_server -port $port
        http run
        }

    set res [http client {*}$test_addr -port $port -method get -target /]
    kill $port
    without_headers $res
} -result {200 {hello, world}}

# prefix vars with \ to defer evaluation until inside the subshell
proc check_target_body_headers {method} {
    # shell to check that post and put have access to target, body and headers
    string map "\$method $method" {
    $load_http
    namespace import act::*
    namespace eval ::my_ns {
    variable target
    variable body
    variable headers
    proc handle {} {
        variable target
        variable body
        variable headers
        if {\$target ne "/" || \$body ne "big body" || [dict get \$headers "X-Head"] ne "tail"} {
            fail \$target \$body \$headers
        } else {
            success
        }
    }
    http configure -$method [namespace code handle] \
        -reqtargetvariable [namespace current]::target \
        -reqbodyvariable [namespace current]::body \
        -reqheadersvariable [namespace current]::headers \
        {*}$test_server -port $port
    http configure
    http run
    }}
}

proc check_target_headers {method} {
    string map "\$method $method" {
    $load_http
    namespace import act::*
    namespace eval ::my_ns {
    variable target
    variable headers
    proc handle {} {
        variable target
        variable headers
        if {\$target ne "/" || [dict get \$headers "X-Head"] ne "tail"} {
            fail \$target \$headers
        } else {
            success
        }
    }
    http configure -$method [namespace code handle] \
        -reqtargetvariable [namespace current]::target \
        -reqheadersvariable [namespace current]::headers \
        {*}$test_server -port $port
    http configure
    http run
    }}
}

proc set_resp_headers {method} {
    string map "\$method $method" {
    $load_http
    namespace import act::*
    namespace eval ::my_ns {
    proc handle {} {
        list 200 "ok" "text/plain" {X-Head-1 val1 X-Head-2 val2}
    }
    http configure -$method [namespace code handle] \
        {*}$test_server -port $port
    http configure
    http run
    }}
}

#

test options_all {OPTIONS: test variables} -body {
    set method options
    set port [rand_port]
    set fail {proc fail {args} {list 500 "" ""}\n}
    set success {proc success {} {list 200 "" ""}\n}
    background $port [string cat $fail $success [check_target_body_headers $method]]
    set res [http client {*}$test_addr -port $port -method $method \
        -target "/"                                                \
        -body {big body}                                           \
        -headers {X-Head tail}]
    kill $port
    lindex $res 0
} -result 200

test get_all {GET: test variables} -body {
    set method get
    set port [rand_port]
    set fail {proc fail {args} {list 500 \$args ""}\n}
    set success {proc success {} {list 200 "" ""}\n}
    background $port [string cat $fail $success [check_target_headers $method]]
    set res [http client {*}$test_addr -port $port -method $method \
        -target "/"                                                \
        -headers {X-Head tail}]
    kill $port
    lindex $res 0
} -result 200

test delete_all {DELETE: test variables} -body {
    set method delete
    set port [rand_port]
    set fail {proc fail {args} {list 500 "" ""}\n}
    set success {proc success {} {list 204 "" ""}\n}
    background $port [string cat $fail $success [check_target_body_headers $method]]
    set res [http client {*}$test_addr -port $port -method $method \
        -target "/"                                                \
        -body {big body}                                           \
        -headers {X-Head tail}]
    kill $port
    lindex $res 0
} -result 204

test post_all {POST: test all} -body {
    set method post
    set port [rand_port]
    set fail {proc fail {args} {list 500 \$args ""}\n}
    set success {proc success {} {list 200 "" ""}\n}
    background $port [string cat $fail $success [check_target_body_headers $method]]
    set res [http client {*}$test_addr -port $port -method $method \
        -target "/"                                                \
        -body {big body}                                           \
        -headers {X-Head tail}]
    kill $port
    lindex $res 0
} -result 200

test put_all {POST: test all} -body {
    set method put
    set port [rand_port]
    set fail {proc fail {args} {list 500 \$args ""}\n}
    set success {proc success {} {list 200 "" ""}\n}
    background $port [string cat $fail $success [check_target_body_headers $method]]
    set res [http client {*}$test_addr -port $port -method $method \
        -target "/"                                                \
        -body {big body}                                           \
        -headers {X-Head tail}]
    kill $port
    lindex $res 0
} -result 200

test set_response_headers {test response headers} -body {
    set method get
    set port [rand_port]
    background $port [set_resp_headers $method]
    set res [http client {*}$test_addr -port $port -method $method \
        -target "/"]
    kill $port
    return "[dict get [lindex $res 1] X-Head-1] [dict get [lindex $res 1] X-Head-2]"
} -result {val1 val2}


cleanupTests
