//============================================================================
// Description : SidBerry wrapper to GPIO ports
// Authors     : Alessio Lombardo, LouD
// Last update : 2024
//============================================================================

#include "gpioInterface.h"

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

#define VENDOR_ID      0x5553
#define PRODUCT_ID     0x4001
#define ACM_CTRL_DTR   0x01
#define ACM_CTRL_RTS   0x02
static struct libusb_device_handle *devh = NULL;
static int ep_in_addr = 0x02;
static int ep_out_addr  = 0x82;

int gpioSetup()
{
	int rc;
	/* - set line encoding: here 9600 8N1
     * 9600 = 0x2580 -> 0x80, 0x25 in little endian
	 * 115200 = 0x1C200 -> 0x00, 0xC2, 0x01 in little endian
	 * 921600 = 0xE1000 -> 0x00, 0x10, 0x0E in little endian
     */
    unsigned char encoding[] = { 0x00, 0x10, 0x0E, 0x00, 0x00, 0x00, 0x08 };

 	/* Initialize libusb */
    rc = libusb_init(NULL);
	if (rc < 0) {
        fprintf(stderr, "Error initializing libusb: %s\n", libusb_error_name(rc));
        exit(1);
    }

	/* Set debugging output to max level. */
	// libusb_set_debug(NULL, 3);  /* DEPRECATED */
	libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, 3);

	/* Look for a specific device and open it. */
	devh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
    if (!devh) {
        fprintf(stderr, "Error finding USB device\n");
        goto out;
    }

	/* As we are dealing with a CDC-ACM device, it's highly probable that
     * Linux already attached the cdc-acm driver to this device.
     * We need to detach the drivers from all the USB interfaces. The CDC-ACM
     * Class defines two interfaces: the Control interface and the
     * Data interface.
     */
    for (int if_num = 0; if_num < 2; if_num++) {
        if (libusb_kernel_driver_active(devh, if_num)) {
            libusb_detach_kernel_driver(devh, if_num);
        }
        rc = libusb_claim_interface(devh, if_num);
        if (rc < 0) {
            fprintf(stderr, "Error claiming interface: %s\n",
                    libusb_error_name(rc));
            goto out;
        }
    }

    /* Start configuring the device:
     * - set line state
     */
    rc = libusb_control_transfer(devh, 0x21, 0x22, ACM_CTRL_DTR | ACM_CTRL_RTS,
                                0, NULL, 0, 0);
    if (rc < 0) {
        fprintf(stderr, "Error during control transfer: %s\n",
                libusb_error_name(rc));
    }

    /* - set line encoding: here 9600 8N1
     * 9600 = 0x2580 -> 0x80, 0x25 in little endian
	 * 115200 = 0x1C200 -> 0x00, 0xC2, 0x01 in little endian
     */
    // unsigned char encoding[] = { 0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08 };
    rc = libusb_control_transfer(devh, 0x21, 0x20, 0, 0, encoding,
                                sizeof(encoding), 0);
    if (rc < 0) {
        fprintf(stderr, "Error during control transfer: %s\n",
                libusb_error_name(rc));
    }

	return rc;
out:
    if (devh)
		libusb_close(devh);
    libusb_exit(NULL);
    return rc;
}

void sidWrite(unsigned char *buff)
{
	int size = sizeof(buff);
	int actual_length;
	if (libusb_bulk_transfer(devh, ep_in_addr, buff, size,
							&actual_length, 0) < 0)
	{
		fprintf(stderr, "Error while sending char\n");
	}
}

unsigned char sidRead(unsigned char *writebuff, unsigned char *buff)
{
	sidWrite(writebuff);
	unsigned char readbuffer[4];
	int rc, actual_length;
	rc = libusb_bulk_transfer(devh, ep_out_addr, readbuffer, sizeof(readbuffer), &actual_length, 1000);
	if (rc == LIBUSB_ERROR_TIMEOUT) {
		fprintf(stderr, "Timeout (%d)\n", actual_length);
		return -1;
	} else if (rc < 0) {
		fprintf(stderr, "Error while waiting for char\n");
	}
	buff[0] = readbuffer[0];
	return actual_length;
}

void pauseSID(void)
{
	unsigned char buff[4] = {0x80, 0x0, 0x0, 0x0};
	sidWrite(buff);
}

void resetSID(void)
{
	unsigned char buff[4] = {0x40, 0x0, 0x0, 0x0};
	sidWrite(buff);
}

int gpioStop()
{
	unsigned char buff[4] = {0x0, 0x0, 0x0, 0x0};
	sidWrite(buff);
    if (devh)
            libusb_close(devh);
    libusb_exit(NULL);
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
