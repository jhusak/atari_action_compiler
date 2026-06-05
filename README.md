# Atari Action! Compiler

Atari Action! Compiler in a sandbox made of fake6502 (MOS 6502 Emulator in C) written in C.

It emulates original sandboxed Action! cartridge in bare atari 8-bit environment. Only CPU is emulated, no nmi, but every virtual frame (originally 1/50 sec) the system clock is increased. The frame lasts about 30000 instructions (as cycles are not counted). It uses XL rom (sorry for that, maybe altirra rom will be better).

The cartridge is not modified in any way - some hooks are added (for example recognising idle loop, inserting filenames into filename buffers and poke(764,12) :)) After compile, the error code is gathered and last compiled line printed, as in original cartridge.

It works in a way that a flag setting to Monitor mode is set, then C"filename" command is invoked, waiting for IDLE loop, then W"filename" is invoked, printed error texts if any and program ends.

The comatibility is not full 100% - as user may use SET instruction, which writes to memory, and there some uses for it. It may not work if used on hardware registers (which are not implemented) or others strange uses. But when you use them as they were made to, you should not encounter any problems.

There is no overhead by disk operating system (whole available memory may be used), from the (reasonable) beginning to 0x93ff.

The Action! cartridge has it's full potential. It should work exactly as original, including all included libraries (beware of case sensitive).

# Alpha state

Compiler is in it's alpha state - it means it may have bugs. However, usual usecases work perfectly.

# Future releases

There are some ideas. If you have ones, fill free to raise an issue :)

# About fake6502 (base)

A simplest Tiny MOS 6502 Emulator in C, so clean. I have looked into many emulators, but this is the simplest and worth seeing how to program cleanly in C.

# About device.c

This file is borrowed from atari800 project. It is cut and simplified to work with only one host drive.
