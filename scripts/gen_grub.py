#!/usr/bin/python
import os
import sys


def gen_line_str(entry_type, img_path, cmdline=""):
    print "\t" + entry_type + " " + img_path + " " + cmdline

def gen_menuentry(timeout, img_path, mod_paths, cmdline):

    print "set timeout=" + timeout
    print "set default=0"

    print "menuentry \"Nautilus\" {"

    gen_line_str("multiboot2", img_path, cmdline)

    for mod_path in mod_paths:
        gen_line_str("module2", mod_path)

    print "\tboot"
    print "}"


cmdline = ' '.join(sys.argv[1:])
image = "/boot/nautilus.bin"
modules = ["/boot/nautilus.syms", "/boot/nautilus.secs"]

gen_menuentry("0", image, modules, cmdline)
