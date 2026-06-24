## Atari Action! Compiler

### Author: Jakub Husak, 2026

### STATE:
 - Compiler works in 100% as original
 - Dynamic library linking may fail in some cases.

**Atari Action Compiler** in a sandbox made of fake6502 (MOS 6502 Emulator in C) written in C.

It emulates **an original Action! cartridge** sandboxed in bare atari 8-bit environment. Only CPU is emulated, no nmi, but every virtual frame (originally 1/50 sec) the system clock is increased. The frame lasts 313 lines increasing VCOUNT every several instructions (as cycles are not counted). It uses Altirra XL rom as embedded operating system.

The cartridge used is the original with memory zap parts nopped, and some hooks are added (for example recognising idle loop, inserting filenames into filename buffers and poke(764,12) :)) After compile, the error code is gathered and last compiled line printed, as in original cartridge. Additionally, a text representation of the error is printed.

It works in a way that a flag setting to Monitor mode is set, then C"filename" command is invoked, waiting for IDLE loop, then W"filename" is invoked, printed error texts if any and program ends.

The compatibility is not full 100% - as user may use SET instruction, which writes to memory, and there some uses for it. It may not work if used on hardware registers (which are not implemented) or others strange uses. But when you use them as they were made to, you should not encounter any problems.

The EXPERIMENTAL dynamic library linking works in general, there are some minor issues to fix, for example (issue solved) in Printf the %H marker goes to print hex function in another bank. So crawler copies the function to area under Axxx, that is used during compilation time, but it is not used during runtime.

There is no overhead by disk operating system (whole available memory may be used), from the (reasonable) beginning to 0x93ff (well, $9c00-"-pX"\*256, where default for X is 8 and may be changed) 

The Action! cartridge has it's full potential. It should work exactly as original, including all included libraries (beware of case sensitive).

# Usage:

usage:

  ./actionc input.act output.xex \<options\>
  
    options:

    -a	- set atari file format (0x9b = enter), default unix file format

        Normally Action! gets file in atari format (enter=0x9b). But in unix/PC world enter=0xa.
        The input in unix format is translated to Atari format on the fly. However, it may be
        surprising, that you can also compile Atari format not using -b switch. But sometimes
        you have control codes in your sources. Then ctrl-J may be treated as newline, and generate an error.

    -c	- print action library calls during compilation
    -C	- print action library calls during compilation along with code
        
        This is the most missing feature for me, because often I use Action! as pure standalone
        assembly generator (not using the Action! library under A000+). Uppercase option reports
        the right line if call is not last in line and the next line, if call ends the line.

    -w	- write mem.sav (for inspection)

        This writes whole 0-65535 memory image. The addressees between D000-D800 are not valid
        (have no true values) in general. Only vcount and consol (set to 7) is emulated, which is needed for Action!
        to generate bells and keyboard clicks (silent, of course). Without emulated vcount Action! loops on generating clicks :)

    -t  - show compilation time.

    -r  - generate run addr instead of init

    -l  - EXPERIMENTAL - includes dynamically built Action! library (only used functions).

    -m addr val - like SET addr=val in Action!; may be used multiple times

        This simulates SET command. The values are set BEFORE any other activity.


# Alpha state

Compiler is in its alpha state - it means it may have bugs. However, usual use-cases work perfectly.

# Future releases

There are some ideas. If you have ones, fill free to raise an issue :)

# About fake6502 (base)

A simplest Tiny MOS 6502 Emulator in C, so clean. I have looked into many emulators, but this is the simplest and worth seeing how to program cleanly in C.

# About device.c

This file is borrowed from atari800 project. It is cut and simplified to work with only one host drive.
