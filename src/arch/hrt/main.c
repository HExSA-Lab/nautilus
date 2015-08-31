#define __NAUTILUS_MAIN__

#include <nautilus/mb_utils.h>
#include <arch/hrt/init.h>


/*
 * 2nd two parameters are 
 * only used if this is an HRT 
 */
void 
main (unsigned long mbd, 
      unsigned long magic, 
      unsigned long mycpuid, 
      unsigned long apicid)

{
    if (mb_is_hrt_environ(mbd)) {

        if (mycpuid == 0) {
            hrt_bsp_init(mbd, magic, mycpuid);
        } else {
            hrt_ap_init(mbd, mycpuid);
        }

    } else {
        /* fall back to normal init */
        default_init(mbd, magic, mycpuid);
    }
}
