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

        boot_stack_start = .;
        . += 0x200000;  /* we start out with a 2M stack */
        boot_stack_end = .;
    }


    .text ALIGN(0x1000) : 
    {
        *(.text*)
        *(.gnu.linkonce.t*)
    }

    .init ALIGN(0x1000) : AT(ADDR(.text) + SIZEOF(.text))
    {
        *(.init)
        *(.gnu.linkonce.init)
    }

    .fini ALIGN(0x1000) : AT(ADDR(.init) + SIZEOF(.init))
    {
        *(.fini)
        *(.gnu.linkonce.fini)
    }

    .init_array ALIGN(0x1000) : AT(ADDR(.fini) + SIZEOF(.fini))
    {
        _init_array_start = .;
        *(.init_array*)
        _init_array_end = .;
        *(.gnu.linkonce.init_array*)

    }


    .gcc_except_table ALIGN(0x1000) : AT(ADDR(.init_array) + SIZEOF(.init_array))
    {
        *(.gcc_except_table*)
        *(.gnu.linkonce.gcc_except*)
    }

    _data_start = . ;    

    .data ALIGN(0x1000) : AT(ADDR(.gcc_except_table) + SIZEOF(.gcc_except_table))
    {
        *(.data*)
        *(.gnu.linkonce.d*)
    }

    .tests ALIGN(0x1000) : AT(ADDR(.data) + SIZEOF(.data))
    {
        __start_tests = .;
        *(.tests*);
        __stop_tests = .;
    }

    .cmdline_flags ALIGN(0x1000) : AT(ADDR(.tests) + SIZEOF(.tests))
    {
        __start_flags = .;
        *(.cmdline_flags*);
        __stop_flags = .;
    }
    
    .rodata ALIGN(0x1000) : AT(ADDR(.cmdline_flags) + SIZEOF(.cmdline_flags))
    {
        *(.rodata*)
        *(.gnu.linkonce.r*)
    }

    .got ALIGN(0x1000) : AT(ADDR(.rodata)+SIZEOF(.rodata))
    {
        *(.got*)
        *(.gnu.linkconce.got*)
    }

    .shell_cmds ALIGN(0x1000) : AT(ADDR(.got)+SIZEOF(.got))
    {
        __start_shell_cmds = .;
        *(.shell_cmds*);
        __stop_shell_cmds = .;
    }

    .aspace_impls ALIGN(0x1000) : AT(ADDR(.shell_cmds)+SIZEOF(.shell_cmds))
    {
        __start_aspace_impls = .;
        *(.aspace_impls*);
        __stop_aspace_impls = .;
    }

    _loadEnd = .; 
    
    .bss ALIGN(0x1000) : AT(ADDR(.aspace_impls)+SIZEOF(.aspace_impls))
    {
        *(COMMON)
        *(.bss*)
        *(.gnu.linkonce.b*)
    }
    
    _bssEnd = .; 

    _data_end = .;
    
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
    }
}

