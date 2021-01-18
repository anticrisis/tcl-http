proc swap {a b} {
    # efficient swap of two variables
    uplevel {set a $b[set b $a; list]}
}

proc rand_int {a b} {
    # return random int in range [a, b)
    if {$b < $a} {swap a b}
    return [expr round(floor(rand() * ($b - $a))) + $a]
}

proc rand_port {} {rand_int 8000 50000}
