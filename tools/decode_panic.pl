#!/usr/bin/perl -w
#

$do_edit = $#ARGV>=0 ;

# name of the file that includes the panic
$serial = "./serial.out";
# the kernel binary
$kernel = "./nautilus.bin";
# where to find the source tree
$tree =   "./";
# the prefix from the decoded file locations to remove
$cut =    "/home/pdinda/test/nautilus/";
# the editor you want to use to traverse the panic
$editor = "vim";
# how to tell that editor to start at a particular line
$lineno_prefix = "+";

(-e $kernel) or die "no kernel file $kernel\n";
(-e $serial) or die "no serial file $serial\n";

open(S,$serial) or die "cannot open serial file $serial\n";

while (<S>) {
    if (/\[.+\]\s+RIP:\s+(\S+)/ || /RIP:\s+\S+\:(\S+)/) {                                                           
	$seq .= $1." ";
	push @rips, $1;
    }
}

defined($seq) or die "No panic detected in $serial\n";                                                            

close(S);

@locs = `addr2line -e $kernel $seq`;

chomp(@locs);

# print the raw decoded stack trace trace
print map { "$rips[$_]\t$locs[$_]\n" } 0..$#locs;


if ($do_edit) {
    # walk the raw decoded stack trace back
    # file by file
    foreach $l (@locs) {
	($file,$line) = split(/\:/,$l);
	$file =~ s/^$cut//g;
	$cmd = "$editor $tree$file $lineno_prefix$line";                                                        
	#       print $cmd,"\n";
	system $cmd;
    }
}
