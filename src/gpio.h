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

#include "macros.h"

/* Helpers */

/* Calculate actual pin by provided port */
extern int calcPin(uint8_t pin, uint8_t port);
/* ID the correct interface port by provided pin */
extern int idPortByPin(uint8_t pin);
/* ID the correct interface port by provided port */
extern int idPort(uint8_t port);

/* GPIO Functions */

/* Enable a single interface port */
extern int initPort(uint8_t port);
/* Close a single interface port */
extern int closePort(uint8_t port);
/* Enable BitBang on a single interface port with all pins to output */
extern int enableBitBang(uint8_t port);
/* Disable BitBang on a single interface port */
extern int disableBitBang(uint8_t port);
/* Change the pinMode of a single pin to input or output in BitBang mode on a single interface port */
extern void pinMode(uint8_t pin, uint8_t mode);
/* Change the pinMode with a pinmask for all pins to input or output in BitBang mode on a single interface port */
extern void pinModePort(uint8_t port, unsigned char mask);
/* Change the pin state of a single pin that is set to output in BitBang mode */
extern void digitalWrite(uint8_t pin, int level);
/* Read the pin state of a single pin that is set to input in BitBang mode */
extern int digitalRead(uint8_t pin);
/* Read the pin state of a single interface port that is set to input in BitBang mode */
extern int digitalReadBus(uint8_t port);

extern struct ftdi_context *ftdi1, *ftdi2;
extern int f_port, gpiopin, gpioport;
extern unsigned char maskport1[1];
extern unsigned char maskport2[1];
extern unsigned char gpiobuff1[1];
extern unsigned char gpiobuff2[1];
extern unsigned char readbuff[1];

#define INPUT 0
#define OUTPUT 1
#define HIGH 1
#define LOW 0

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
