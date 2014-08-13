ENTRY(nautilus_entry)

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

        /* stack */
        . += 0x4000;
        boot_stack = .;
        . += 0x1000; /* TODO: remove this, for now to be safe... */
    }


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
    
    _loadEnd = .; 
    
    .bss ALIGN(0x1000) : AT(ADDR(.rodata)+SIZEOF(.rodata))
    {
        *(COMMON)
        *(.bss)
        *(.gnu.linkonce.b*)
    }
    
    _bssEnd = .; 
    
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
    }
}

