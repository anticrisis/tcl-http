#
# This sample was created for performance testing of database writes and reads.
# It does not actually provide a working user database.
#
package require sqlite3
package require act_http
namespace import act::*

if {$argc < 2} {
    puts "Usage: $argv0 -host host -port port"
    exit 1
}


namespace eval ::userdb {}

proc userdb::open {filename} {
    sqlite3 ::db $filename
}

proc userdb::close {} {
    # optional: recommended analysis settings
    db eval {pragma analysis_limit=100}
    db eval {pragma optimize}
    db close
}

proc userdb::ensure {filename} {
    if {[info commands db] eq ""} {::userdb::open $filename}
}

proc userdb::create {filename} {
    ensure $filename

    # optional: recommended security settings
    db config trusted_schema off
    db config enable_view off
    db config enable_trigger off
    db config defensive 1
    db config dqs_dml 0
    db config dqs_ddl 0

    # recommended: enable write-ahead log for performance
    db eval {pragma journal_mode=WAL}

    db eval {drop table if exists users}
    db eval {create table users(username text primary key not null,
        plainpass text not null)}
    }

    proc userdb::now {}  {db eval {select datetime('now')}}
    proc userdb::uuid {} {db eval {select lower(hex(randomblob(16)))}}

    proc userdb::create_user {username plainpass} {
    db eval {insert into users values(:username, :plainpass)}
}

proc userdb::valid_plainpass {username checkpass} {
    set res [db eval {select plainpass from users where username=:username}]
    return [expr {$res eq $checkpass}]
}

# ---------------------------------------------------------------------------

set dbfile "test-userdb.sqlite"

userdb::create $dbfile

namespace eval web {
    variable target
}

proc web::create_user {} {
    set random_name [userdb::uuid]
    set random_pass [userdb::uuid]
    userdb::create_user $random_name $random_pass
    list 200 "Created user $random_name with pass $random_pass" "text/plain"
}

proc web::check_password {} {
    set random_name [userdb::uuid]
    set random_pass [userdb::uuid]
    set result [userdb::valid_plainpass $random_name $random_pass]
    list 200 "Checked user $random_name pass $random_pass: $result" "text/plain"
}

proc web::default {} {
    list 200 "Try paths /create or /check" "text/plain"
}

proc web::handle_get {} {
    variable target
    switch $target {
        /create {create_user}
        /check  {check_password}
        default {default}
    }
}

http configure -get web::handle_get -reqtargetvar web::target {*}$argv

puts "Listening on http://[http configure -host]:[http configure -port]"
http run
