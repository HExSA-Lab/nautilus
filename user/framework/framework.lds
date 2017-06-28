ENTRY(__nk_exec_entry)

SECTIONS
{
    . = 0x0;  /* we will relocate in all cases */


    .text : ALIGN(0x1000) {
        *(.mbhdr)
	. = ALIGN(0x1000);
	/* The actual code begins at 0x1000 / offset 0x2000 into file */
    	_loadStart = .;
        *(.text*)
        *(.gnu.linkonce.t*)
    }

    .data : ALIGN(0x1000) {
        *(.data*)
        *(.gnu.linkonce.d*)
    }
    
    .rodata : ALIGN(0x1000) {
        *(.rodata*)
        *(.gnu.linkonce.r*)
    }
    
    .got : ALIGN(0x1000) {
        *(.got*)
        *(.gnu.linkconce.got*)
    }

    _loadEnd = .; 
    
    .bss : ALIGN(0x1000) {
        *(COMMON)
        *(.bss*)
        *(.gnu.linkonce.b*)
    }
    
    _bssEnd = .; 
    
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
	
    }
}

