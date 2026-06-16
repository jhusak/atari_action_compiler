# Tests

Run the tests by hand. Please install atari800 emulator somewhere in the $PATH

For convinience every time test is run, actionc is compiled and used locally.

Tests are here to check not the cartridge itself (as it is a s it is), but the sandbox.

The memory contents, especially in the beginning of the memory are very important during runtime, for example jump table located at 4c6-4ff. For now they are simply a part of generated xex file.
