#!/usr/bin/perl -w

while (<STDIN>) {
    $sym{$1} = 1 if (/undefined reference to `(.*)'/);
}

print map { "GEN_DEF($_)\n" } sort keys %sym;

    
