#!/usr/bin/perl -w

@files=`find . -name "*.S"`;
chomp(@files);
foreach $f (@files) {
	$o = $f;
	$o =~ s/\.S$/\.o/;
	print " $o " if (-e $o);
}
