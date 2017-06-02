```

    _   __               __   _  __                
   / | / /____ _ __  __ / /_ (_)/ /__  __ _____    
  /  |/ // __ `// / / // __// // // / / // ___/ 
 / /|  // /_/ // /_/ // /_ / // // /_/ /(__  )     
/_/ |_/ \__,_/ \__,_/ \__//_//_/ \__,_//____/ **with Lua**


```
[![Nautilus](https://img.shields.io/travis/rust-lang/rust.svg)]()
[![Lua Version](https://img.shields.io/badge/Lua-5.2.4-blue.svg)](https://www.lua.org/source/5.2/)
[![py ver](https://img.shields.io/badge/Python-3.6-yellow.svg)](https://www.python.org/downloads/)

# Lua interpreter for Nautilus AeroKernel  

Nautilus is an example of an AeroKernel, a very thin kernel-layer exposed 
(much like a library OS, or libOS) directly to the runtime and application. 
In order to provide the user-space like facility to perform tests on nautilus' 
implementation an interpreted language viz. Lua has been ported to work on nautilus. 

# Getting Started

## Prerequisites

- git 
- gcc cross compiler (more on this to come)
- grub version >= 2.02
- xorriso (for CD burning)
- QEMU 
- Python >= 3.6


## Hardware Support

Nautilus works with the following hardware:

- x86_64 machines (AMD and Intel)

## Dowloading the source and Building

First,get the source of Nautilus that has LUA already ported by running 

`$> git clone https://github.com/goutkannan/Nautilus-Lua.git <destination folder>`

The main repo for Nautilus is at <https://bitbucket.org/kchale/nautilus>. Fetch the code by running

`$> git clone https://goutkannan@bitbucket.org/kchale/nautilus.git`


## Make Menu Selection

First, configure Nautilus by running

`make menuconfig`

In order to pre-load lua with Nautilus, choose the following options 
Under the `Lua for Nautilus` categor, select the options 1.Lua Load 2.Lua to test by pressing the `y` button. Incase of second option the debug symbols option present under `Debugging ---> Compile with Debug Symbols' should be enabled.

![Menu_init](https://github.com/goutkannan/Nautilus-LUA/blob/master/img/Topmenu.JPG)




![Second_level](https://github.com/goutkannan/Nautilus-LUA/blob/master/img/Menu.JPG)

After selecting the desired configurations exit the menuconfig. In the prompt select `yes` to save the chosen configuration. 

Select any options you require, then run `make` to build the HRT binary image. To make a bootable CD-ROM, 
run `make isoimage`. 



If you see weird errors, chances are there is something wrong with your GRUB2 toolchain 
(namely, `grub-mkrescue`). Make sure `grub-mkrescue` knows where its libraries are, especially if you've 
installed the latest GRUB from source. Use `grub-mkrescue -d`. We've run into issues with naming of
the GRUB2 binaries, in which case a workaround with symlinks was sufficient.


# Running and Debugging under QEMU

Recommended:

`$> qemu-system-x86_64 -cdrom nautilus.iso -m 2048`

Nautilus has multicore support, so this will also work just fine:

`$> qemu-system-x86_64 -cdrom nautilus.iso -m 2048 -smp 4`

## Interaction with the shell 

Lua boots up with  `root-shell>` , Enter the command `lua` to invoke the Lua interpreter.The shell will load with Lua interpreter waiting for input. 

![init screen](https://github.com/goutkannan/Nautilus-Lua/blob/master/img/Lua_init_1.JPG)

In the below sample, we show how to call a math function viz. abs() 

![sample math](https://github.com/goutkannan/Nautilus-Lua/blob/master/img/sample_math.JPG)

From the command `math.abs(param)` we can understand that `abs` is a function in the library `math`. 
We have implemented the nautilus test framework in the similar manner.

So a typical call to nautilus' function_to_test will look like `naut.function_to_test(**args)` 
In order to get the return value back append the command with an '=' sign. 



Inorder to execute a pre-loaded lua script use the command `lua -i scrip_name.lua` 

![sample_script](https://github.com/goutkannan/Nautilus-Lua/blob/master/img/lua_execute.JPG)



## Sample test for Nautilus functions

Here are few sample nautilus functions that can be tested using lua interpreter.
Say, we need to call Nautilus' function `nk_tls_test()` the command to call from lua shell is:

`naut. nk_tls_test()`

As nk_tls_test's return type is void we are not retreiving the output from Lua. Incase of function
`msr_read` which takes in a uin32_t and returns a long unin32_t, We use the '=' operater to retrieve tthe
value returned by the function. 

`=naut.msr_read(12)`

Here is an a example of a Lua script file that contains a mixture of native Lua and call to Nautilus function. This will enable us 
in writing a full fledge script that involves Lua programming as well as testing Nautilus functionalities.
 
<p> The default lua script file is located in the base directory with the name 
<i>lua_script.txt</i>.</p>


![Sample Test](https://github.com/goutkannan/Nautilus-Lua/blob/master/img/lua_script.JPG)


# Resources

You can find publications related to Nautilus and HRTs/HVMs at 
http://halek.co

You can refer to the Lua programming guide to know more about the lua commands and syntax

<https://www.lua.org/pil/contents.html>

For more detailed developer documentation, refer to Reference Manual 
<https://www.lua.org/manual/5.3/> 



# Acknowledgements
This project was done under the guidance and supervision of Prof. Kyle C. Hale.

Kyle C. Hale (c) 2015
Illinois Institute of Technology 

