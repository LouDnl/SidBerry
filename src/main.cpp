//============================================================================
// Description : SidBerry player & driver
// Authors     : Gianluca Ghettini, Alessio Lombardo, LouD
// Last update : 2024
//============================================================================

#include "mos6502.h"
#include "SidFile.h"
#include "sidberry.h"

#pragma GCC diagnostic ignored "-Wnarrowing"

uint8_t memory[65536];         // init 64K ram
int sidcount = 1;              // default to 1 sid
int custom_clock = 0;          // default custom clock to 0
int custom_hertz = 0;          // default custom hertz to 0
int volume = 15;               // init volume at 10. max is 15
bool verbose = false;           // init verbose boolean
bool trace = false;               // init trace boolean
bool calculatedclock = false;  // init calculated clock speed boolean
bool calculatedhz = false;       // init calculated refresh boolean
volatile sig_atomic_t stop;    // init variable for ctrl+c
#if BOARD == USBSIDPICO
bool real_read = true;         // use actual pin reading when USBSID-Pico
#else
bool real_read = false;        // use actual pin reading when true
#endif
timeval t1, t2;
long int elaps;


void inthand(int signum)
{
    exitPlayer();
    stop = 1;
}

void exitPlayer(void)  // TODO: Add wait for current USB transfer to finish for clean exit
{
    printf("\n** Exit **\n");
    for (int j = 0; j < sidcount; j++) {
        for (int i = 0xD400; i < 0xD418; i++) {
            MemWrite((i + (j * 0x20)), 0);
        }
    }
    #if BOARD == USBSIDPICO
    resetSID();
    #else
    gpioWrite(RES, LOW);  // make sure any sound stops by resetting the sid
    #endif
    gpioStop();
}

void enableGpio(void)
{
    #if BOARD == USBSIDPICO
    /* Do nothing, not needed */
    #else
    // address pins are always output
    gpioMode(A0, OUTPUT);
    gpioMode(A1, OUTPUT);
    gpioMode(A2, OUTPUT);
    gpioMode(A3, OUTPUT);
    gpioMode(A4, OUTPUT);

    #if BOARD == FTDI
    gpioMode(RES, OUTPUT); // FTDI has reset pin
    gpioMode(RW, OUTPUT);  // FTDI has read/write pin
    #endif
    gpioMode(CS, OUTPUT); // ChipSelect

    #if BOARD == FTDI
    gpioWrite(RES, HIGH); // FTDI reset always high so the SID is enabled
    #endif
    gpioWrite(CS, HIGH);
    #endif
    printf("Setup finished\n");
}

void TestWrite(uint16_t addr, uint8_t byte)
{
    printf("\na: %02X, b: %02X", addr, byte);

    uint16_t phyaddr = addr;

    // set address to the bus
    if (phyaddr & 0x01)
        gpioWrite(A0, HIGH);
    else
        gpioWrite(A0, LOW);
    if (phyaddr & 0x02)
        gpioWrite(A1, HIGH);
    else
        gpioWrite(A1, LOW);
    if (phyaddr & 0x04)
        gpioWrite(A2, HIGH);
    else
        gpioWrite(A2, LOW);
    if (phyaddr & 0x08)
        gpioWrite(A3, HIGH);
    else
        gpioWrite(A3, LOW);
    if (phyaddr & 0x10)
        gpioWrite(A4, HIGH);
    else
        gpioWrite(A4, LOW);

    // set data to the bus
    if (byte & 0x01)
        gpioWrite(D0, HIGH);
    else
        gpioWrite(D0, LOW);
    if (byte & 0x02)
        gpioWrite(D1, HIGH);
    else
        gpioWrite(D1, LOW);
    if (byte & 0x04)
        gpioWrite(D2, HIGH);
    else
        gpioWrite(D2, LOW);
    if (byte & 0x08)
        gpioWrite(D3, HIGH);
    else
        gpioWrite(D3, LOW);
    if (byte & 0x10)
        gpioWrite(D4, HIGH);
    else
        gpioWrite(D4, LOW);
    if (byte & 0x20)
        gpioWrite(D5, HIGH);
    else
        gpioWrite(D5, LOW);
    if (byte & 0x40)
        gpioWrite(D6, HIGH);
    else
        gpioWrite(D6, LOW);
    if (byte & 0x80)
        gpioWrite(D7, HIGH);
    else
        gpioWrite(D7, LOW);

    // assert cs line
    gpioWrite(CS, LOW);
    usleep(500000);
    gpioWrite(CS, HIGH);

    printf("\nOK\n");

    return;
}

void TestLoop(void)
{
    int addr;
    int byte;
    printf("\n\n");

    while (1)
    {
        printf("\naddr:");
        scanf("%d", &addr);
        printf("\nbyte:");
        scanf("%d", &byte);
        TestWrite(addr, byte);
    }
}

void SingleWrite(uint16_t addr, uint8_t byte)  // TODO: Add USBSID-Pico support
{
    #if BOARD == FTDI
    pinModePort(PORT1, 0xFF); // Output
    gpioWrite(RW, HIGH);
    #endif

    memory[addr] = byte;
    uint16_t phyaddr = addr & 0x1f;


    // NOTE: If you use a slow connection to tty device, the printf function may affect the playback speed
    DBG("one single memwrite ~ addr: %04x byte: %04x phyaddr: %04x | Synth 1: $%02X%02X %02X%02X %02X %02X %02X | Synth 2: $%02X%02X %02X%02X %02X %02X %02X | Synth 3: $%02X%02X %02X%02X %02X %02X %02X\n",
        addr, byte, phyaddr,
        memory[0xD400], memory[0xD401], memory[0xD402], memory[0xD403], memory[0xD404], memory[0xD405], memory[0xD406],
        memory[0xD407], memory[0xD408], memory[0xD409], memory[0xD40A], memory[0xD40B], memory[0xD40C], memory[0xD40D],
        memory[0xD40E], memory[0xD40F], memory[0xD410], memory[0xD411], memory[0xD412], memory[0xD413], memory[0xD414]);

    // set address to the bus
    if (phyaddr & 0x01)
        gpioWrite(A0, HIGH);
    else
        gpioWrite(A0, LOW);
    if (phyaddr & 0x02)
        gpioWrite(A1, HIGH);
    else
        gpioWrite(A1, LOW);
    if (phyaddr & 0x04)
        gpioWrite(A2, HIGH);
    else
        gpioWrite(A2, LOW);
    if (phyaddr & 0x08)
        gpioWrite(A3, HIGH);
    else
        gpioWrite(A3, LOW);
    if (phyaddr & 0x10)
        gpioWrite(A4, HIGH);
    else
        gpioWrite(A4, LOW);

    // set data to the bus
    if (byte & 0x01)
        gpioWrite(D0, HIGH);
    else
        gpioWrite(D0, LOW);
    if (byte & 0x02)
        gpioWrite(D1, HIGH);
    else
        gpioWrite(D1, LOW);
    if (byte & 0x04)
        gpioWrite(D2, HIGH);
    else
        gpioWrite(D2, LOW);
    if (byte & 0x08)
        gpioWrite(D3, HIGH);
    else
        gpioWrite(D3, LOW);
    if (byte & 0x10)
        gpioWrite(D4, HIGH);
    else
        gpioWrite(D4, LOW);
    if (byte & 0x20)
        gpioWrite(D5, HIGH);
    else
        gpioWrite(D5, LOW);
    if (byte & 0x40)
        gpioWrite(D6, HIGH);
    else
        gpioWrite(D6, LOW);
    if (byte & 0x80)
        gpioWrite(D7, HIGH);
    else
        gpioWrite(D7, LOW);

    // assert cs line
    gpioWrite(CS, LOW);
    usleep(1);
    gpioWrite(CS, HIGH);

    return;
}

uint8_t SingleRead(uint16_t addr)  // NOTICE: USB devices only
{
    if (addr >= 0x0000 && addr <= 0xFFFF) // address decoding logic ~ needed otherwise ALL memreads are sent here
    {
        DBG("\nOne single memread addr: %i, hex: 0x%04x\n", addr, addr);
        #if BOARD != USBSIDPICO
        uint16_t phyaddr = addr & 0x1f;
        // set address to the bus
        if (phyaddr & 0x01)
            gpioWrite(A0, HIGH);
        else
            gpioWrite(A0, LOW);
        if (phyaddr & 0x02)
            gpioWrite(A1, HIGH);
        else
            gpioWrite(A1, LOW);
        if (phyaddr & 0x04)
            gpioWrite(A2, HIGH);
        else
            gpioWrite(A2, LOW);
        if (phyaddr & 0x08)
            gpioWrite(A3, HIGH);
        else
            gpioWrite(A3, LOW);
        if (phyaddr & 0x10)
            gpioWrite(A4, HIGH);
        else
            gpioWrite(A4, LOW);

        unsigned char dataresult[1];
        unsigned char dresult[7];

        #if BOARD == FTDI
        ftdi_tciflush(ftdi1);
        digitalReadBus(PORT1);  // ISSUE: Not really working with FTDIBerry
        #endif

        gpioWrite(CS, LOW);
        usleep(1);
        gpioWrite(CS, HIGH);
        #endif
        #if BOARD == USBSIDPICO
        /* USBSID code */
        uint16_t phyaddr = addr & 0xFF;
        unsigned char result[1];
        unsigned char buff[4];  /* 4 Byte buffer */
        buff[0] = 0x0;  /* Read */
        buff[1] = (addr >> 8);  /* Address Byte 1 ~ 0x4D */
        buff[2] = phyaddr;  /* Address Byte 2 ~ 0x1C */
        buff[3] = 0x0;  /* Data bus value */
        sidRead(buff, result);
        if (trace == true && verbose != true) printf("Read result: 0x%02x 0b" PRINTF_BINARY_PATTERN_INT8 " from addr: 0x%04x \r\n", result[0], PRINTF_BYTE_TO_BINARY_INT8(result[0]), addr);
        #endif

    }
    return memory[addr];
}

void MemWrite(uint16_t addr, uint8_t byte)  // NOTICE: USB devices only
{
    if (addr >= 0xD400 && addr <= 0xDFFF)
    {
        // access to SID chip

        memory[addr] = byte;

        if (verbose && !trace)
        {
            // NOTE: If you use a slow connection to tty device, the printf function may affect the playback speed
            printf("Voice 1: $%02X%02X %02X%02X %02X %02X %02X | Voice 2: $%02X%02X %02X%02X %02X %02X %02X | Voice 3: $%02X%02X %02X%02X %02X %02X %02X | Filter: %02X %02X %02X Vol: %02X \n",
                   memory[0xD400], memory[0xD401], memory[0xD402], memory[0xD403], memory[0xD404], memory[0xD405], memory[0xD406],
                   memory[0xD407], memory[0xD408], memory[0xD409], memory[0xD40A], memory[0xD40B], memory[0xD40C], memory[0xD40D],
                   memory[0xD40E], memory[0xD40F], memory[0xD410], memory[0xD411], memory[0xD412], memory[0xD413], memory[0xD414],
                   memory[0xD415], memory[0xD416], memory[0xD417], memory[0xD418]);
        }

        uint16_t phyaddr = addr & 0x7F;     /* 4 SIDs max */

        #if BOARD != USBSIDPICO
        if (verbose && trace)
        {
            // NOTE: If you use a slow connection to tty device, the printf function may affect the playback speed
            printf("one memwrite ~ addr: %04x byte: %04x phyaddr: %04x | Voice 1: $%02X%02X %02X%02X %02X %02X %02X | Voice 2: $%02X%02X %02X%02X %02X %02X %02X | Voice 3: $%02X%02X %02X%02X %02X %02X %02X | Filter: %02X %02X %02X Vol: %02X \n",
                   addr, byte, phyaddr,
                   memory[0xD400], memory[0xD401], memory[0xD402], memory[0xD403], memory[0xD404], memory[0xD405], memory[0xD406],
                   memory[0xD407], memory[0xD408], memory[0xD409], memory[0xD40A], memory[0xD40B], memory[0xD40C], memory[0xD40D],
                   memory[0xD40E], memory[0xD40F], memory[0xD410], memory[0xD411], memory[0xD412], memory[0xD413], memory[0xD414],
                   memory[0xD415], memory[0xD416], memory[0xD417], memory[0xD418]);
        }
        #endif
        #if BOARD == FTDI
        if (real_read == true)
        {
            pinModePort(PORT1, 0xFF); // Port to output
        }
        #endif

        #if BOARD == USBSIDPICO
        unsigned char buff[4];  /* 4 Byte buffer */
        buff[0] = 0x10;         /* Write */
        buff[1] = (addr >> 8);  /* Address Byte 1 ~ 0xD4 */
        buff[2] = phyaddr;      /* Address Byte 2 ~ 0x1C */
        buff[3] = byte;         /* Data bus value */
        sidWrite(buff);
        if (verbose && trace)
        {
            printf("one memwrite ~ $addr: %04x phyrange: %02x phyaddr: %02x byte: %02x\n",
                   addr, buff[1], phyaddr, byte);
        }
        #else

        // set address to the bus
        if (phyaddr & 0x01)
            gpioWrite(A0, HIGH);
        else
            gpioWrite(A0, LOW);
        if (phyaddr & 0x02)
            gpioWrite(A1, HIGH);
        else
            gpioWrite(A1, LOW);
        if (phyaddr & 0x04)
            gpioWrite(A2, HIGH);
        else
            gpioWrite(A2, LOW);
        if (phyaddr & 0x08)
            gpioWrite(A3, HIGH);
        else
            gpioWrite(A3, LOW);
        if (phyaddr & 0x10)
            gpioWrite(A4, HIGH);
        else
            gpioWrite(A4, LOW);

        // set data to the bus
        if (byte & 0x01)
            gpioWrite(D0, HIGH);
        else
            gpioWrite(D0, LOW);
        if (byte & 0x02)
            gpioWrite(D1, HIGH);
        else
            gpioWrite(D1, LOW);
        if (byte & 0x04)
            gpioWrite(D2, HIGH);
        else
            gpioWrite(D2, LOW);
        if (byte & 0x08)
            gpioWrite(D3, HIGH);
        else
            gpioWrite(D3, LOW);
        if (byte & 0x10)
            gpioWrite(D4, HIGH);
        else
            gpioWrite(D4, LOW);
        if (byte & 0x20)
            gpioWrite(D5, HIGH);
        else
            gpioWrite(D5, LOW);
        if (byte & 0x40)
            gpioWrite(D6, HIGH);
        else
            gpioWrite(D6, LOW);
        if (byte & 0x80)
            gpioWrite(D7, HIGH);
        else
            gpioWrite(D7, LOW);

        if (real_read == true) gpioWrite(RW, LOW);  // rw to low for writing
        // assert CS line (strobe)
        gpioWrite(CS, LOW);
        usleep(1);
        gpioWrite(CS, HIGH);
        if (real_read == true) {
            gpioWrite(RW, HIGH);      // rw to high for reading
        #if BOARD == FTDI
            pinModePort(PORT1, 0x0);  // Port to input
        #endif
        }
        #endif
    }
    else
    {
        // access to memory
        memory[addr] = byte;
    }
    return;
}

uint8_t MemRead(uint16_t addr)
{

    if (addr >= 0xD400 && addr <= 0xDFFF) // address decoding logic
    {
        if (real_read == false)  // default
        {
            // Songs like Cantina_Band.sid from HVSC DEMOS use this!
            // access to SID chip
            if ((addr & 0x00FF) == 0x001B)
            {
                // emulate read access to OSC3/Random register, return a random value
                printf("\nread! 1b");
                return rand();
            }
            if ((addr & 0x00FF) == 0x001C)
            {
                // emulate access to envelope ENV3 register, return a random value
                printf("\nread! 1c");
                return rand();
            }
        } else
        {
            // Songs like Cantina_Band.sid from HVSC DEMOS use this!
            // access to SID chip
            if ((addr & 0x001F) == 0x001B || (addr & 0x001F) == 0x001C)
            {
                uint16_t phyaddr = addr & 0xFF;
                unsigned char result[1];

                #if BOARD == USBSIDPICO
                /* USBSID code */
                unsigned char buff[4];  /* 4 Byte buffer */
                buff[0] = 0x0;          /* Read */
                buff[1] = (addr >> 8);  /* Address Byte 1 ~ 0xD4 */
                buff[2] = phyaddr;      /* Address Byte 2 ~ 0x1C */
                buff[3] = 0x0;          /* Data bus value */
                sidRead(buff, result);
                #else
                // set address to the bus
                if (phyaddr & 0x01)
                    gpioWrite(A0, HIGH);
                else
                    gpioWrite(A0, LOW);
                if (phyaddr & 0x02)
                    gpioWrite(A1, HIGH);
                else
                    gpioWrite(A1, LOW);
                if (phyaddr & 0x04)
                    gpioWrite(A2, HIGH);
                else
                    gpioWrite(A2, LOW);
                if (phyaddr & 0x08)
                    gpioWrite(A3, HIGH);
                else
                    gpioWrite(A3, LOW);
                if (phyaddr & 0x10)
                    gpioWrite(A4, HIGH);
                else
                    gpioWrite(A4, LOW);

                // ftdi_tciflush(ftdi1);  // empty buffer
                gpioWrite(RW, HIGH);  // rw high for reading
                gpioWrite(CS, LOW);   // cs low to enable chip
                #if BOARD == FTDI
                result[0] = digitalReadBus(PORT1);  // read the port
                #endif
                gpioWrite(CS, HIGH);  // cs back to high to disable chip

                #endif
                if (trace == true && verbose != true) printf("Read result: 0x%02x 0b" PRINTF_BINARY_PATTERN_INT8 " from addr: 0x%04x \r\n", result[0], PRINTF_BYTE_TO_BINARY_INT8(result[0]), addr);
                return result[0];
            }
        }
    }
    return memory[addr];
}

int load_sid(mos6502 cpu, SidFile sid, int song_number)
{
    for (unsigned int i = 0; i < 65536; i++)
    {
        memory[i] = 0x00; // fill with NOPs
    }

    /* Make sure volume does not mute on play for SID1 to max sidcount */
    for (int i = 0; i < sidcount; i++) {
        MemWrite((VOL_ADDR + (i * 0x20)), 0xE);
        memory[(VOL_ADDR + (i * 0x20))] = 0xE;
    }

    uint16_t load = sid.GetLoadAddress();
    uint16_t len = sid.GetDataLength();
    uint8_t *buffer = sid.GetDataPtr();
    for (unsigned int i = 0; i < len; i++)
    {
        memory[i + load] = buffer[i];
    }

    uint16_t play = sid.GetPlayAddress();
    uint16_t init = sid.GetInitAddress();

    // install reset vector for microplayer (0x0000)
    memory[0xFFFD] = 0x00;
    memory[0xFFFC] = 0x00;

    // install IRQ vector for play routine launcher (0x0013)
    memory[0xFFFF] = 0x00;
    memory[0xFFFE] = 0x13;

    // install the micro player, 6502 assembly code

    memory[0x0000] = 0xA9; // A = 0, load A with the song number
    memory[0x0001] = song_number;

    memory[0x0002] = 0x20;                 // jump sub to INIT routine
    memory[0x0003] = init & 0xFF;         // lo addr
    memory[0x0004] = (init >> 8) & 0xFF; // hi addr

    memory[0x0005] = 0x58; // enable interrupt
    memory[0x0006] = 0xEA; // nop
    memory[0x0007] = 0x4C; // jump to 0x0006
    memory[0x0008] = 0x06;
    memory[0x0009] = 0x00;

    memory[0x0013] = 0xEA; // nop  //0xA9; // A = 1
    memory[0x0014] = 0xEA; // nop //0x01;
    memory[0x0015] = 0xEA; // 0x78 CLI
    memory[0x0016] = 0x20; // jump sub to play routine
    memory[0x0017] = play & 0xFF;
    memory[0x0018] = (play >> 8) & 0xFF;
    memory[0x0019] = 0xEA; // 0x58 SEI
    memory[0x001A] = 0x40; // RTI: return from interrupt

    cpu.Reset();
    cpu.Run(CLOCK_CYCLES); // 100000 clockcycles
    return 0;
}

int getch_noecho_special_char(void)
{
    int char_code = 0;
    int buf = 0;
    char buf2[3] = {0, 0, 0};
    const char *buf_erase_echo = string("\033[2K\r").data();

    struct termios old = {0};
    tcgetattr(0, &old);
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 0;
    old.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &old);

    read(0, &buf2[0], 1);
    read(0, &buf2[1], 1);
    read(0, &buf2[2], 1);
    if (buf2[0] != 0)
        write(0, buf_erase_echo, 5);

    if (buf2[0] == '\033' && buf2[1] == '\0')
    { // Escape Key
        buf = 256;
    }
    else if (buf2[0] == '\033' && buf2[1] == 91 && buf2[2] == 68)
    { // Left Arrow
        buf = 257;
    }
    else if (buf2[0] == '\033' && buf2[1] == 91 && buf2[2] == 67)
    { // Right Arrow
        buf = 258;
    }
    else
    {
        buf = buf2[0];
    }

    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    tcsetattr(0, TCSADRAIN, &old);

    return buf;
}

void change_player_status(mos6502 cpu, SidFile sid, int key_press, bool *paused, bool *exit, uint8_t *mode_vol_reg, int *song_number, int *sec, int *min)
{

    if (key_press == 256 || key_press == (int)'q')
    { // Escape (reset all registers and quit)
        exitPlayer();
        *paused = false;
        *exit = true;
    }
    else if (key_press == 32)  // space
    { // Pause
        if (*paused)
        {
            printf("\rPlay Sub-Song %d / %d [%02d:%02d] @ Volume: %d            ", (*song_number) + 1, sid.GetNumOfSongs(), *min, *sec, volume);
            fflush(stdout);
            for (int i = 0; i < sidcount; i++) {
                MemWrite((VOL_ADDR + (i * 0x20)), *mode_vol_reg);
            }
            *paused = false;
        }
        else
        {
            printf("\rPlay Sub-Song %d / %d [%02d:%02d] @ Volume: %d [PAUSED]   ", (*song_number) + 1, sid.GetNumOfSongs(), *min, *sec, volume);
            fflush(stdout);
            for (int i = 0; i < sidcount; i++) {
                MemWrite((VOL_ADDR + (i * 0x20)), 0);
            }
            #if BOARD == USBSIDPICO
            pauseSID();
            #endif
            *paused = true;
        }
    }
    else if (key_press == 119 || key_press == (int)'w')  // 119 W
    {
        if (volume < 15) {
            volume++;
        }
        *mode_vol_reg = volume;
        for (int i = 0; i < sidcount; i++) {
            MemWrite((VOL_ADDR + (i * 0x20)), *mode_vol_reg);
        }
    }
    else if (key_press == 115 || key_press == (int)'s')  // 115 S
    {
        if (volume > 0) {
            volume--;
        }
        // *mode_vol_reg = MemRead(0xD418);  // NOTE: This is totally unnescessary, D418 is not readable
        *mode_vol_reg = volume;
        for (int i = 0; i < sidcount; i++) {
            MemWrite((VOL_ADDR + (i * 0x20)), *mode_vol_reg);
        }
    }
    else if (key_press == (int)'v')
    {
        verbose = !verbose;
        if (verbose)
            cout << "VERBOSE" << endl;
        else
            cout << "NO VERBOSE" << endl;
    }
    else if (key_press == (int)'r')
    {
        load_sid(cpu, sid, *song_number);
        *min = 0;
        *sec = 0;
        *paused = false;
        printf("\rPlay Sub-Song %d / %d [%02d:%02d] @ Volume: %d            ", (*song_number) + 1, sid.GetNumOfSongs(), *min, *sec, volume);
        fflush(stdout);
    }
    else if (key_press == 257)
    { // Previous Sub-Song
        (*song_number)--;
        if (*song_number < 0)
            *song_number = sid.GetNumOfSongs() - 1;
        load_sid(cpu, sid, *song_number);
        *min = 0;
        *sec = 0;
        *paused = false;
        printf("\rPlay Sub-Song %d / %d [%02d:%02d] @ Volume: %d            ", (*song_number) + 1, sid.GetNumOfSongs(), *min, *sec, volume);
        fflush(stdout);
    }
    else if (key_press == 258)
    { // Next Sub-Song
        (*song_number)++;
        if (*song_number == sid.GetNumOfSongs())
            (*song_number) = 0;
        load_sid(cpu, sid, *song_number);
        *min = 0;
        *sec = 0;
        *paused = false;
        printf("\rPlay Sub-Song %d / %d [%02d:%02d] @ Volume: %d            ", (*song_number) + 1, sid.GetNumOfSongs(), *min, *sec, volume);
        fflush(stdout);
    }
    else if (key_press > 0)
    {
        if (*paused)
        {
            printf("\rPlay Sub-Song %d / %d [%02d:%02d] @ Volume: %d [PAUSED]    ", (*song_number) + 1, sid.GetNumOfSongs(), *min, *sec, volume);
            fflush(stdout);
        }
        else
        {
            printf("\rPlay Sub-Song %d / %d [%02d:%02d] @ Volume: %d            ", (*song_number) + 1, sid.GetNumOfSongs(), *min, *sec, volume);
            fflush(stdout);
        }
    }
}

void sidPlaySetup(void)
{
    // setup wiring library, configure GPIOs
    gpioSetup();
    #if BOARD != USBSIDPICO
    gpioMode(D0, OUTPUT);
    gpioMode(D1, OUTPUT);
    gpioMode(D2, OUTPUT);
    gpioMode(D3, OUTPUT);
    gpioMode(D4, OUTPUT);
    gpioMode(D5, OUTPUT);
    gpioMode(D6, OUTPUT);
    gpioMode(D7, OUTPUT);

    gpioMode(A0, OUTPUT);
    gpioMode(A1, OUTPUT);
    gpioMode(A2, OUTPUT);
    gpioMode(A3, OUTPUT);
    gpioMode(A4, OUTPUT);

    #if BOARD == FTDI
    gpioMode(RES, OUTPUT); // FTDI has reset pin
    gpioMode(RW, OUTPUT);  // FTDI has read/write pin
    #endif
    gpioMode(CS, OUTPUT);

    gpioWrite(D0, LOW);
    gpioWrite(D1, LOW);
    gpioWrite(D2, LOW);
    gpioWrite(D3, LOW);
    gpioWrite(D4, LOW);
    gpioWrite(D5, LOW);
    gpioWrite(D6, LOW);
    gpioWrite(D7, LOW);

    gpioWrite(A0, LOW);
    gpioWrite(A1, LOW);
    gpioWrite(A2, LOW);
    gpioWrite(A3, LOW);
    gpioWrite(A4, LOW);

    #if BOARD == FTDI
    gpioWrite(RES, HIGH); // FTDI reset pin to high to enable the board
    gpioWrite(RW, LOW);      // FTDI read/write to low to enable writing
    #endif
    if (real_read == true) gpioWrite(CS, HIGH); // Chip select high
    #endif
}

int main(int argc, char *argv[])
{
    signal(SIGINT, inthand); // use signal to check for signal interrupts and set inthand if so
    SidFile sid;

    string filename = "";
    int song_number = 0;
    calculatedclock = true;
    calculatedhz = true;

    for (int param_count = 1; param_count < argc; param_count++)
    {
        if (filename.length() == 0 and argv[param_count][0] != '-')
        {
            filename = argv[param_count];
        }
        else if (!strcmp(argv[param_count], "-v") || !strcmp(argv[param_count], "--verbose"))
        {
            verbose = true;
        }
        else if (!strcmp(argv[param_count], "-t") || !strcmp(argv[param_count], "--trace"))
        {
            trace = true;
        }
        else if (!strcmp(argv[param_count], "-cc") || !strcmp(argv[param_count], "--customclock"))
        {
            param_count++;
            custom_clock = atoi(argv[param_count]) - 1;
            printf("READ Decimal: %i\n", atoi(argv[param_count]) - 1);
        }
        else if (!strcmp(argv[param_count], "-ch") || !strcmp(argv[param_count], "--customhertz"))
        {
            param_count++;
            custom_hertz = atoi(argv[param_count]) - 1;
            printf("READ Decimal: %i\n", atoi(argv[param_count]) - 1);
        }
        else if (!strcmp(argv[param_count], "-clc") || !strcmp(argv[param_count], "--calclock"))
        {
            calculatedclock = false;
        }
        else if (!strcmp(argv[param_count], "-clh") || !strcmp(argv[param_count], "--calhertz"))
        {
            calculatedhz = false;
        }
        else if (!strcmp(argv[param_count], "-cl") || !strcmp(argv[param_count], "--calculated"))
        {
            calculatedclock = false;
            calculatedhz = false;
        }
        else if (!strcmp(argv[param_count], "-start")) // TODO: FIX AND FINISH
        {
            printf("Starting the device\n");
        #if BOARD == FTDI
            gpioInitPorts();
            enableGpio();
        #elif BOARD == USBSIDPICO
            int rc = gpioSetup();
            // printf("rc: %d\r\n", rc);
        #endif
            return 0;
        }
        else if (!strcmp(argv[param_count], "-stop")) // TODO: FIX AND FINISH
        {
            printf("Stopping the device\n");
        #if BOARD == FTDI
            gpioInitPorts();
        #endif
            gpioStop();

            return 0;
        }
        else if (!strcmp(argv[param_count], "-r") || !strcmp(argv[param_count], "--read")) // TODO: FIX AND FINISH
        {
            if (argc != 3) {
                printf("Error, usage: -r FFFF \n");
                return 1;
            }

            DBG("READ Decimal: %ld | Hex: 0x%04lx\n", strtol(argv[2], NULL, 16), strtol(argv[2], NULL, 16));

            #if BOARD == FTDI
            gpioInitPorts();     // Init the USB port
            gpioWrite(RW, HIGH); // reading requires ReadWrite to be HIGH
            #endif

            SingleRead(strtol(argv[2], NULL, 16));

            return 0;
        }
        else if (!strcmp(argv[param_count], "-w") || !strcmp(argv[param_count], "--write")) // TODO: FIX AND FINISH
        {
            if (argc != 4) {
                printf("Error, usage: -w FFFF FFFF \n");
                return 1;
            }

            DBG("WRITE Decimal 1: %ld | Hex 1: 0x%04lx | Decimal 2: %ld | Hex 2: 0x%04lx\n", strtol(argv[2], NULL, 16), strtol(argv[2], NULL, 16), strtol(argv[3], NULL, 16), strtol(argv[3], NULL, 16));

            #if BOARD == FTDI
            gpioInitPorts();    // Init the USB port
            gpioWrite(RW, LOW); // writing requires ReadWrite to be LOW
            #endif

            SingleWrite(strtol(argv[2], NULL, 16), strtol(argv[3], NULL, 16));

            return 0;
        }
        else if (!strcmp(argv[param_count], "-rr") || !strcmp(argv[param_count], "--realreads"))
        {
            real_read = true;
        }
        else if (!strcmp(argv[param_count], "-V") || !strcmp(argv[param_count], "--version"))
        {
            cout << endl;
            cout << "SidBerry 4.1 - (June - 2024)" << endl;
            cout << "MOS SID (MOS6581/8580) and clones player" << endl;
            cout << "Supports: USBSID-Pico, RaspberryPI, AriettaG25, FTDI ft2232h and other Linux-based systems with GPIO ports" << endl;
            cout << "Hardware for RaspberryPI  : Gianluca Ghettini, Thoroide " << endl;
            cout << "Hardware for AriettaG25   : Alessio Lombardo " << endl;
            cout << "Hardware for USBSID-Pico  : LouD " << endl;
            cout << "Hardware for FTDI ft2233h : LouD " << endl;
            cout << "Low-level interface       : Gianluca Ghettini, Alessio Lombardo " << endl;
            cout << "MOS6502 Emulator          : Gianluca Ghettini" << endl;
            cout << "Sid Player                : Gianluca Ghettini, Alessio Lombardo " << endl;
            cout << "Additional features       : LouD " << endl;
            cout << endl;
            return 0;
        }
        else if (!strcmp(argv[param_count], "-s") || !strcmp(argv[param_count], "--song"))
        {
            param_count++;
            song_number = atoi(argv[param_count]) - 1;
        }
        else if (!strcmp(argv[param_count], "-h") || !strcmp(argv[param_count], "--help"))
        {
            cout << endl;
            cout << "Usage: " << argv[0] << " <Sid Filename> [Options]" << endl;
            cout << "Options: " << endl;
            cout << " -s,  --song          : Set Sub-Song number (default depends on the Sid File) " << endl;
            cout << " -v,  --verbose       : Verbose mode (show SID registers content) " << endl;
            cout << " -t,  --trace         : Trace mode (prints some additional trace logging) " << endl;
            cout << " -V,  --version       : Show version and other informations " << endl;
            cout << " -h,  --help          : Show this help message " << endl;
            cout << " ------------------------------------------------------------------------------------------ " << endl;
            cout << " Experimental features: " << endl;
            cout << " -clc, --calclock     : Disable calculated play SID with the calculated clockspeed (PAL/NTSC) " << endl;
            cout << " -clh, --calhertz     : Disable calculated play SID with the calculated refreshrate (50Hz/60Hz) " << endl;
            cout << " -cl,  --calculated   : Disable calculated for both calculated clockspeed and refreshrate " << endl;
            cout << " -cc,  --customclock  : Manually define the clockspeed, requires -clc " << endl;
            cout << " -ch,  --customhertz  : Manually define the refreshrate (Hz) by ms, requires -clh " << endl;
            cout << " -rr,  --realreads    : Reads the datapins when a SID needs to (unfinished, defaults to true for USBSID-Pico) " << endl;
            cout << " ------------------------------------------------------------------------------------------ " << endl;
            cout << " Features for reading and writing single addresses (unfinished): " << endl;
            cout << " -start               : Inits the usb and ports (FTDI) for reading and writing " << endl;
            cout << " -stop                : Stops the usb and ports (FTDI) for reading and writing " << endl;
            cout << " -r,  --read          : Read a register (prints content of a register) " << endl;
            cout << " -w,  --write         : Write date to a register " << endl;
            cout << endl;
            return 0;
        }
        else
        {
            cout << "Warning: Invalid Parameter at position " << param_count << endl;
        }
    }

    int res = sid.Parse(filename);
    if (song_number < 0 or song_number >= sid.GetNumOfSongs())
    {
        cout << "Warning: Invalid Sub-Song Number. Default Sub-Song will be chosen." << endl;
        song_number = sid.GetFirstSong();
    }

    if (res == SIDFILE_ERROR_FILENOTFOUND)
    {
        cerr << "error loading sid file! not found" << endl;
        return 1;
    }

    if (res == SIDFILE_ERROR_MALFORMED)
    {
        cerr << "error loading sid file! malformed" << endl;
        return 2;
    }

    const char *chiptype[4] = {"Unknown", "MOS6581", "MOS8580", "MOS6581 and MOS8580"};
    const char *clockspeed[5] = {"Unknown", "PAL", "NTSC", "PAL and NTSC", "DREAN"};
    int sidflags = sid.GetSidFlags();
    uint32_t sidspeed = sid.GetSongSpeed(song_number); // + 1);
    int curr_sidspeed = sidspeed & (1 << song_number); // ? 1 : 0;  // 1 ~ 60Hz, 2 ~ 50Hz
    int ct = sid.GetChipType(1);
    int cs = sid.GetClockSpeed();
    int sv = sid.GetSidVersion();
    int clock_speed =
        calculatedclock == true
        ? clockSpeed[cs]
            : custom_clock > 0
            ? custom_clock
                : CLOCK_DEFAULT;
    int refresh_rate =
        calculatedhz == true
        ? refreshRate[cs]
            : custom_hertz > 0
            ? custom_hertz
                : HERTZ_DEFAULT;

    cout << "\n< Sid Info >" << endl;
    cout << "---------------------------------------------" << endl;
    cout << "SID Title          : " << sid.GetModuleName() << endl;
    cout << "Author Name        : " << sid.GetAuthorName() << endl;
    cout << "Release & (C)      : " << sid.GetCopyrightInfo() << endl;
    cout << "---------------------------------------------" << endl;
    cout << "SID Type           : " << sid.GetSidType() << endl;
    cout << "SID Format version : " << dec << sv << endl;
    cout << "---------------------------------------------" << endl;
    cout << "SID Flags          : 0x" << hex << sidflags << " 0b" << bitset<8>{sidflags} << endl;
    cout << "Chip Type          : " << chiptype[ct] << endl;
    if (sv == 3 || sv == 4)
        cout << "Chip Type 2        : " << chiptype[sid.GetChipType(2)] << endl;
    if (sv == 4)
        cout << "Chip Type 3        : " << chiptype[sid.GetChipType(3)] << endl;
    cout << "Clock Type         : " << hex << clockspeed[cs] << endl;
    cout << "Clock Speed        : " << dec << clock_speed << endl;
    cout << "Refresh Rate       : " << dec << refresh_rate << endl;
    if (sv == 3 || sv == 4 || sv == 78) {
        cout << "---------------------------------------------" << endl;
        cout << "SID 2 $addr        : $d" << hex << sid.GetSIDaddr(2) << "0" << endl;
        if (sv == 4 || sv == 78)
            cout << "SID 3 $addr        : $d" << hex << sid.GetSIDaddr(3) << "0" << endl;
        if (sv == 78)
            cout << "SID 4 $addr        : $d" << hex << sid.GetSIDaddr(4) << "0" << endl;
    }
    cout << "---------------------------------------------" << endl;
    cout << "Data Offset        : $" << setfill('0') << setw(4) << hex << sid.GetDataOffset() << endl;
    cout << "Image length       : $" << hex << sid.GetInitAddress() << " - $" << hex << (sid.GetInitAddress() - 1) + sid.GetDataLength() << endl;
    cout << "Load Address       : $" << hex << sid.GetLoadAddress() << endl;
    cout << "Init Address       : $" << hex << sid.GetInitAddress() << endl;
    cout << "Play Address       : $" << hex << sid.GetPlayAddress() << endl;
    cout << "---------------------------------------------" << endl;
    cout << "Song Speed(s)      : $" << hex << curr_sidspeed << " $0x" << hex << sidflags << " 0b" << bitset<32>{sidflags} << endl;  // BUG: Always 0x24 !?
    cout << "Selected Sub-Song  : " << dec << song_number + 1 << " / " << dec << sid.GetNumOfSongs() << endl;

    sidcount =
        sv == 3
        ? 2
            : sv == 4
            ? 3
                : sv == 78
                ? 4
                    : 1;

    sidPlaySetup();  // Setup gpios for playing SID files

    srand(0);
    mos6502 cpu(MemRead, MemWrite);

    int time_unit = 0;
    int sec = 0;
    int min = 0;

    // bool paused = true;
    bool paused = false;
    bool exit = false;
    uint8_t mode_vol_reg = volume;

    load_sid(cpu, sid, song_number);

    if (verbose)
        cout << endl;

    cout << "\n< Player Commands >" << endl;
    cout << "Space       : Pause/Continue " << endl;
    cout << "Left  Arrow : Previous Sub-Song " << endl;
    cout << "Right Arrow : Next Sub-Song " << endl;
    cout << "R           : Restart current Sub-Song " << endl;
    cout << "V           : Verbose (show SID registers) " << endl;
    cout << "W           : Volume up " << endl;
    cout << "S           : Volume down " << endl;
    cout << "Q or Escape : Quit " << endl
         << endl;

    printf("\rPlay Sub-Song %d / %d [%02d:%02d] @ Volume: %d            ", song_number + 1, sid.GetNumOfSongs(), min, sec, volume);
    fflush(stdout);


    // main loop, play song
    while (!exit || !stop)
    {

        while (paused)
        {
            change_player_status(cpu, sid, getch_noecho_special_char(), &paused, &exit, &mode_vol_reg, &song_number, &sec, &min);
            usleep(100000);
        }

        gettimeofday(&t1, NULL);

        change_player_status(cpu, sid, getch_noecho_special_char(), &paused, &exit, &mode_vol_reg, &song_number, &sec, &min);

        if (exit || stop)
            break;
        if (paused)
            continue;

        // trigger IRQ interrupt
        cpu.IRQ();

        // execute the player routine
        cpu.Run(0);

        gettimeofday(&t2, NULL);

        // wait 1/50 sec.
        if (t1.tv_sec == t2.tv_sec)
            elaps = t2.tv_usec - t1.tv_usec;
        else
            elaps = clock_speed - t1.tv_usec + t2.tv_usec;
        if (elaps < refresh_rate)
            usleep(refresh_rate - elaps); // 50Hz refresh rate is 20us
        time_unit++;
        if (time_unit % 50 == 0)
        {
            sec++;
            if (sec == 60)
            {
                sec = 0;
                min++;
            }
            if (!verbose)
            {
                printf("\rPlay Sub-Song %d / %d [%02d:%02d] @ Volume: %d            ", song_number + 1, sid.GetNumOfSongs(), min, sec, volume);
                // printf("\rPlay Sub-Song %d / %d [%02d:%02d] Memory address: $%04x", song_number + 1, sid.GetNumOfSongs(), min, sec, phyaddr);
                fflush(stdout);
            }
        }
    }

    return 0;
}
