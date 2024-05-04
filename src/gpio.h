//============================================================================
// Description : FTDI ft2232h supporting GPIO header file for SidBerry
// Author      : LouD
// Last update : 2024
//============================================================================

#pragma once
#ifndef GPIO_H
#define GPIO_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ftdi.hpp>

#include "sidberry.h"
#include "macros.h"

/* Helpers */
extern int calcPin(uint8_t pin, uint8_t port);
/* Helpers */
extern int idPortByPin(uint8_t pin);
/* Helpers */
extern int idPort(uint8_t port);

/* BitBang all pins output */
extern int setupBitBang();

/* BitBang full deinitializer */
extern int breakdownBitBang();

extern int initPort(uint8_t port);
extern int enableBitBang(uint8_t port);
extern int disableBitBang(uint8_t port);
extern int closePort(uint8_t port);
extern void pinMode(uint8_t pin, uint8_t mode);

/* Set a single pin to HIGH or LOW in BitBing mode */
extern void digitalWrite(uint8_t pin, int level);
/* Read a single pin in BitBang mode */
extern int digitalRead(uint8_t pin);

extern struct ftdi_context *ftdi1, *ftdi2;
extern int f_port, gpiop, gpiopin, gpioport, gpiolevel, gpiostate;
extern unsigned char maskport1[1];
extern unsigned char maskport2[1];
extern unsigned char gpiobuff1[1]; // = 0xFF;
extern unsigned char gpiobuff2[1]; // = 0xFF;

// #define INVERTEDLOGIC
#ifndef INVERTEDLOGIC
#define ON 1
#define OFF 0
#else
#define ON 0
#define OFF 1
#endif
#define INPUT 0
#define OUTPUT 1
#define HIGH ON
#define LOW OFF

#define VENDOR 0x0403
#define PRODUCT 0x6010

#define PORTMASK 0xFF
#define NULLMASK 0x0

#define PORT1 INTERFACE_A // AD0 ~ AD7
#define PORT2 INTERFACE_B // BD0 ~ BD7

#define PORTA 10
#define PORTB 20

/* PORT1 */
#define AD0 10
#define AD1 11
#define AD2 12
#define AD3 13
#define AD4 14
#define AD5 15
#define AD6 16
#define AD7 17

/* PORT2 */
#define BD0 20
#define BD1 21
#define BD2 22
#define BD3 23
#define BD4 24
#define BD5 25
#define BD6 26
#define BD7 27

#endif
