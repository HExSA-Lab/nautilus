.align 4096
.global __RAMDISK_START 
__RAMDISK_START:
.incbin "ramdisk.img"
.global __RAMDISK_END 
__RAMDISK_END:
