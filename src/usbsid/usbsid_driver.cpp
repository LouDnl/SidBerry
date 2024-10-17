#include "usbsid_driver.h"

// #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

unsigned char result[LEN_IN_BUFFER];
int out_buffer_length;

int usbSIDSetup(void)
{
    rc = read_completed = write_completed = -1;
    out_buffer_length = (ASYNC_THREADING == 0) ? LEN_OUT_BUFFER : 4 /* LEN_OUT_BUFFER_ASYNC */;
    // out_buffer_length = 64;

    /* - set line encoding: here 9600 8N1
     * 9600 = 0x2580 -> 0x80, 0x25 in little endian
     * 115200 = 0x1C200 -> 0x00, 0xC2, 0x01 in little endian
     * 921600 = 0xE1000 -> 0x00, 0x10, 0x0E in little endian
     * 2000000 = 0x1E8480 -> 0x80, 0x84, 0x1E in little endian
     * 9000000 = 0x895440 -> 0x40, 0x54, 0x89 in little endian
     */
    unsigned char encoding[] = { 0x40, 0x54, 0x89, 0x00, 0x00, 0x00, 0x08 };

     /* Initialize libusb */
    rc = libusb_init(NULL);
    // rc = libusb_init_context(&ctx, /*options=NULL, /*num_options=*/0);  // NOTE: REQUIRES LIBUSB 1.0.27!!
    if (rc < 0) {
        fprintf(stderr, "Error initializing libusb: %d %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
        goto out;
    }

    /* Set debugging output to min/max (4) level */
    libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, 0);

    /* Look for a specific device and open it. */
    devh = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
    if (!devh) {
        fprintf(stderr, "Error opening USB device with VID & PID: %d %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
        rc = -1;
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
            fprintf(stderr, "Error claiming interface: %d, %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
            rc = -1;
            goto out;
        }
    }

    /* Start configuring the device:
     * - set line state
     */
    rc = libusb_control_transfer(devh, 0x21, 0x22, ACM_CTRL_DTR | ACM_CTRL_RTS, 0, NULL, 0, 0);
    if (rc < 0) {
        fprintf(stderr, "Error configuring line state during control transfer: %d, %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
        rc = -1;
        goto out;
    }

    rc = libusb_control_transfer(devh, 0x21, 0x20, 0, 0, encoding, sizeof(encoding), 0);
    if (rc < 0) {
        fprintf(stderr, "Error configuring line encoding during control transfer: %d, %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
        rc = -1;
        goto out;
    }

    if (ASYNC_THREADING == 0) {
        out_buffer = libusb_dev_mem_alloc(devh, LEN_OUT_BUFFER);
        transfer_out = libusb_alloc_transfer(0);
        libusb_fill_bulk_transfer(transfer_out, devh, ep_out_addr, out_buffer, LEN_OUT_BUFFER, sid_out, &write_completed, 0);

        in_buffer = libusb_dev_mem_alloc(devh, LEN_IN_BUFFER);
        transfer_in = libusb_alloc_transfer(0);
        libusb_fill_bulk_transfer(transfer_in, devh, ep_in_addr, in_buffer, LEN_IN_BUFFER, sid_in, &read_completed, 0);
    } else {
        out_buffer = libusb_dev_mem_alloc(devh, LEN_OUT_BUFFER_ASYNC);
        transfer_out = libusb_alloc_transfer(0);
        libusb_fill_bulk_transfer(transfer_out, devh, ep_out_addr, out_buffer, 4/* LEN_OUT_BUFFER_ASYNC */, sid_out, &write_completed, 0);
        rc = libusb_submit_transfer(transfer_out);
        if (rc < 0) {
            fprintf(stderr, "Error during transfer_out submit transfer: %s\n", libusb_error_name(rc));
            rc = -1;
            goto out;
        }
    }

    if (ASYNC_THREADING == 1) {
        pthread_create(&ptid, NULL, &usbSIDStart, NULL);
        fprintf(stdout, "[USBSID] Thread created\r\n");
    }

    return rc;
out:
    usbSIDExit();
    return rc;
}

int usbSIDExit(void)
{
    usbSIDPause();

    rc = libusb_cancel_transfer(transfer_out);
    if (rc < 0 && rc != -5)
        fprintf(stderr, "Failed to cancel transfer %d - %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));

    rc = libusb_cancel_transfer(transfer_in);
    if (rc < 0 && rc != -5)
        fprintf(stderr, "Failed to cancel transfer %d - %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));

    rc = libusb_dev_mem_free(devh, in_buffer, LEN_IN_BUFFER);
    if (rc < 0)
        fprintf(stderr, "Failed to free in_buffer DMA memory: %d, %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));

    rc = libusb_dev_mem_free(devh, out_buffer, out_buffer_length);
    if (rc < 0)
        fprintf(stderr, "Failed to free out_buffer DMA memory: %d, %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));

    for (int if_num = 0; if_num < 2; if_num++) {
        if (libusb_kernel_driver_active(devh, if_num)) {
            rc = libusb_detach_kernel_driver(devh, if_num);
            fprintf(stderr, "libusb_detach_kernel_driver error: %d, %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
        }
        libusb_release_interface(devh, if_num);
    }

    if (devh) {
        libusb_close(devh);
    }
    libusb_exit(NULL);

    devh = NULL;
    fprintf(stdout, "Linux usbsid: closed\r\n");
    return 0;
}

void *usbSIDStart(void*)
{
    fprintf(stdout, "[USBSID] Thread started\r\n");
    /* pthread_detach(pthread_self());
    fprintf(stdout, "[USBSID] Thread detached"); */

    while(!exit_thread) {
        while (!write_completed)
            libusb_handle_events_completed(ctx, &write_completed);
    }
    fprintf(stdout, "[USBSID] Thread finished\r\n");
    pthread_exit(NULL);
    return NULL;
}
float timedifference_msec(struct timespec t0, struct timespec t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000000.0f + (t1.tv_nsec - t0.tv_nsec) / 1000.0f;
}
void usbSIDWrite(unsigned char *buff)
{
    /* struct timespec t0; */
    /* struct timespec t1; */
    /* float elapsed; */
    /* clock_gettime(CLOCK_MONOTONIC_RAW, &t0); */
    /* clock_t tic = clock(); */
    write_completed = 0;
    // memcpy(out_buffer, buff, 4);
    memcpy(out_buffer, buff, 3);
    if (ASYNC_THREADING == 0) {
        libusb_submit_transfer(transfer_out);
        libusb_handle_events_completed(ctx, NULL);
    }
    /* clock_t toc = clock(); */
    /* fprintf(stdout, "[W]@%02x [D]%02x [uS]%.2d [S]%f\n", buff[1], buff[2], (int)((double)(toc - tic) / CLOCKS_PER_SEC * 1000000), (double)(toc - tic) / CLOCKS_PER_SEC); */

    /* clock_gettime(CLOCK_MONOTONIC_RAW, &t1); */
    /* fprintf(stdout, "[W]@%02x [D]%02x [uS]%f\n", buff[1], buff[2], timedifference_msec(t0, t1)); */
}

void usbSIDRead_toBuff(unsigned char *writebuff)
{
    if (ASYNC_THREADING == 0) {  /* Reading not supported with async write */
        /* clock_t tic = clock(); */
        /* struct timespec t0; */
        /* struct timespec t1; */
        /* float elapsed; */
        /* clock_gettime(CLOCK_MONOTONIC_RAW, &t0); */

        read_completed = 0;
        memcpy(out_buffer, writebuff, 3);
        libusb_submit_transfer(transfer_out);
        libusb_handle_events_completed(ctx, NULL);
        libusb_submit_transfer(transfer_in);
        libusb_handle_events_completed(ctx, &read_completed);
        /* clock_t toc = clock();
        fprintf(stdout, "[R]@%02x [D]%02x [uS]%.2d [S]%f\n", writebuff[1], result[0], (int)((double)(toc - tic) / CLOCKS_PER_SEC * 1000000), (double)(toc - tic) / CLOCKS_PER_SEC); */

        /* clock_gettime(CLOCK_MONOTONIC_RAW, &t1); */
        /* fprintf(stdout, "[R]@%02x [D]%02x [uS]%f\n", writebuff[1], result[0], timedifference_msec(t0, t1)); */
    }
}

unsigned char usbSIDRead(unsigned char *writebuff, unsigned char *buff)
{
    if (ASYNC_THREADING == 0) {  /* Reading not supported with async write */
        usbSIDRead_toBuff(writebuff);
        memcpy(buff, in_buffer, 1);
    } else {
        buff[0] = 0xFF;
    }

    return buff[0];
}

void usbSIDPause(void)
{
    unsigned char buff[4] = {0x2, 0x0, 0x0, 0x0};
    usbSIDWrite(buff);
}

void usbSIDReset(void)
{
    unsigned char buff[4] = {0x3, 0x0, 0x0, 0x0};
    usbSIDWrite(buff);
}

static void LIBUSB_CALL sid_out(struct libusb_transfer *transfer)
{

    write_completed = (*(int *)transfer->user_data);


    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        rc = transfer->status;
        if (rc != LIBUSB_TRANSFER_CANCELLED) {
		    fprintf(stderr, "Warning: transfer out interrupted with status %d, %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
            print_libusb_transfer(transfer);
        }
		libusb_free_transfer(transfer);
        return;
    }

    if (transfer->actual_length != out_buffer_length) {
        fprintf(stderr, "Sent data length %d is different from the defined buffer length: %d or actual length %d\n", transfer->length, out_buffer_length, transfer->actual_length);
        print_libusb_transfer(transfer);
    }

    {
        USBSIDDBG("SID_OUT: ");
        for (int i = 0; i <= sizeof(transfer->buffer); i++) USBSIDDBG("$%02x ", transfer->buffer[i]);
        USBSIDDBG("\r\n");
    }

    if (ASYNC_THREADING == 1) {
        rc = libusb_submit_transfer(transfer);
        if (rc < 0) {
            fprintf(stderr, "Error during transfer_out submit transfer: %s\n", libusb_error_name(rc));
        }
    }

    write_completed = 1;
}

static void LIBUSB_CALL sid_in(struct libusb_transfer *transfer)
{

    read_completed = (*(int *)transfer->user_data);

    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        rc = transfer->status;
		if (rc != LIBUSB_TRANSFER_CANCELLED) {
            fprintf(stderr, "Warning: transfer in interrupted with status '%s'\n", libusb_error_name(rc));
            print_libusb_transfer(transfer);
        }
        libusb_free_transfer(transfer);
        return;
    }

    /* if (transfer->actual_length != LEN_IN_BUFFER) {
        fprintf(stderr, "Received data length %d is different from the defined buffer length: %d or actual length: %d\n", transfer->length, LEN_IN_BUFFER, transfer->actual_length);
        print_libusb_transfer(transfer_in);
    } */

    read_completed = 1;
    memcpy(result, in_buffer, 1); /* Copy read data to result */

    {
        USBSIDDBG("SID_IN: ");
        for (int i = 0; i <= sizeof(transfer->buffer); i++) USBSIDDBG("$%02x ", transfer->buffer[i]);
        USBSIDDBG("\r\n");
    }
}

void print_libusb_transfer(struct libusb_transfer *p_t)
{
    int i;
	if ( NULL == p_t) {
		printf("libusb_transfer is empty\n");
	}
	else {
		printf("\r\n");
        printf("libusb_transfer structure:\n");
		printf("flags         =%x \n", p_t->flags);
		printf("endpoint      =%x \n", p_t->endpoint);
		printf("type          =%x \n", p_t->type);
		printf("timeout       =%d \n", p_t->timeout);
		printf("status        =%d '%s': '%s'\n", p_t->status, libusb_error_name(p_t->status), libusb_strerror(p_t->status));
		// length, and buffer are commands sent to the device
		printf("length        =%d \n", p_t->length);
		printf("actual_length =%d \n", p_t->actual_length);
		printf("user_data     =%p \n", p_t->user_data);
		printf("buffer        =%p \n", p_t->buffer);
		printf("buffer        =%x \n", *p_t->buffer);

		for (i=0; i < p_t->length; i++){
			printf("[%d: %x]", i, p_t->buffer[i]);
		}
        printf("\r\n");
	}
	return;
}


uint16_t sid_address(uint16_t addr)
{
  /* Set address for SID no# */
  /* D500, DE00 or DF00 is the second sid in SIDTYPE1, 3 & 4 */
  /* D500, DE00 or DF00 is the third sid in all other SIDTYPE */
  switch (addr) {
    case 0xD400 ... 0xD499:
      switch (SIDTYPE) {
        case SIDTYPE1:
        case SIDTYPE3:
        case SIDTYPE4:
          return addr; /* $D400 -> $D479 */
          break;
        case SIDTYPE2:
          return ((addr & SIDLMASK) >= 0x20) ? (SID2ADDR | (addr & SID1MASK)) : addr;
          break;
        case SIDTYPE5:
          return ((addr & SIDLMASK) >= 0x20) && ((addr & SIDLMASK) <= 0x39) /* $D420~$D439 -> $D440~$D459 */
                  ? (addr + 0x20)
                  : ((addr & SIDLMASK) >= 0x40) && ((addr & SIDLMASK) <= 0x59) /* $D440~$D459 -> $D420~$D439 */
                  ? (addr - 0x20)
                  : addr;
          break;
      }
      break;
    case 0xD500 ... 0xD599:
    case 0xDE00 ... 0xDF99:
      switch (SIDTYPE) {
        case SIDTYPE1:
        case SIDTYPE2:
          return (SID2ADDR | (addr & SID1MASK));
          break;
        case SIDTYPE3:
          return (SID3ADDR | (addr & SID1MASK));
          break;
        case SIDTYPE4:
          return (SID3ADDR | (addr & SID2MASK));
          break;
        case SIDTYPE5:
          return (SID1ADDR | (addr & SIDUMASK));
          break;
      }
      break;
    default:
      return addr;
      break;
  }

}
