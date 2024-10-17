//============================================================================
// Description : SidBerry wrapper to GPIO ports - Header File
// Authors     : Gianluca Ghettini, Alessio Lombardo, LouD
// Last update : 2024
//============================================================================

#pragma once
#ifndef GPIOINTERFACE_H
#define GPIOINTERFACE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <cstdint>

/* #include "macros.h" */

#define USBSIDPICO 0
#define FTDI 1
#define RASPBERRYPI 2
#define ARIETTAG25 3
#define CUSTOM 4

#if BOARD == RASPBERRYPI // Raspberry Pi
extern "C"
{
#include <wiringPi.h>
}

/* WiringPi pin numbers */
// #define CLK            21
#define RES 25 // BCM26
#define RW 24  // BCM19
#define CS 3   // BCM22

#define A0 8 // BCM02
#define A1 9 // BCM03
#define A2 7 // BCM04
#define A3 0 // BCM17
#define A4 2 // BCM27

#define D7 1  // BCM18
#define D6 4  // BCM23
#define D5 5  // BCM24
#define D4 6  // BCM25
#define D3 26 // BCM12
#define D2 27 // BCM16
#define D1 28 // BCM20
#define D0 29 // BCM21

#elif BOARD == ARIETTAG25 // Acme Systems Arietta G25

#include <wiringSam.h>

#define CS 65
#define A0 6
#define A1 92
#define A2 68
#define A3 67
#define A4 66
#define D0 8
#define D1 0
#define D2 29
#define D3 28
#define D4 27
#define D5 26
#define D6 25
#define D7 24

#elif BOARD == FTDI // FTDI ft2232h

#include "gpio.h"

#define RES BD0
#define RW BD1
#define CS BD2

#define A0 BD3
#define A1 BD4
#define A2 BD5
#define A3 BD6
#define A4 BD7

#define D0 AD0
#define D1 AD1
#define D2 AD2
#define D3 AD3
#define D4 AD4
#define D5 AD5
#define D6 AD6
#define D7 AD7

int gpioInitPorts();  // FTDI Only
int gpioClosePorts(); // FTDI Only
int gpioEnableBB();   // FTDI Only
int gpioDisableBB();  // FTDI Only

#elif BOARD == USBSIDPICO

#include "usbsid/usbsid_driver.h"

// REQUIRED ~ init all to 0
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0

#define RES 0
#define RW 0
#define CS 0

#define A0 0
#define A1 0
#define A2 0
#define A3 0
#define A4 0

#define D0 0
#define D1 0
#define D2 0
#define D3 0
#define D4 0
#define D5 0
#define D6 0
#define D7 0

#elif BOARD == CUSTOM // Custom Board
// 1) GPIO library (if needed)
// 2) I/O Defines (if needed): INPUT mode, OUTPUT mode, HIGH level, LOW level
// 3) Pinout Defines (mandatory): CS (Chip Select), A0-A4 (Address Bus), D0-D7 (Data Bus)

// REQUIRED
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0

// #define RES 0
// #define RW 0
#define CS 0

#define A0 0
#define A1 0
#define A2 0
#define A3 0
#define A4 0

#define D0 0
#define D1 0
#define D2 0
#define D3 0
#define D4 0
#define D5 0
#define D6 0
#define D7 0

#endif // BOARD

/*
 * Function to SETUP the GPIO ports. Map this function to the equivalent GPIO library function or write it.
 */
int gpioSetup();
/*
 * Function to break down the GPIO ports.
 */
int gpioStop();
/*
 * Function to set the MODE of a GPIO port. Map this function to the equivalent GPIO library function or write it.
 */
void gpioMode(int pin, int mode);
/*
 * Function to READ a pin state from a GPIO port.
 */
int gpioRead(int pin);
/*
 * Function to WRITE to a GPIO port. Map this function to the equivalent GPIO library function or write it.
 */
void gpioWrite(int pin, int level);

#endif // GPIOINTERFACE_H
