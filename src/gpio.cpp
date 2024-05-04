//============================================================================
// Description : FTDI ft2232h supporting GPIO source file for SidBerry
// Author      : LouD
// Last update : 2024
//============================================================================

#include "gpio.h"

struct ftdi_context *ftdi1, *ftdi2;
int f_port, gpiop, gpiopin, gpioport, gpiolevel, gpiostate;
unsigned char maskport1[1];
unsigned char maskport2[1];
unsigned char gpiobuff1[1];
unsigned char gpiobuff2[1];

/* Helpers */
int calcPin(uint8_t pin, uint8_t port)
{
    return port == PORT1 ? pin - PORTA : port == PORT2 ? pin - PORTB
                                                       : 0;
}

int idPortByPin(uint8_t pin)
{
    return (pin >= PORTA) && (pin < PORTB) ? PORT1 : (pin >= PORTB) && (pin < 30) ? PORT2
                                                                                  : 0;
}

int idPort(uint8_t port)
{
    return (port == 1) || (port == 10) ? PORT1 : (port == 2) || (port == 20) ? PORT2
                                                                             : 0;
}

int setupBitBang()
{
    DBG("\n");

    // Init 1. channel
    if ((ftdi1 = ftdi_new()) == 0)
    {
        fprintf(stderr, "ftdi_new failed\n");
        return EXIT_FAILURE;
    }

    ftdi_set_interface(ftdi1, INTERFACE_A);
    f_port = ftdi_usb_open(ftdi1, VENDOR, PRODUCT);
    if (f_port < 0 && f_port != -5)
    {
        fprintf(stderr, "unable to open ftdi device: %d (%s)\n", f_port, ftdi_get_error_string(ftdi1));
        ftdi_free(ftdi1);
        exit(-1);
    }
    DBG("ftdi open succeeded(channel 1): %d\n", f_port);

    DBG("enabling bitbang mode(channel 1)\n");
    ftdi_set_bitmode(ftdi1, 0xFF, BITMODE_BITBANG);

    // Init 2. channel
    if ((ftdi2 = ftdi_new()) == 0)
    {
        fprintf(stderr, "ftdi_new failed\n");
        return EXIT_FAILURE;
    }
    ftdi_set_interface(ftdi2, INTERFACE_B);
    f_port = ftdi_usb_open(ftdi2, VENDOR, PRODUCT);
    if (f_port < 0 && f_port != -5)
    {
        fprintf(stderr, "unable to open ftdi device: %d (%s)\n", f_port, ftdi_get_error_string(ftdi2));
        ftdi_free(ftdi2);
        exit(-1);
    }
    DBG("ftdi open succeeded(channel 2): %d\n", f_port);

    DBG("enabling bitbang mode (channel 2)\n");
    ftdi_set_bitmode(ftdi2, 0xFF, BITMODE_BITBANG);

    // buf[0]* = BITMASK;

    return 0;
}

int breakdownBitBang()
{
    // struct ftdi_context *ftdi1, *ftdi2;
    DBG("disabling bitbang mode(channel 1)\n");
    ftdi_disable_bitbang(ftdi1);
    ftdi_usb_close(ftdi1);
    ftdi_free(ftdi1);

    DBG("disabling bitbang mode(channel 2)\n");

    ftdi_disable_bitbang(ftdi2);
    ftdi_usb_close(ftdi2);
    ftdi_free(ftdi2);
    return 0;
}

int initPort(uint8_t port)
{

    gpioport = idPort(port);

    DBG("\n");

    if (gpioport == 1)
    { // Init channel
        if ((ftdi1 = ftdi_new()) == 0)
        {
            fprintf(stderr, "ftdi_new failed\n");
            return EXIT_FAILURE;
        }

        ftdi_set_interface(ftdi1, PORT1);
        f_port = ftdi_usb_open(ftdi1, VENDOR, PRODUCT);
        if (f_port < 0 && f_port != -5)
        {
            fprintf(stderr, "unable to open ftdi device: %d (%s)\n", f_port, ftdi_get_error_string(ftdi1));
            ftdi_free(ftdi1);
            exit(-1);
        }
    }
    else if (gpioport == 2)
    { // Init channel
        if ((ftdi2 = ftdi_new()) == 0)
        {
            fprintf(stderr, "ftdi_new failed\n");
            return EXIT_FAILURE;
        }

        ftdi_set_interface(ftdi2, PORT2);
        f_port = ftdi_usb_open(ftdi2, VENDOR, PRODUCT);
        if (f_port < 0 && f_port != -5)
        {
            fprintf(stderr, "unable to open ftdi device: %d (%s)\n", f_port, ftdi_get_error_string(ftdi2));
            ftdi_free(ftdi2);
            exit(-1);
        }
    }
    else
    {
        fprintf(stderr, "%d is not a valid port!\n", gpioport);
        return 1;
    }
    DBG("ftdi open succeeded(channel %i): %d\n", gpioport, f_port);
    return 0;
}

int enableBitBang(uint8_t port)
{
    gpioport = idPort(port);

    DBG("enabling bitbang all pins output mode(channel %i)\n", gpioport);

    if (gpioport == 1)
    {
        ftdi_set_bitmode(ftdi1, 0xFF, BITMODE_BITBANG);
    }
    else if (gpioport == 2)
    {
        ftdi_set_bitmode(ftdi2, 0xFF, BITMODE_BITBANG);
    }
    else
    {
        fprintf(stderr, "%d is not a valid port!\n", gpioport);
        return 1;
    }
    return 0;
}

int disableBitBang(uint8_t port)
{

    gpioport = idPort(port);

    DBG("disabling bitbang mode(channel %i)\n", gpioport);

    if (gpioport == 1)
    {
        ftdi_disable_bitbang(ftdi1);
    }
    else if (gpioport == 2)
    {
        ftdi_disable_bitbang(ftdi2);
    }
    else
    {
        fprintf(stderr, "%d is not a valid port!\n", gpioport);
        return 1;
    }
    return 0;
}

int closePort(uint8_t port)
{

    gpioport = idPort(port);

    DBG("closing port %i\n", gpioport);

    if (gpioport == 1)
    {
        ftdi_usb_close(ftdi1);
        ftdi_free(ftdi1);
    }
    else if (gpioport == 2)
    {
        ftdi_usb_close(ftdi2);
        ftdi_free(ftdi2);
    }
    else
    {
        fprintf(stderr, "%d is not a valid port!\n", gpioport);
        return 1;
    }
    return 0;
}

void pinMode(uint8_t pin, uint8_t mode)
{
    gpioport = idPortByPin(pin);
    gpiop = calcPin(pin, gpioport);
    switch (gpioport)
    {
    case 1:
        if (mode == 0)
        {
            maskport1[0] &= ~(1 << gpiop);
        }
        else
        {
            maskport1[0] |= mode << gpiop;
        }
        DBG("set gpioport: %i, pin: %i, mode: %i, mask: " PRINTF_BINARY_PATTERN_INT8 "\n",
            gpioport, gpiop, mode, PRINTF_BYTE_TO_BINARY_INT8(maskport1[0]));
        f_port = ftdi_set_bitmode(ftdi1, maskport1[0], BITMODE_BITBANG);
        break;
    case 2:
        if (mode == 0)
        {
            maskport2[0] &= ~(1 << gpiop);
        }
        else
        {
            maskport2[0] |= mode << gpiop;
        }
        DBG("set gpioport: %i, pin: %i, mode: %i, mask: " PRINTF_BINARY_PATTERN_INT8 "\n",
            gpioport, gpiop, mode, PRINTF_BYTE_TO_BINARY_INT8(maskport2[0]));
        f_port = ftdi_set_bitmode(ftdi2, maskport2[0], BITMODE_BITBANG);
        break;
    default:
        break;
    }
}

void digitalWrite(uint8_t pin, int level)
{

    gpioport = idPortByPin(pin);
    gpiop = calcPin(pin, gpioport);

    switch (gpioport)
    {
    case 1:

        f_port = ftdi_read_pins(ftdi1, gpiobuff1);
        DBG("gpioport: %i, pin: %i, level: %i, read buff: " PRINTF_BINARY_PATTERN_INT8 " | ",
            gpioport, gpiop, level, PRINTF_BYTE_TO_BINARY_INT8(gpiobuff1[0]));
        if (level == 0)
        {
            gpiobuff1[0] &= ~(1 << gpiop);
        }
        else
        {
            gpiobuff1[0] |= level << gpiop;
        }
        f_port = ftdi_write_data(ftdi1, gpiobuff1, 1);
        DBG("write buff: " PRINTF_BINARY_PATTERN_INT8 "\n",
            PRINTF_BYTE_TO_BINARY_INT8(gpiobuff1[0]));
        break;
    case 2:

        f_port = ftdi_read_pins(ftdi2, gpiobuff2);
        DBG("gpioport: %i, pin: %i, level: %i, read buff: " PRINTF_BINARY_PATTERN_INT8 " | ",
            gpioport, gpiop, level, PRINTF_BYTE_TO_BINARY_INT8(gpiobuff2[0]));
        if (level == 0)
        {
            gpiobuff2[0] &= ~(1 << gpiop);
        }
        else
        {
            gpiobuff2[0] |= level << gpiop;
        }
        f_port = ftdi_write_data(ftdi2, gpiobuff2, 1);
        DBG("write buff: " PRINTF_BINARY_PATTERN_INT8 "\n",
            PRINTF_BYTE_TO_BINARY_INT8(gpiobuff2[0]));

        break;
    default:
        break;
    }
}

int digitalRead(uint8_t pin)
{

    gpioport = idPortByPin(pin);
    gpiop = calcPin(pin, gpioport);
    int result;

    switch (gpioport)
    {
    case 1:

        f_port = ftdi_read_pins(ftdi1, gpiobuff1);
        DBG("gpioport: %i, pin: %i, read buff: " PRINTF_BINARY_PATTERN_INT8 " | ",
            gpioport, gpiop, PRINTF_BYTE_TO_BINARY_INT8(gpiobuff1[0]));
        result = gpiobuff1[0] >> gpiop;
        break;
    case 2:

        f_port = ftdi_read_pins(ftdi2, gpiobuff2);
        DBG("gpioport: %i, pin: %i, read buff: " PRINTF_BINARY_PATTERN_INT8 " | ",
            gpioport, gpiop, PRINTF_BYTE_TO_BINARY_INT8(gpiobuff2[0]));
        result = gpiobuff2[0] >> gpiop;
        break;

    default:
        result = -1;
        break;
    }
    return result;
}
