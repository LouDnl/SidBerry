## SidBerry ##
### Music player for MOS SID chip (6581/8580), [SIDKick-pico](https://github.com/frntc/SIDKick-pico) or SwinSID like replacements using USB or GPIO enabled devices ###
Such devices include [USBSID-Pico](https://github.com/LouDnl/USBSID-Pico), [FTDI ft2232h board](https://github.com/arm8686/FT2232HL-Board), RaspberryPi, AriettaG25 and more if you want you create the interface code.
#### Original Author and repo of SiBerry ####
[@gianlucag](https://github.com/gianlucag) ~ [SidBerry](https://github.com/gianlucag/SidBerry)


## USBSID-Pico & FTDIBerry ##
This repo contains an adaptation of SidBerry that includes playing SID files over USB on a Linux PC (Windows if you compile on Windows - no support).

### Dependencies for building
**USBSID-Pico** requires libusb \
**FTDIBerry** requires libftdi and libftdi1

## Building SidBerry ##
Examples:
```shell
# Default make will compile for USBSID-Pico with debug symbols enabled
make
# Make default binary with additional debug logging
make debug

# Make binaries for usbsidpico, ftdi, raspberrypi & arietta25g
make all

# Make binaries for usbsidpico & ftdi
make usb

# For the USBSID-Pico with libusb installed, compile with:
make usbsidpico
# For the FTDI USB board with "libftdi(-dev) and libftdi1(-dev)" installed, compile with:
make ftdi
# For the AriettaG25 board with "wiringSAM" installed, compile with:
make arietta25g
# For the RaspberryPI board with "wiringPi" installed, compile with:
make raspberrypi
# For other cards, after having appropriately modified "gpioInterface.cpp" and "gpioInterface.h", compile with:
make custom
```


# Translation of the original [README](README-original.md) by [@gianlucag](https://github.com/gianlucag/SidBerry)

## SidBerry ##
### Music player for SID 6581 chip made with RaspberryPi ###

A jukebox SID made with RasberryPi and the original 6581 SID chip. The chip is housed in a custom board connected directly to the Rasberry GPIOs!

[![ScreenShot](http://img.youtube.com/vi/i_vNFhmKoK4/0.jpg)](http://youtu.be/i_vNFhmKoK4)

The board can play any SID file, just load the .sid files into the Rasberry sdcard, start the player and connect a pair of pre-amplified speakers or better yet a stereo to the jack output.

### Chip SID ###

The original SID, in variants 6581 and 8580, is controlled by loading its 29 internal registers with the appropriate values at the appropriate time. The sequence of bytes sent generates the desired effect or music. Each register is 1 byte and there are 29 registers in total so you need at least 5 gpio of address (2^5=32) and 8 gpio of data for a total of 13 gpio. Another gpio is required for the chip's CS line (Chip Select).

![Alt text](/img/sid.png?raw=true "SID chip")

Fortunately, the Raspberry has 17 GPIOs and the 14 required are perfectly controllable with the WiringPi library. The other pins of the chip are used for the two internal filter capacitors (CAP_a and CAP_b), power supply, R/W line (which we will connect directly to GND, we are always writing), clock signal input, reset line ( always connected to VCC) and analogue audio output (AUDIO OUT).

Here is the configuration of the SID internal registers:

![Alt text](/img/registers.png?raw=true "registers")

Here for a more detailed description of the internal workings of the chip:

http://www.waitingforfriday.com/index.php/Commodore_SID_6581_Datasheet

### Hardware ###

The board exactly reproduces the "boundary conditions" for the SID chip as if it were housed in an original Commodore 64. The original application note clearly shows the connections to be made and the few external components required (clock generator, capacitors and little else).

![Alt text](/img/orig.png?raw=true "orig")

The only difference is that the address and data lines are routed directly to the RasberryPi GPIOs.

IMPORTANT NOTE!: The RasberryPi reasons in CMOS logic at 3.3V while the SID chip is TTL at 5V therefore completely incompatible in terms of voltages. Fortunately, since we are only writing to registers, the RasberryPi will only have to apply 3.3v to the ends of the chip, more than enough to be interpreted as a high logic level by the SID.

![Alt text](/img/board.jpg?raw=true "board")

The complete schematic:

![Alt text](/img/sch.png?raw=true "SID chip")

### Software ###

The bulk of the work. I wanted a completely stand-alone solution, without the use of external players (like ACID64) and therefore I created a player that emulates a large part of an original C64. The emulator is necessary because .sid files are 6502 machine language programs and must be executed as such. The player is written in C/C++ and based on my MOS CPU 6502 emulator plus a simple array of 65535 bytes as RAM (the original C64 in fact has 64K of RAM). The player loads the program code contained in the .sid file into virtual RAM plus an additional assembly code that I called micro-player: essentially it is a minimal program written in machine language for the 6502 CPU that performs two specific tasks:

  * install the reset and interrupt vectors at the correct locations
  * call the play routine of the sid code on every interrupt

This is because the code that generates music in an ordinary C64 is a function called at regular intervals (50 times per second, 50Hz). The call is made by means of an external interrupt. This way it is possible to have music and any other program (video game for example) running at the same time.

Other components are the parser for .sid files and the WiringPi library to drive the RaspberryPi GPIOs.

This is the layout of the entire application, on the right is the 64K virtual RAM memory

![Alt text](/img/diagram.png?raw=true "layout")

This is the micro-player assembler code:

```
// setup reset vector (0x0000)
memory[0xFFFD] = 0x00;
memory[0xFFFC] = 0x00;</p>
// setup interrupt vector, point to play routine (0x0013)
memory[0xFFFF] = 0x00;
memory[0xFFFE] = 0x13;

// micro-player code
memory[0x0000] = 0xA9;
memory[0x0001] = atoi(argv[2]); // register A is loaded with the track number (0-16)

memory[0x0002] = 0x20; // jump to the init-routine in the sid code
memory[0x0003] = init &amp; 0xFF; // I will add it
memory[0x0004] = (init &gt;&gt; 8) &amp; 0xFF; // hi addr

memory[0x0005] = 0x58; // enable interrupts
memory[0x0006] = 0xEA; // nop
memory[0x0007] = 0x4C; // infinite loops
memory[0x0008] = 0x06;
memory[0x0009] = 0x00;

// Interrupt service routine (ISR)
memory[0x0013] = 0xEA; // nop
memory[0x0014] = 0xEA; // nop
memory[0x0015] = 0xEA; // nop
memory[0x0016] = 0x20; // let's jump to the play routine
memory[0x0017] = play &amp; 0xFF;
memory[0x0018] = (play &gt;&gt; 8) &amp; 0xFF;
memory[0x0019] = 0xEA; // nop
memory[0x001A] = 0x40; // return from interrupt
```
### Software changes introduced by Alessio Lombardo ###
- Bug-Fixing (thanks to Thoroide, https://www.gianlucaghettini.net/sidberry2-raspberry-pi-6581-sid-player/)
- Possibility to compile the source for different boards/SoCs. In addition to RaspberryPI, the Acme Systems AriettaG25 board is directly supported. Other cards can be added by appropriately editing the "gpioInterface.cpp" and "gpioInterface.h" files. If necessary, you can refer to external libraries for managing GPIO ports (for example, "wiringPi" in the case of Rapsberry or "wiringSam" in the case of Arietta).
- Added controls to the player: Pause/Continue, previous/next track, Restart, Exit (with register reset), Verbose Mode.

For the AriettaG25 board with the "wiringSAM" library already installed, compile with:
```
g++ *.cpp -D BOARD='ARIETTAG25' -o sidberry -lwiringSam
```
For the RaspberryPI board with the "wiringPi" library already installed, compile with:
```
g++ *.cpp -D BOARD='RASPBERRYPI' -o sidberry -lwiringPi
```
For other cards, after having appropriately modified "gpioInterface.cpp" and "gpioInterface.h", compile with:
```
g++ *.cpp -D BOARD='CUSTOM' -o sidberry
```
