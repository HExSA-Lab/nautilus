# Debugging Nautilus with GDB and QEMU

To use the GDB functionality in Nautilus (implemented in `src/nautilus/gdb-stub.c`), 
configure with remote debugging on, 
and a debug serial port on.  For typical testing, we 
have the debug serial port on COM1 and the serial redirect/log
output port on COM2.  You typically also want to configure Nautilus
to attach to the debugger at boot.  To do source level debugging
Nautilus needs to be compiled with debugging symbols (also in the config).   Note that
we typically compile with optimization as well so that back-to-source
mapping is rarely perfect. 

Assuming you are attaching the debugger at boot, and you are
working on QEMU, then configure the QEMU machine to put the
debug serial port somewhere that is accessible, for example:

```

qemu-system-x86_64 -vga std -smp 4 -m 2048 \
    -chardev socket,host=localhost,port=5000,id=g,server \
    -device isa-serial,chardev=g \
    -chardev stdio,id=d \
    -device isa-serial,chardev=d \
    -cdrom nautilus.iso \
```

This places COM1 behind a tcp server at port 5000. Then bring up gdb:

```
$ gdb nautilus.bin
GNU gdb (Ubuntu 7.11.1-0ubuntu1~16.5) 7.11.1
...
(gdb)
```

And attach

```
(gdb) target remote localhost:5000
```

At this point, you'll see Nautilus boot and then pause at "attaching 
to debugger now".  And gdb will show you something like:

```
(gdb) target remote localhost:5000
'/home/pdinda/test/nautilus/nautilus.bin' has changed; re-reading symbols.
Remote debugging using localhost:5000
nk_gdb_init () at src/nautilus/gdb-stub.c:1349
1349	}
(gdb) 
```

You can now set breakpoints, etc.   When you wan the boot to continue,
just type continue as usual:

```
(gdb) continue
```

The debugger will regain control under the following situations:

    - An exception occurs on any cpu
    - A breakpoint is hit on any cpu
    - `nk_gdb_breakpoint_here()` is hit on any cpu
    - you have the shell and use its `break` command
    - you have PS2 support and hit the debug key
    (currently F8) on the keyboard
    - you have debug interrupt support for the 
    debug serial driver (this is on by default) and
    you hit CTRL-C in GDB.

For example, hitting CTRL-C in gdb:

```
(gdb) continue
Continuing.
^C[New Thread 2]
[New Thread 3]
[New Thread 4]

Thread 1 received signal SIGINT, Interrupt.
0x0000000000321288 in halt () at include/nautilus/cpu.h:298
298	    asm volatile ("hlt");
(gdb) 
```

The model here is that each CPU is a thread.  When the debugger
regains control, all CPUs are stopped.  

For a 4 CPU  setup, you might see something like this:

```
(gdb) info threads
Id   Target Id         Frame 
* 1    Thread 1 (cpu0)   0x0000000000321288 in halt () at include/nautilus/cpu.h:298
2    Thread 2 (cpu1)   msr_read (msr=msr@entry=3221225729) at src/nautilus/msr.c:37
3    Thread 3 (cpu2)   0x0000000000321288 in halt () at include/nautilus/cpu.h:298
4    Thread 4 (cpu3)   io_delay () at include/nautilus/cpu.h:341
```

And then we can see what's up on each CPU:

```
(gdb) bt
#0  0x0000000000321288 in halt () at include/nautilus/cpu.h:298
#1  idle (in=<optimized out>, out=<optimized out>) at src/nautilus/idle.c:85
#2  0x0000000000000000 in ?? ()

[Halting in idle()]

(gdb) thread 2
[Switching to thread 2 (Thread 2)]
#0  msr_read (msr=msr@entry=3221225729) at src/nautilus/msr.c:37
37	{
    (gdb) bt
#0  msr_read (msr=msr@entry=3221225729) at src/nautilus/msr.c:37
#1  0x000000000033ac9e in __cpu_state_get_cpu () at include/nautilus/cpu_state.h:43
#2  preempt_is_disabled () at include/nautilus/cpu_state.h:84
#3  handle_special_switch (what=YIELDING, have_lock=0, flags=0 '\000', 
    lock_to_release=0x102028) at src/nautilus/scheduler.c:2636
#4  0x000000000032127b in idle (in=<optimized out>, out=<optimized out>)
at src/nautilus/idle.c:75
#5  0x0000000000000000 in ?? ()

[Yielding in idle()]

(gdb) thread 4 
    [Switching to thread 4 (Thread 4)]
#0  io_delay () at include/nautilus/cpu.h:341
    341	    asm volatile("outb %%al,%0" : : "dN" (DELAY_PORT));
        (gdb) bt
#0  io_delay () at include/nautilus/cpu.h:341
#1  udelay (n=896) at include/nautilus/cpu.h:357
#2  burner (in=0x10b400, out=<optimized out>) at src/nautilus/shell.c:113
#3  0x0000000000325570 in ?? () at src/nautilus/thread.c:601
#4  0x0000000000000000 in ?? ()
```

[Doing a delay in a test "burner" thread]


## Caveats
- per-cpu variables are problematic since fsbase/gsbase has only 
    recently been supported in GDB, and this code defaults to not
    including them.   If you enable fsbase/gsbase for use with 
    GDB 8+, note that this may still not handle them correctly.   
    The best approach is probably to use the global array index
    `system->cpus[i]`... instead
- don't change the segment registers from gdb (there is no need to
    ever do this in Nautilus anyway).   Other registers are fair game.
- there is no current support for floating point state.   It does 
convey x87 and base SSE regs as required, but these are not 
populated.   Easy to add if necessary at some point.  Same thing
for advanced SSE, AVX, AVX*, etc. 
- it is possible to avoid attaching gdb at startup (you can 
  disable this option).   When you do attach gdb (`target remote`)
  you will need to have it send the interrupt (CTRL-C), though,
  or else interrupt gdb through some other means (F8, intentional
  breakpoint, etc).   There is nothing "special" about attaching
  gdb at boot - an attach is simply the result of processing any
  exception. 
