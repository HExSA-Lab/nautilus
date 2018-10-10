/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2018, Conghao Liu
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 *
 * Authors:  Conghao Liu <cliu115@hawk.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/getopt.h>

int optopt;
int opterr;//so far not used
int optind = 1;
char* optarg;

int 
getopt (int argc, char*const* argv,const char* options) {

    if (optind >= argc) {//already reach the end of input args
        return -1;
    }
    
    // begining of main logic
    // if current arg start without '-', find out the next arg that start with '-'
    while (argv[optind][0] != '-') {
        optind++;
        if (optind >= argc) {
            return -1;
        }
    }

    // check if we found a valid opt char
    for (int i = 0; options[i] != 0; i++) {
        if (argv[optind][1] != options[i]) {
            continue;
        }
        // check if this option needs a value
        if (options[i+1] == ':') {

            // find out the location of the value. It could be the remaining string
            // of this list, or it could be the next argv list
            if (argv[optind][2] != 0) {//we found it
                optarg = &argv[optind][2];
                optind++;
                return options[i];
            }

            // nope, we have to check if next argv list exists
            if (++optind >= argc) {//gg, no value error 
               optopt = options[i];
               return '?';
            }

            // we found it!
            optarg = argv[optind++];
            return options[i];
        }
        // this option doesn't need a value
        optind++;
        return options[i];
    }

    //no valid opt found, should setup optopt 
    optopt = argv[optind][1];
    optind++;
    return '?';
}
