//============================================================================
// Description : header file for SidBerry
// Author      : LouD
// Year        : 2024
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

#if defined(DEBUG_SIDBERRY)
#define DBG(...) printf(__VA_ARGS__)
#else
#define DBG(...)
#endif

// Clock cycles for the MOS6502
#define CLOCK_CYCLES 100000

// Clock speed: 0.985 MHz (PAL) or 1.023 MHz (NTSC)
#define DEFAULT_CLOCK 1000000 // 1MHz = 1us
#define PAL_CLOCK 985000      // 0.985 MHz
#define NTSC_CLOCK 1023000    // 1.023 MHz

// Refreshrates in microseconds
#define HERTZ_50 20000 // 50Hz ~ 20us
#define HERTZ_60 16667 // 60Hz ~ 16.67us

enum clock_speeds
{
    UNKNOWN = PAL_CLOCK,
    PAL = PAL_CLOCK,
    NTSC = NTSC_CLOCK,
    BOTH = DEFAULT_CLOCK

};
static const enum clock_speeds clockSpeed[] = {UNKNOWN, PAL, NTSC, BOTH};

enum refresh_rates
{
    DEFAULT = HERTZ_50,
    EU = HERTZ_50,
    US = HERTZ_60,
    GLOBAL = HERTZ_60
};
static const enum refresh_rates refreshRate[] = {DEFAULT, EU, US, GLOBAL};

/*
    printf("enum index %i\n", (clock_speed)cs); // index
    printf("enum value %i\n", clockSpeed[cs]);	// value
*/
extern int clock_speed;
extern int refresh_rate;
extern uint8_t memory[65536];
// extern uint16_t phyaddr;
extern bool verbose;
extern bool trace;
extern bool exclock;
extern bool exrefresh;
extern volatile sig_atomic_t stop;

void inthand(int signum);

#endif // SIDBERRY_H
