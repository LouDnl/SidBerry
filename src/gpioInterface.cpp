//============================================================================
// Description : SidBerry wrapper to GPIO ports
// Authors     : Alessio Lombardo, LouD
// Last update : 2024
//============================================================================

#include "gpioInterface.h"
// uint8_t memory[65536];         // init 64K ram
// #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#if BOARD == RASPBERRYPI // Raspberry Pi (GPIO library: <wiringPi.h>)

int gpioSetup()
{
    wiringPiSetup();

    return 0;
}

int gpioStop()
{
    return 0;
}

void gpioMode(int pin, int mode)
{
    pinMode(pin, mode);
}

int gpioRead(int pin)
{
    return 0;
}

void gpioWrite(int pin, int level)
{
    digitalWrite(pin, level);
}

#elif BOARD == ARIETTAG25 // Acme Systems Arietta G25 (GPIO library: <wiringSam.h>)

int gpioSetup()
{
    wiringSamSetup();
    return 0;
}

int gpioStop()
{
    return 0;
}

void gpioMode(int pin, int mode)
{
    FILE *fp = fopen("/sys/class/gpio/export", "w");
    fprintf(fp, "%d", pin);
    fclose(fp);
    if (pin >= 0 && pin < 32)
        pinMode(WSAM_PIO_A, pin, mode);
    else if (pin >= 32 && pin < 64)
        pinMode(WSAM_PIO_B, pin - 32, mode);
    else if (pin >= 64 && pin < 96)
        pinMode(WSAM_PIO_C, pin - 64, mode);
}

int gpioRead(int pin)
{
    return 0;
}

void gpioWrite(int pin, int level)
{
    if (pin >= 0 && pin < 32)
        digitalWrite(WSAM_PIO_A, pin, level);
    else if (pin >= 32 && pin < 64)
        digitalWrite(WSAM_PIO_B, pin - 32, level);
    else if (pin >= 64 && pin < 96)
        digitalWrite(WSAM_PIO_C, pin - 64, level);
}

#elif BOARD == FTDI // FTDI ft2232h ~ requires libftdi libftdi-dev libftdi1 libftdi1-dev

int gpioInitPorts() // FTDI Only
{
    initPort(PORT1);
    initPort(PORT2);

    return 0;
}

int gpioClosePorts() // FTDI Only
{
    closePort(PORT1);
    closePort(PORT2);

    return 0;
}

int gpioEnableBB() // FTDI Only
{
    enableBitBang(PORT1);
    enableBitBang(PORT2);

    return 0;
}

int gpioDisableBB() // FTDI Only
{
    disableBitBang(PORT1);
    disableBitBang(PORT2);

    return 0;
}

int gpioSetup()
{
    gpioInitPorts();
    gpioEnableBB();

    return 0;
}

int gpioStop()
{
    gpioDisableBB();
    gpioClosePorts();

    return 0;
}

int gpioRead(int pin)
{
    return digitalRead(pin);
}

void gpioWrite(int pin, int level)
{
    digitalWrite(pin, level);
}

void gpioMode(int pin, int mode)
{
    pinMode(pin, mode);
}

#elif BOARD == USBSIDPICO


int gpioSetup()
{
    usbSIDSetup();
    return 0;
}

int gpioStop()
{
    usbSIDExit();
    return 0;
}

void gpioMode(int pin, int mode)
{
    (void)pin;
    (void)mode;
}

int gpioRead(int pin)
{
    (void)pin;
    return 0;
}

void gpioWrite(int pin, int level)
{
    (void)pin;
    (void)level;
}

#elif BOARD == CUSTOM // Custom Board

int gpioSetup()
{
    return 0;
}

int gpioStop()
{
    return 0;
}

void gpioMode(int pin, int mode)
{
}

int gpioRead(int pin)
{
    return 0;
}

void gpioWrite(int pin, int level)
{
}

#endif // BOARD
