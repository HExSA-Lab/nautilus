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
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2016, Peter Dinda
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/dev.h>
#include <dev/vesa.h>
#include <nautilus/realmode.h>

#ifndef NAUT_CONFIG_DEBUG_VESA
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("vesa: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("vesa: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("vesa: " fmt, ##args)

static spinlock_t state_lock;
static int in_progress = 0;

static vesa_mode_t orig_mode;
static vesa_mode_t cur_mode;
static struct vesa_adapter_info adapter_info;
static vesa_mode_t *modes_start=0;
static struct vesa_mode_info    cur_mode_info;


static void enumerate_modes();

static struct nk_dev_int ops = {
    .open=0,
    .close=0,
};

int vesa_init()
{
    INFO("init\n");
    if (vesa_get_adapter_info(&adapter_info)) { 
	ERROR("Failed to get adapter info at init - probably not a VESA card\n");
	return -1;
    }
    INFO("version %04x capabilities %08x memory %lu KB\n",
	 adapter_info.version,
	 adapter_info.capabilities,
	 adapter_info.video_memory*64);
    INFO("oem %s vendor %s product_name %s software_rev %04x product_rev %s\n",
	 VESA_PTR_TO_LINEAR(adapter_info.oem),
	 VESA_PTR_TO_LINEAR(adapter_info.vendor),
	 VESA_PTR_TO_LINEAR(adapter_info.product_name),
	 adapter_info.software_rev,
	 VESA_PTR_TO_LINEAR(adapter_info.product_rev));
    INFO("modes at %04x:%04x (%p)\n",
	 VESA_PTR_TO_SEG(adapter_info.video_modes),
	 VESA_PTR_TO_OFF(adapter_info.video_modes),
	 VESA_PTR_TO_LINEAR(adapter_info.video_modes));

    if (VESA_PTR_TO_SEG(adapter_info.video_modes)==NAUT_CONFIG_REAL_MODE_INTERFACE_SEGMENT) {
	// the data was copied where we asked, so we need to keep in mind that we copied
	// it to a final destination when computing where the mode table is
	if (VESA_PTR_TO_OFF(adapter_info.video_modes)<0x8000) {
	    ERROR("Mode list is in non-user portion of interface segment...\n");
	    return -1;
	}
	if (VESA_PTR_TO_OFF(adapter_info.video_modes)>=0x8000+sizeof(struct vesa_adapter_info)) {
	    ERROR("Mode list is beyond the adapter info structure...\n");
	    return -1;
	}
	modes_start = (vesa_mode_t *)(uint64_t)&adapter_info + VESA_PTR_TO_OFF(adapter_info.video_modes) - 0x8000;
    } else {
	// We better hope it is pointing into the ROM...
	if (VESA_PTR_TO_SEG(adapter_info.video_modes)==0xc000) {
	    // if it's in the rom, then we just need to take note, since 
	    // it will not change
	    modes_start = (vesa_mode_t *)(uint64_t)VESA_PTR_TO_LINEAR(adapter_info.video_modes);
	} else {
	    ERROR("Mode list is stored outside of ROM and outside of our transfer segment (%04x:%04x)...\n",VESA_PTR_TO_SEG(adapter_info.video_modes),VESA_PTR_TO_OFF(adapter_info.video_modes));
	    return -1;
	}
    }
	    
    enumerate_modes();

    if (vesa_get_cur_mode(&orig_mode)) { 
	ERROR("Failed to get current mode at init - weird\n");
	return -1;
    }
    cur_mode=orig_mode;

    if (vesa_get_mode_info(cur_mode,&cur_mode_info)) { 
	ERROR("Failed to get current mode info at init - weird\n");
	return -1;
    }

    nk_dev_register("vesa", NK_DEV_GENERIC,0,&ops,0);

    return 0;
}

void vesa_deinit()
{
    INFO("deinit\n");
    vesa_set_cur_mode(orig_mode);
}



typedef enum {ENUM, FIND} scan_action;

static int _scan_modes(struct vesa_mode_request *r, vesa_mode_t *mode, scan_action s)
{
    vesa_mode_t *cur;
    struct vesa_mode_info m;

    for (cur = modes_start; *cur!=0xffff; cur++) { 
	if (vesa_get_mode_info(*cur,&m)) {
	    ERROR("Failed to get info for mode %x\n",*cur);
	    return -1;
	}
	switch (s) {
	case ENUM:
	    INFO("mode %04x:\n",*cur);
	    INFO("     attr=%08x w=%u h=%u\n",
		 m.attributes,
		 (uint32_t)m.width,
		 (uint32_t)m.height);
	    INFO("     bpp=%u pitch=%u\n",
		 (uint32_t)m.bpp,
		 (uint32_t)m.pitch);
	    INFO("     text=%d color=%d lfb=%d fb=%p\n", 
		 !(m.attributes & VESA_ATTR_GRAPHICS),
		 !!(m.attributes & VESA_ATTR_COLOR),
		 !!(m.attributes & VESA_ATTR_LINEARFB),
		 (void*)(uint64_t)m.framebuffer);
	    break;
	case FIND:
	    DEBUG(" examine mode %04x:\n",*cur);
	    DEBUG("     attr=%08x w=%u h=%u\n",
		  m.attributes,
		  (uint32_t)m.width,
		  (uint32_t)m.height);
	    DEBUG("     bpp=%u pitch=%u\n",
		  (uint32_t)m.bpp,
		  (uint32_t)m.pitch);
	    DEBUG("     text=%d color=%d lfb=%d fb=%p\n", 
		  !(m.attributes & VESA_ATTR_GRAPHICS),
		  !!(m.attributes & VESA_ATTR_COLOR),
		  !!(m.attributes & VESA_ATTR_LINEARFB),
		  (void*)(uint64_t)m.framebuffer);
	    if ((m.attributes & VESA_ATTR_HWSUPPORT) &&
		(m.attributes & VESA_ATTR_COLOR) &&
		(m.width == r->width) &&
		(m.height == r->height) &&
		(r->text || m.bpp == r->bpp) &&
		(((m.attributes & VESA_ATTR_GRAPHICS) && !r->text && (m.attributes & VESA_ATTR_LINEARFB)) ||
		 (!(m.attributes & VESA_ATTR_GRAPHICS) && r->text))) {

		DEBUG(" FOUND mode %04x:\n",*cur);
		DEBUG("     attr=%08x w=%u h=%u\n",
		      m.attributes,
		      (uint32_t)m.width,
		      (uint32_t)m.height);
		DEBUG("     bpp=%u pitch=%u\n",
		      (uint32_t)m.bpp,
		      (uint32_t)m.pitch);
		DEBUG("     text=%d color=%d lfb=%d fb=%p\n", 
		      !(m.attributes & VESA_ATTR_GRAPHICS),
		      !!(m.attributes & VESA_ATTR_COLOR),
		      !!(m.attributes & VESA_ATTR_LINEARFB),
		      (void*)(uint64_t)m.framebuffer);
		
		*mode = *cur;
		return 0;
	    }
	    break;
	}
    }

    switch (s) {
    case ENUM:
	return 0;
	break;
    case FIND:
    default:
	return -1;
	break;
    }
}

static void enumerate_modes()
{
    _scan_modes(0,0,ENUM);
}

int vesa_find_matching_mode(struct vesa_mode_request *r, vesa_mode_t *mode)
{
    return _scan_modes(r,mode,FIND);
}


static int _vesa_doit(struct nk_real_mode_int_args *r, void *esdidata, int count)
{
    r->vector = 0x10;

    if (nk_real_mode_start()) { 
	ERROR("real mode start failed\n");
	return -1;
    } else {
	if (esdidata) {
	    memcpy(REAL_TO_LINEAR(r->es,r->di),esdidata,count);
	}
	if (nk_real_mode_int(r)) { 
	    ERROR("real mode int failed\n");
	    nk_real_mode_finish();
	    return -1;
	} else {
	    // success
	    return 0;
	}
    }
}

static int _vesa_done()
{
    return nk_real_mode_finish();
}
	

int vesa_get_cur_mode(vesa_mode_t *mode)
{
    struct nk_real_mode_int_args r;

    nk_real_mode_set_arg_defaults(&r);
    
    r.ax = 0x4f03;
    
    if (_vesa_doit(&r,0,0)) { 
	ERROR("failed to execute vesa call\n");
	return -1;
    }

    if (r.ax != 0x004f) { 
	ERROR("vesa call reports failure (%04x)\n",r.ax);
	_vesa_done();
	return -1;
    }
    
    *mode = r.bx;

    _vesa_done();

    DEBUG("vesa mode is 0x%04x\n", *mode);

    return 0;
}

int vesa_get_adapter_info(struct vesa_adapter_info *info)
{
    struct nk_real_mode_int_args r;

    nk_real_mode_set_arg_defaults(&r);
    
    r.ax = 0x4f00;
    r.di = 0x8000;   // write data in the real mode seg
    
    if (_vesa_doit(&r,"VBE2",4)) { 
	ERROR("failed to execute vesa call\n");
	return -1;
    }

    if (r.ax != 0x004f) { 
	ERROR("vesa call reports failure (%04x)\n",r.ax);
	_vesa_done();
	return -1;
    }
    
    memcpy(info,(void*)(uint64_t)(REAL_TO_LINEAR(r.es,r.di)),sizeof(struct vesa_adapter_info));

    _vesa_done();

    if (memcmp(info->signature,"VESA",4)) {
	ERROR("vesa adapter structure does not have correct signature\n");
	return -1;
    }

    DEBUG("vesa adapter info complete\n");

    return 0;
}
    

int vesa_get_mode_info(vesa_mode_t mode, struct vesa_mode_info *info)
{
    struct nk_real_mode_int_args r;

    nk_real_mode_set_arg_defaults(&r);
    
    r.ax = 0x4f01;
    r.cx = mode;
    r.di = 0x8000;   // write data in the real mode seg
    
    if (_vesa_doit(&r,0,0)) { 
	ERROR("failed to execute vesa call\n");
	return -1;
    }

    if (r.ax != 0x004f) { 
	ERROR("vesa call reports failure (%04x)\n",r.ax);
	_vesa_done();
	return -1;
    }
    
    memcpy(info,(void*)(uint64_t)(REAL_TO_LINEAR(r.es,r.di)),sizeof(struct vesa_mode_info));

    _vesa_done();

    DEBUG("vesa mode info complete\n");

    return 0;
}


int vesa_set_cur_mode(vesa_mode_t mode)
{
    struct nk_real_mode_int_args r;

    nk_real_mode_set_arg_defaults(&r);
    
    r.ax = 0x4f02;
    r.bx = mode;
    r.di = 0x8000;   // CRTC data if we ever support it

    if (r.cx & VESA_MODE_USER_CRTC) { 
	ERROR("Do not support user crtc!\n");
	return -1;
    }
    
    if (_vesa_doit(&r,0,0)) { 
	ERROR("failed to execute vesa call\n");
	return -1;
    }

    if (r.ax != 0x004f) { 
	ERROR("vesa call reports failure (%04x)\n",r.ax);
	_vesa_done();
	return -1;
    }
    
    _vesa_done();

    cur_mode = mode;

    if (vesa_get_mode_info(cur_mode,&cur_mode_info)) { 
	ERROR("Failed to get mode data on mode switch\n");
	return -1;
    }

    DEBUG("vesa mode set complete\n");

    return 0;
}

inline void vesa_draw_pixel(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t *fb = (uint8_t*) (uint64_t) cur_mode_info.framebuffer;
    unsigned width = cur_mode_info.width;
    uint16_t height = cur_mode_info.height;
    uint8_t bpp = cur_mode_info.bpp;
    uint16_t pitch = cur_mode_info.pitch;
    uint32_t pixel = (((uint32_t)r)<<16) + (((uint32_t)g)<<8) + (((uint32_t)b));
    
    if (bpp!=32) { 
	ERROR("Can only draw in 32 bit color at this point\n");
    }
    
    fb += pitch * y + 4 * x;
    
    //DEBUG("Draw Pixel: (%x,%x)=%08x target=%p\n", x,y,pixel, fb);

    *(uint32_t *)fb = pixel;
}

void vesa_test()
{
    vesa_mode_t mode, orig_mode;
    struct vesa_mode_request r;

    if (vesa_get_cur_mode(&orig_mode)) {
	ERROR("Failed to get current mode\n");
	return;
    }

    INFO("Original mode is %04x\n",orig_mode);
    
    INFO("Looking for a 80x60 text mode\n");

    r.width=80;
    r.height=60;
    r.text=1;
    r.lfb=0;
    
    if (vesa_find_matching_mode(&r,&mode)) { 
	INFO("Cannot find such a text mode\n");
    } else {
	mode = mode | VESA_MODE_DONT_CLEAR;
	if (vesa_set_cur_mode(mode)) {
	    ERROR("Failed to set current mode\n");
	    return;
	}
	
	INFO("We should now be in 80x60 text mode\n");
	udelay(5000000);
	mode = orig_mode | VESA_MODE_DONT_CLEAR;
	if (vesa_set_cur_mode(mode)) {
	    ERROR("Failed to set current mode\n");
	    return;
	}
	INFO("We should now be back to 80x25 text mode\n");
    }

    r.width=1024;
    r.height=768;
    r.bpp=32;
    r.text=0;
    r.lfb=1;

    INFO("Looking for a 1024x768x32 graphics mode\n");

    if (vesa_find_matching_mode(&r,&mode)) { 
	INFO("Cannot find a matching mode\n");
    } else {
	INFO("Found mode - now switching to it\n");
	if (vesa_set_cur_mode(mode)) {
	    ERROR("Failed to set current mode\n");
	    return;
	}

	INFO("Now drawing\n");

	uint16_t n,i,j;
	for (n=0;n<4;n++) {
	    for (i=n*(768/4);i<(n+1)*(768/4);i++) { 
		for (j=0;j<1024;j++) {
		    if (n==0) {
			vesa_draw_pixel(j,i,j/(1024/256),0,0);
		    } else if (n==1) { 
			vesa_draw_pixel(j,i,0,j/(1024/256),0);
		    } else if (n==2) { 
			vesa_draw_pixel(j,i,0,0,j/(1024/256));
		    } else if (n==3) { 
			vesa_draw_pixel(j,i,j/(1024/256),j/(1024/256),j/(1024/256));
		    }
		}
	    }
	}
	INFO("Done drawing, now waiting\n");
	udelay(5000000);
    }


    INFO("Switching back to original mode\n");

    if (vesa_set_cur_mode(orig_mode)) {
	ERROR("Failed to set current mode\n");
	return;
    }

    INFO("We should now be back to the original mode again\n");

}


	
