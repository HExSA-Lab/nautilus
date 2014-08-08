ENTRY(nautilus_entry)

/*VIRT_BASE = 0xffffffff80000000; */

SECTIONS
{
    . = 0x100000;

    .boot :
    {
        *(.mbhdr)
        _loadStart = .;
        *(.boot)
        . = ALIGN(4096);
        pml4 = .;
        . += 0x1000;
        pdpt = .;
        . += 0x1000;
        pd = .;
        . += 0x1000; 
        /*pt = .;
        . += 0x1000; */
        . += 0x4000;
        boot_stack = .;
        . += 0x1000; /* TODO: remove this, for now to be safe... */
    }

    /*. += VIRT_BASE;*/

    .text ALIGN(0x1000) : 
    {
        *(.text)
        *(.gnu.linkonce.t*)
    }
    
    .data ALIGN(0x1000) : AT(ADDR(.text) + SIZEOF(.text))
    {
        *(.data)
        *(.gnu.linkonce.d*)
    }
    
    .rodata ALIGN(0x1000) : AT(ADDR(.data) + SIZEOF(.data))
    {
        *(.rodata*)
        *(.gnu.linkonce.r*)
    }
    
    _loadEnd = .; /*- VIRT_BASE; */
    
    .bss ALIGN(0x1000) : AT(ADDR(.rodata)+SIZEOF(.rodata))
    {
        *(COMMON)
        *(.bss)
        *(.gnu.linkonce.b*)
    }
    
    _bssEnd = .; /*- VIRT_BASE; */
    
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
    }
}

