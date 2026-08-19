# Engine-OS

Engine-OS is supposed to be an operating system that allows you to create games *inside* the operating system and distribute them as standalone bootable `.EFI` files. 

You write a game in the built in editor and compile it against the runtime kernel that does all the heavy lifting for you

## Working things

- Boots on UEFI
- Own memory management
- NVMe driver
- Some type of small shell with commands, Type `help` for the available command list
- "neenor" is the built in text editor that lets you create files into the EnFS filesystem

- **EnScript** is our own scripting language, Reference to this can be found in [ENSCRIPT.md](ENSCRIPT.md). This language is a bit Assembly flavoured but meant to change to more C like language later. For reference code you can see [pong](Games/pong.en) or [snake](Games/snake.en). (Realized that .es files show as typescript so changed to .en)

- **AOT compiler** works halfly

## Making a game

`neenor snake.en` write it in the editor

`play snake.en` to test it

`export snake.en snake.efi` export and link it to the runtime kernel

Then on the linux side you can pull it out of the disk.img with `./getgame.sh snake.efi`

or run it straight with `make rungame GAME=snake.efi`

These both place the bootable `.EFI` into
esp/export/EFI/BOOT/BOOTX64.EFI


you can also build the game into opcode inside the kernel

`neenor test.en` write something

`make test.en test` compile it into opcode

`test` to run the code

Right now exporting still works through the interpreter instead of the opcode

## Building

You need `x86_64-w64-mingw32-gcc`, `qemu-system-x86_64` and OVMF firmware (edk2).

The Makefile excpects OVMF at `/usr/share/edk2/x64/`

`make` build the OS (BOOTX64.EFI)

`make runtime` build the game runtime kernel (RUNTIME.EFI)

`make run` boot the OS in QEMU with an NVMe disk attached

`make disk` create a fresh empty 1G disk image

First boot on a fresh disk, run these in the shell

`mkpart 64 engine --yes`

`mount`

`format --yes`

## Roadmap

- **AOT compiler** 
- **C-Like Sytnax**
- **FAT32 support**


## -The general who builds his own roads is never lost on them. -Sun Tzu, The Art of Engine OS


## My yappings (Not anything important under this)

This project is meant to be left as open source. You can take this project and see the source code for yourself if you want to (The code might not be the best you have ever seen). Also the main reason im making this is because i have loved game development since the day i started coding and i though why not create a game engine. Then i started to create a game engine in Vulkan. After a year or so messing around with Vulkan i started to get bored in the vulkan language and how i didnt have control over some things. Then i started thinking about dabbling into OS dev and got this goddamn bright idea of "What if i made an Operating System that is Game Engine". This way i would have the most control over everything. The Drivers and shi. But being me i don't know why i always try to make my life so much harder and use basically no external dependencies and libraries. Also if someone read all of this yapping `Fuck you`. Im just kidding i hope you find happiness in your life and understand the meaning of life.