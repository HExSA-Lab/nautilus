#!/usr/bin/perl -w

$#ARGV==0 or die "Decodes guest RIP addresses against a guest kernel dissassembly file\nThis tells you which functions in the guest kernel are being used\nusage: guest_kernel_calls.pl disassmfile < RIPS\n";

open(K,shift);

@k = <K>;

close(K);

while (<STDIN>) { 
  if (/\[.*?\] RIP: 0x0*(\S+)/) { 
    $addr=$1;
    chomp($addr);
    print join("\t",$addr,findit($addr)),"\n";
  }
}


sub findit {
  my $addr=shift;
  my $i;
  my $line=-1;
  my $funcline=-1;
  my $funcaddr;
  my $funcname;

  if (0) {
    return "USER";
  } else {
    # search forward
    for ($i=0;$i<=$#k;$i++) { 
      if ($k[$i] =~ /\s*(\S+):/) {
	$x=$1;
	if ($x eq $addr) { 
	  $line=$i;
	  last;
	}
      }
    }
    if ($line<0) {
      return "CANNOT FIND IN DISASSEMBLY";
    } else {
      # search backward
      for ($i=$line;$i>=0;$i--) { 
	if ($k[$i] =~ /^(\S+)\s\<(\S+)\>:/) {
	  $funcline=$i;
	  $funcname=$2; 
	  $funcaddr=$1;
	  last;
	}
      } 
      if ($funcline<0) { 
	return "CANNOT FIND FUNCTION IN DISASSEMBLY";
      } else {
	return $funcname." at ".$funcaddr;
      }
    }
  }
}
      
	  
    
  
    
