#!/usr/bin/python
import os
import sys


def gen_line_str(entry_type, img_path, cmdline=""):
    print "\t" + entry_type + " " + img_path + " " + cmdline

def gen_menuentry(timeout, img_path, sym_path, cmdline):

    print "set timeout=" + timeout
    print "set default=0"

    print "menuentry \"Nautilus\" {"

    gen_line_str("multiboot2", img_path, cmdline)
    gen_line_str("module2", sym_path)

    print "\tboot"
    print "}"


cmdline = ' '.join(sys.argv[1:])
gen_menuentry("0", "/boot/nautilus.bin", "/boot/nautilus.syms", cmdline)

    
    



    

