# Tense

> Note that this version of tense is very unreliable. It will almost definitely lock up for non-trivial programs.

Tense aims to be a framework for virtual-time execution in the Linux kernel.

The project consists of a loadable kernel module, a C library, and samples. The published implementation does not account for time dilation on multicore archiectures. I have a prototype for that which follows the same structure, but is still too unstable. In the meantime I recommend running programs with `taskset -c` to restrict CPU allocation.

## Instructions

If I refer to it, `$WORK` is the parent directory of this repository.

1. Run a modified kernel

The point of this step is to modify your operating system so that it supports running Tense. You will do this by applying a patch to the Linux kernel sources, choosing a kernel configuration, building with it, and running the newly built kernel.

  - Get the Linux kernel sources (git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git)
  - Go to the v4.16 tag (latest should be fine, but this is what I develop against)
  - Apply the patch located in `kernels/linux/tense.patch`
  - Build the patched kernel - see `ckb` in My aliases, my kernel configurations are in `kernels/linux/configs`, but feel free to use your own or copy the one from your distro.
  - Install the kernel - see `cki(m)` in My aliases, just a regular kernel installation.

When you boot into the new kernel there should be no noticeable difference apart from a *small* performance overhead which can be measured by how fast you run out of battery on a laptop for example.

2. Prepare tense for use

The actual implementation of tense is in a loadable kernel module and not in the patched kernel itself. You need to build this module and insert it whenever you want to use the framework.

  - Use the Makefile in `kernels/linux` or see `tki` and `tku` in My aliases.
  - Check that the module is inserted with `lsmod` (if your computer hasn't crashed it probably suceeded).
  - There should now be a file in `/sys/kernel/debug` named `tense`. The kernel module handles operations on that file.

3. Use tense through the C library

The C library (libtense) is in a CLion project and uses CMake because I got tired of writing Makefiles... You can build it either way, it will also build a bunch of test programs in the default build.

If you choose to link the library against your program, then you should just be able to run the program in virtual time. Most of the example programs do that.

Otherwise, look at `preload.c` which uses dynamic linking to enable the library. It will check if the `TENSE` environment variable is set (see `tense` in My aliases). The path to `libtense.so` also needs to be in `LD_LIBRARY_PATH`, in my case this is `$WORK/tense/libtense/cmake-build-debug`.

4. Run an example

Look at one of the example programs in `libtense/test/tense_lock_race.c`. It can be built with:

```
cmake --build $WORK/tense/libtense/cmake-build-debug --target tense_lock_race -- -j 4
```

I use the `cmake` which comes packaged with CLion.

Then run it with (see My aliases for `tense`):

```
tense $WORK/tense/libtense/cmake-build-debug/tense_lock_race 200
```

This will produce a `tense.log` file with the contents of stderr. You can play with adjusting the time dilation percent, moving it around other sections of code, or adding other timing points.

## My aliases

```

# cd tense kernel module
alias ctk="cd $WORK/git/tense/kernels/linux"

# tense kernel module install / update
alias tki="make && sudo insmod tense.ko && sudo chmod a+rx /sys/kernel/debug"
alias tku="make && sudo rmmod -fv tense && sudo insmod tense.ko"

# cd tense lib
alias ctl="cd $WORK/git/tense/libtense/cmake-build-debug"

# cd kernel sources
alias ck="cd $WORK/git/kernels/linux"

# cd kernel sources and build to my ubuntu config
alias ckb="ck && make O=$WORK/obj/linux-x86-ubuntuconfig -j6"

# cd kernel build and install / install + modules
alias cki="cd $WORK/obj/linux-x86-ubuntuconfig && sudo make install"
alias ckim="cd $WORK/obj/linux-x86-ubuntuconfig && sudo make modules_install install"

# tense environment on a single core
alias tense="env TENSE=y taskset -c 4 ${@} 2> tense.log"

```
