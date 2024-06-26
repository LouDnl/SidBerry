//============================================================================
// Description : header file for SidBerry
// Author      : LouD
// Last update : 2024
//============================================================================

#pragma once
#ifndef SIDBERRY_H
#define SIDBERRY_H
#include <iostream>
using namespace std;
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <cstdint>
#include <iomanip>
#include <bitset>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#include "gpioInterface.h"
#include "macros.h"

// Clock cycles for the MOS6502
#define CLOCK_CYCLES 100000

// Clock speed: 0.985 MHz (PAL) or 1.023 MHz (NTSC)
#define CLOCK_DEFAULT 1000000  // 1MHz = 1us
#define CLOCK_PAL      985248  // 0.985 MHz
#define CLOCK_NTSC    1022727  // 1.023 MHz
#define CLOCK_DREAN   1023440  // 1.023 MHz

// Refreshrates in microseconds
#define HERTZ_DEFAULT 20000  // 50Hz ~ 20000 == 20us
#define HERTZ_50      19950  // 50Hz ~ 20000 == 20us / 50.125Hz ~ 19.950124688279us exact
#define HERTZ_60      16715  // 60Hz ~ 16667 == 16.67us / 59.826Hz ~ 16.715140574332 exact

// Default addresses
#define VOL_ADDR 0xD418

enum clock_speeds
{
    UNKNOWN = CLOCK_DEFAULT,
    PAL     = CLOCK_PAL,
    NTSC    = CLOCK_NTSC,
    BOTH    = CLOCK_NTSC,
    DREAN   = CLOCK_DREAN,

};
static const enum clock_speeds clockSpeed[] = {UNKNOWN, PAL, NTSC, BOTH, DREAN};

enum refresh_rates
{
    DEFAULT = HERTZ_DEFAULT,
    EU      = HERTZ_50,
    US      = HERTZ_60,
    GLOBAL  = HERTZ_60
};
static const enum refresh_rates refreshRate[] = {DEFAULT, EU, US, GLOBAL};

/*
    printf("enum index %i\n", (clock_speed)cs); // index
    printf("enum value %i\n", clockSpeed[cs]);	// value
*/
extern timeval t1, t2;
extern long int elaps;
extern int sidcount;
extern int custom_clock;
extern int custom_hertz;
extern int volume;
extern int clock_speed;
extern int refresh_rate;
extern uint8_t memory[65536];
// extern uint16_t phyaddr;
extern bool verbose;
extern bool trace;
extern bool calculatedclock;
extern bool calculatedrefresh;
extern bool real_read;
extern volatile sig_atomic_t stop;

void inthand(int signum);

#endif // SIDBERRY_H
