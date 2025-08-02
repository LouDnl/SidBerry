//============================================================================
// Description : header file for SidBerry
// Author      : LouD
// Last update : 2024
//============================================================================

#pragma once
#ifndef SIDBERRY_H
#define SIDBERRY_H
#ifndef TERMIWIN_DONOTREDEFINE
using namespace std;
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
// #else
// #include <include/sys/termios.h>
// #include "termios.h"
#else
#include <conio.h>
#include <ctype.h>
#include <WinCon.h>
// #include <wchar.h>
#endif
#include <iostream>
#include <stdio.h>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <bitset>
#include <chrono>
#include <thread>

#include "driver/USBSID.h"


#if defined(DEBUG_SIDBERRY)
#define DBG(...) printf(__VA_ARGS__)
#else
#define DBG(...)
#endif

#if defined(WINDOWS_COMPILE)
/* Source: https://stackoverflow.com/a/26085827 */
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h> // portable: uint64_t   MSVC: __int64

// MSVC defines this in winsock2.h!?
// typedef struct timeval {
//     long tv_sec;
//     long tv_usec;
// } timeval;

int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
    // until 00:00:00 January 1, 1970
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
    return 0;
}
#endif

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

// mos6502 memory
extern uint8_t memory[65536];

enum clock_speeds
{
  UNKNOWN = CLOCK_DEFAULT,
  PAL     = CLOCK_PAL,
  NTSC    = CLOCK_NTSC,
  BOTH    = CLOCK_NTSC,
  DREAN   = CLOCK_DREAN,
};

enum refresh_rates
{
  DEFAULT = HERTZ_DEFAULT,
  EU      = HERTZ_50,
  US      = HERTZ_60,
  GLOBAL  = HERTZ_60
};

enum scan_lines
{
  C64_PAL_SCANLINES = 312,
  C64_NTSC_SCANLINES = 263
};

enum scanline_cycles
{
  C64_PAL_SCANLINE_CYCLES = 63,
  C64_NTSC_SCANLINE_CYCLES = 65
};

enum CIA1_registers
{ /* https://www.c64-wiki.com/wiki/CIA */
  CIA_TIMER_LO = 0xDC04,
  CIA_TIMER_HI = 0xDC05,
};

static const enum clock_speeds clockSpeed[] = {UNKNOWN, PAL, NTSC, BOTH, DREAN};
static const enum refresh_rates refreshRate[] = {DEFAULT, EU, US, GLOBAL};
static const enum scan_lines scanLines[] = {C64_PAL_SCANLINES, C64_PAL_SCANLINES, C64_NTSC_SCANLINES, C64_NTSC_SCANLINES};
static const enum scanline_cycles scanlinesCycles[] = {C64_PAL_SCANLINE_CYCLES, C64_PAL_SCANLINE_CYCLES, C64_NTSC_SCANLINE_CYCLES, C64_NTSC_SCANLINE_CYCLES};
const char *sidtype[5] = {"Unknown", "N/A", "MOS8580", "MOS6581", "FMopl" };  /* 0 = unknown, 1 = N/A, 2 = MOS8085, 3 = MOS6581, 4 = FMopl */
const char *chiptype[4] = {"Unknown", "MOS6581", "MOS8580", "MOS6581 and MOS8580"};
const char *clockspeed[5] = {"Unknown", "PAL", "NTSC", "PAL and NTSC", "DREAN"};

/* USBSID Specific */
USBSID_NS::USBSID_Class* us_sid;

/* function to track ctrl+c
   sigint excerpt from https://stackoverflow.com/questions/26965508/infinite-while-loop-and-control-c#26965628 */
void inthand(int signum);

/* Handler for a clean exit */
void exitPlayer(void);

/* Main address writing function */
void MemWrite(uint16_t addr, uint8_t byte);
/* Main address reading function */
uint8_t MemRead(uint16_t addr);

/* Load SID file into memory */
int load_sid(mos6502 cpu, SidFile sid, int song_number);
/* Get key pressed without echo */
int getch_noecho_special_char(void);
/* Player state handler */
void change_player_status(mos6502 cpu, SidFile sid, int key_press, bool *paused, bool *exit, uint8_t *mode_vol_reg, int *song_number, int *sec, int *min);

/* Player setup */
void USBSIDSetup(void);

#endif // SIDBERRY_H
