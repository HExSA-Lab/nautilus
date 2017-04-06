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
 * Copyright (c) 2017, Yingyi Luo, Guixing Lin, and Jinghang Wang
 * Copyright (c) 2017, Peter Dinda
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Yingyi Luo <YingyiLuo2015@u.northwestern.edu>
 *           Guixing Lin <GuixingLin2018@u.northwestern.edu>
 *           Jinghang Wang <JinghangWang2018@u.northwestern.edu>
 *           Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __FS_FAT32_H__
#define __FS_FAT32_H__

int nk_fs_fat32_attach(char *devname, char *fsname, int readonly);
int nk_fs_fat32_detach(char *fsname);

#endif
