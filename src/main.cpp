//============================================================================
// Description : SidBerry player & driver
// Authors     : Gianluca Ghettini, Alessio Lombardo, LouD
// Last update : 2024
//============================================================================

#ifdef TERMIWIN_DONOTREDEFINE
#include "winsock2.h"
#include "windows.h"
#endif

/* Serial port stuffs */
#if defined(UNIX_COMPILE)
#define termios asmtermios
#define winsize asmwinsize
#define termio asmtermio
#include <asm/termios.h>
#undef  termios
#undef  winsize
#undef  termio
#include <asm-generic/termbits.h>
#include <sys/ioctl.h> // Used for TCGETS2/TCSETS2, which is required for custom baud rates
#include <unistd.h>
#endif

#include "mos6502/mos6502.h"
#include "SidFile.h"
#include "sidberry.h"

#pragma GCC diagnostic ignored "-Wnarrowing"

uint8_t memory[65536];         // init 64K ram
int sidcount = 1;              // default to 1 sid
int sidno;
int fmoplsidno = -1;
int pcbversion = -1;

uint16_t sidone, sidtwo, sidthree, sidfour;
int sidssockone = 0, sidssocktwo = 0;
int sockonesidone = 0, sockonesidtwo = 0;
int socktwosidone = 0, socktwosidtwo = 0;
int custom_clock = 0;          // default custom clock to 0
int custom_hertz = 0;          // default custom hertz to 0
int volume = 15;               // init volume at 10. max is 15

bool verbose = false;          // init verbose boolean
bool trace = false;            // init trace boolean
bool debug = false;            // init debug boolean
bool playonesockettwo = false; // force play on socket two
bool use_cycles = false;       // add cycles to writes
bool use_asid = false;         // use ASID to write to USBSID-Pico (or other ASID supporting devices)
bool use_serial = false;       // use direct serial connection to write to USBSID-Pico
bool use_usbsid = false;       // use USB to write to USBSID-Pico

/* Serial stuffs */
#if defined(UNIX_COMPILE)
#define BAUD_RATE 921600
const char *defaultserialport = "/dev/ttyAMA5";
char USBSIDSerial[13]; /* 12 chars long (still testing) */
int us_Out, serial_port;
#endif

bool calculatedclock = false;  // init calculated clock speed boolean
bool calculatedhz = false;     // init calculated refresh boolean
volatile sig_atomic_t stop;    // init variable for ctrl+c
bool real_read = true;         // use actual pin reading when USBSID-Pico
timeval t1, t2, t3, t4, c1, c2;
long int elaps;

uint64_t cyclecount, last_write_cyclecount, last_sidwr_cyclecount;
static uint32_t frames, p_frames;

extern void list_ports(void);
extern int asid_init(char *param, int no_sids);
extern void asid_close(void);
void sendSIDEnvironment(bool isPAL);
void sendSIDType(bool is6581);
int asid_dump(unsigned short addr, unsigned char byte, int sidno);
int asid_flush(void);
#if defined(UNIX_COMPILE)
int serial_write_chars(unsigned char * data, size_t size);
#endif

void exitPlayer(void)
{
    fprintf(stdout, "\n** Exit **\n");
    for (int j = 0; j < sidcount; j++) {
        for (int i = 0xD400; i < 0xD418; i++) {
            MemWrite((i + (j * 0x20)), 0);
        }
    }
    if (use_usbsid) delete us_sid;  /* Executes us_sid->USBSID_Close(); */
    if (use_asid) asid_close();
    #if defined(UNIX_COMPILE)
    if (use_serial) {
	unsigned char buffer[4] = {0xFF,0xFF,0xFF,0xFF};
        serial_write_chars(buffer,4);
        close(serial_port);
    }
    #endif
}

void inthand(int signum)
{
    stop = 1;
    exitPlayer();
}

#if defined(UNIX_COMPILE)
void open_serialport(void)
{
    // fd = open(USBSIDSerial, /* O_RDWR */ O_WRONLY | O_NOCTTY | O_NONBLOCK);
    serial_port = open(USBSIDSerial, O_WRONLY);
    if(serial_port > 0) {
        fprintf(stdout, "USBSID opened successfully\n");
    } else {
        fprintf(stderr, "USBSID Error %i while opening: %s\n", errno, strerror(errno));
    }

    // Create new termios struct, we call it 'tty' for convention
    // No need for "= {0}" at the end as we'll immediately write the existing
    // config to this struct
    // struct termios {
    //   tcflag_t c_iflag;    /* input mode flags */
    //   tcflag_t c_oflag;    /* output mode flags */
    //   tcflag_t c_cflag;    /* control mode flags */
    //   tcflag_t c_lflag;    /* local mode flags */
    //   cc_t c_line;         /* line discipline */
    //   cc_t c_cc[NCCS];     /* control characters */
    // } tty;
    struct termios2 tty;

    // Read in existing settings, and handle any error
    // NOTE: This is important! POSIX states that the struct passed to tcsetattr()
    // must have been initialized with a call to tcgetattr() overwise behaviour
    // is undefined
    /* USING TERMIOS.H */
    // if(tcgetattr(serial_port, &tty) != 0) {
    //     printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
    // }
    /* USING IOCTL.H */
    ioctl(serial_port, TCGETS2, &tty);

    /* https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/ */
    tty.c_cflag &= ~PARENB;        // Clear parity bit, disabling parity (most common)
    tty.c_cflag &= ~CSTOPB;        // Clear stop field, only one stop bit used in communication (most common)
    tty.c_cflag &= ~CSIZE;         // Clear all the size bits, then use one of the statements below
    tty.c_cflag |= CS8;            // 8 bits per byte (most common)
    tty.c_cflag &= ~CRTSCTS;       // Disable RTS/CTS hardware flow control (most common)
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

    tty.c_lflag &= ~ICANON;        // Disable Canonical Mode
    tty.c_lflag &= ~ECHO;          // Disable echo
    tty.c_lflag &= ~ECHOE;         // Disable erasure
    tty.c_lflag &= ~ECHONL;        // Disable new-line echo
    tty.c_lflag &= ~ISIG;          // Disable interpretation of INTR, QUIT and SUSP

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);                          // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

    // Set custom baud rate
    tty.c_cflag &= ~CBAUD;
    tty.c_cflag |= CBAUDEX;
    // On the internet there is also talk of using the "BOTHER" macro here:
    // tty.c_cflag |= BOTHER;
    // I never had any luck with it, so omitting in favour of using
    // CBAUDEX
    tty.c_ispeed = BAUD_RATE; // What a custom baud rate!
    tty.c_ospeed = BAUD_RATE;

    // Write terminal settings to file descriptor
    ioctl(serial_port, TCSETS2, &tty);

    // Save tty settings, also checking for error
    /* USING TERMIOS.H */
    // if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
    //     printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
    // }
    /* USING IOCTL.H */
    ioctl(serial_port, TCSETS2, &tty);

}
#endif

#if defined(UNIX_COMPILE)
int serial_write_chars(unsigned char * data, size_t size)
{
    us_Out = write(serial_port, data, size);
    /* printf("[%d] %02X %02X %02X\n",us_Out,data[0],data[1],data[2]); */
    return us_Out;
}
#endif

uint8_t addr_translation(uint16_t addr)
{
    uint8_t sock2add = (playonesockettwo ? (sidssockone == 1 ? 0x20 : sidssockone == 2 ? 0x40 : 0x0) : 0x0);
    if (addr == 0xDF40 || addr == 0xDF50) {
        if (fmoplsidno >= 1 && fmoplsidno <=4) {
            sidno = fmoplsidno;
            return (((sidno - 1) * 0x20) + (addr & 0x1F));
        } else {
            sidno = 5; /* Skip */
            return (0x80 + (addr & 0x1F)); /* Out of scope address */
        }
    }
    switch (sidcount) {
        case 1: /* Easy just return the address */
            if (addr >= sidone && addr < (sidone + 0x1F)) {
                sidno = 1;
                return (sock2add + (addr & 0x1F));
            };
            break;
        case 2: /* Intermediate remap all above 0x20 to sidno 2 */
            if (addr >= sidone && addr < (sidone + 0x1F)) {
                sidno = 1;
                return (addr & 0x1F);
            } else if (addr >= sidtwo && addr < (sidtwo + 0x1F)) {
                sidno = 2;
                return (0x20 + (addr & 0x1F));
            };
            break;
        case 3: /* Advanced */
            if (addr >= sidone && addr < (sidone + 0x1F)) {
                sidno = 1;
                return (addr & 0x1F);
            } else if (addr >= sidtwo && addr < (sidtwo + 0x1F)) {
                sidno = 2;
                return (0x20 + (addr & 0x1F));
            } else if (addr >= sidthree && addr < (sidthree + 0x1F)) {
                sidno = 3;
                return (0x40 + (addr & 0x1F));
            };
            break;
        case 4: /* Expert */
            if (addr >= sidone && addr < (sidone + 0x1F)) {
                sidno = 1;
                return (addr & 0x1F);
            } else if (addr >= sidtwo && addr < (sidtwo + 0x1F)) {
                sidno = 2;
                return (0x20 + (addr & 0x1F));
            } else if (addr >= sidthree && addr < (sidthree + 0x1F)) {
                sidno = 3;
                return (0x40 + (addr & 0x1F));
            } else if (addr >= sidfour && addr < (sidfour + 0x1F)) {
                sidno = 4;
                return (0x60 + (addr & 0x1F));
            };
            break;
        default:
            break;
    };
    return 0xFE;
}

uint16_t last_raddr, last_waddr;
uint8_t last_byte;

void MemWrite(uint16_t addr, uint8_t byte)
{
    last_waddr = addr;
    last_byte = byte;
    gettimeofday(&c1, NULL);
    if (addr >= 0xD400 && addr <= 0xD5FF || addr >= 0xDE00 && addr <= 0xDFFF)
    {
        // gettimeofday(&v2, NULL);
        // prevval = v2.tv_usec - v1.tv_usec;
        // printf("uS since last SIDwrite = %lld\n", prevval);
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

        uint8_t phyaddr = addr_translation(addr) & 0xFF;  /* 4 SIDs max */
        unsigned char buff[3] = { 0x0, phyaddr, byte };   /* 3 Byte buffer */
        if (use_usbsid && !use_cycles) us_sid->USBSID_Write(buff, 3);
        if (use_usbsid && use_cycles) us_sid->USBSID_WriteRingCycled(phyaddr, byte, (cyclecount - last_sidwr_cyclecount));
        // if (use_usbsid && use_cycles) us_sid->USBSID_WriteRingCycled(phyaddr, byte, (c1.tv_usec - c2.tv_usec) + 6);  /* 6 cycles */
        // if (use_usbsid) us_sid->USBSID_WriteRing(phyaddr, byte);
        // if (use_usbsid) us_sid->USBSID_WriteRingCycled(phyaddr, byte, (c1.tv_usec - c2.tv_usec));
        if (use_asid) asid_dump(phyaddr, byte, sidno);
        #if defined(UNIX_COMPILE)
        if (use_serial && !use_cycles) {
            unsigned char serialbuffer[2] = {
                phyaddr, byte
            };
            serial_write_chars(serialbuffer, 2);
        }
        if (use_serial && use_cycles) {
            unsigned char serialbuffer[4] = {
                phyaddr, byte, 0x00, 0x06
            };
            serial_write_chars(serialbuffer, 4);
        }
        #endif

        if (verbose && trace)
        {
            printf("[%d][W]@%02x [D]%02x [F]%u [C]%u %u\n", sidno, phyaddr, byte, frames, (c1.tv_usec - c2.tv_usec), (cyclecount - last_write_cyclecount));
        }
        // v1 = v2;
        last_sidwr_cyclecount = cyclecount;
    }
    else
    {
        // access to memory
        memory[addr] = byte;
    }
    // gettimeofday(&c2, NULL);
    c2 = c1;
    last_write_cyclecount = cyclecount;
    return;
}

uint8_t MemRead(uint16_t addr)
{
    last_raddr = addr;
    /* printf("[R]$%04x $%02x\r\n", addr, memory[addr]); */
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
                /* USBSID code */
                uint8_t phyaddr = addr_translation(addr) & 0xFF;  /* 4 SIDs max */
                unsigned char buff[3] = { 0x1, phyaddr, 0x0 };   /* 3 Byte buffer */
                uint8_t result;
                if (use_usbsid && !use_cycles) result = us_sid->USBSID_Read(buff);  /* Cannot use reading with buffer & cycles */
                else result = 0;
                if (verbose && trace)
                {
                    fprintf(stdout, "[%d][R]@%02x [D]%02x\n", sidno, phyaddr, result);
                }
                return result;
            }
        }
    }
    return memory[addr];
}

void CycleFn(mos6502* cpu)
{
    if(!debug) return;
    printf("[C]%4u [PC]%04X [S]%02X [P]%02X [A]%02X [X]%02X [Y]%02X [W]%04X:%02X [R]%04X\n",
        cyclecount,
        cpu->GetPC(),
        cpu->GetS(),
        cpu->GetP(),
        cpu->GetA(),
        cpu->GetX(),
        cpu->GetY(),
        last_waddr, last_byte,
        last_raddr
    );
    return;
}

int load_sid(mos6502 cpu, SidFile sid, int song_number)
{
    // gettimeofday(&v1, NULL);
    for (unsigned int i = 0; i < 65536; i++)
    {
        memory[i] = 0x00; // fill with NOPs
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

    memory[0x0002] = 0x20;               // jump sub to INIT routine
    memory[0x0003] = init & 0xFF;        // lo addr
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
    cpu.RunN(CLOCK_CYCLES, cyclecount); // 100000 clockcycles
    // cpu.Run(CLOCK_CYCLES, cyclecount, cpu.CYCLE_COUNT); // 100000 clockcycles
    return 0;
}

int getch_noecho_special_char(void)
{
    #ifndef TERMIWIN_DONOTREDEFINE  /* TODO: Finish for Windows! */
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
    #else
    return 0;
    #endif
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
            if (use_usbsid) us_sid->USBSID_UnMute();
            *paused = false;
        }
        else
        {
            printf("\rPlay Sub-Song %d / %d [%02d:%02d] @ Volume: %d [PAUSED]   ", (*song_number) + 1, sid.GetNumOfSongs(), *min, *sec, volume);
            fflush(stdout);
            for (int i = 0; i < sidcount; i++) {
                MemWrite((VOL_ADDR + (i * 0x20)), 0);
            }
            if (use_usbsid) us_sid->USBSID_Mute();
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
        *mode_vol_reg = volume;
        for (int i = 0; i < sidcount; i++) {
            MemWrite((VOL_ADDR + (i * 0x20)), *mode_vol_reg);
        }
    }
    else if (key_press == 91 || key_press == (int)'a')  // 91 A
    {
        if (use_usbsid) {
            if (pcbversion == 13) {
                if (*paused) {
                    us_sid->USBSID_ToggleStereo();
                } else {
                    fprintf(stdout, "PRESS PAUSE FIRST!\n");
                }
            }
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

void USBSIDSetup(void)
{
    USBSID_NS::USBSID_Class* us_sid = new USBSID_NS::USBSID_Class();
    if (use_usbsid && !use_cycles) {
        printf("Opening USBSID-Pico\n");
        if (us_sid->USBSID_Init(false, false) < 0) {
            printf("USBSID-Pico not found, exiting...\n");
            exit(1);
        }
    }
    if (use_usbsid && use_cycles) {
        printf("Opening USBSID-Pico with buffer for cycle exact writing\n");
        if (us_sid->USBSID_Init(true, true) < 0) {
            printf("USBSID-Pico not found, exiting...\n");
            exit(1);
        }
    }
    /* Sleep 400ms for Reset to settle */
    struct timespec tv = { .tv_sec = 0, .tv_nsec = (400 * 1000 * 1000) };
    nanosleep(&tv, &tv);
    #ifndef NO_REBOOT
    us_sid->USBSID_Mute();
    us_sid->USBSID_ClearBus();
    us_sid->USBSID_UnMute();
    #endif
}

char * midi_port;
int main(int argc, char *argv[])
{
    signal(SIGINT, inthand); // use signal to check for signal interrupts and set inthand if so
    SidFile sid;

    string filename = "";
    int song_number = 0;
    calculatedclock = true;
    calculatedhz = true;
    use_usbsid = true;
    use_cycles = false;

    for (int param_count = 1; param_count < argc; param_count++)
    {
        if (filename.length() == 0 and argv[param_count][0] != '-')
        {
            filename = argv[param_count];
        }
        else if (!strcmp(argv[param_count], "-midi") || !strcmp(argv[param_count], "--list-midi"))
        {
            list_ports();
            return 0;
        }
        #if defined(UNIX_COMPILE)
        else if (!strcmp(argv[param_count], "-serial") || !strcmp(argv[param_count], "--use-serial"))
        {
            use_asid = false;
            use_serial = true;
            use_usbsid = false;
            //use_cycles = true;
            param_count++;
            if((argc - 1) >= param_count) {
                if(strstr(argv[param_count], "/dev/") != NULL) {
                    strcpy(USBSIDSerial, argv[param_count]);
                } else {
                    param_count--;
                    strcpy(USBSIDSerial, defaultserialport);
                }
            } else {
                strcpy(USBSIDSerial, defaultserialport);
            }
            fprintf(stdout, "Using serial port: %s\n", USBSIDSerial);
        }
        #endif
        else if (!strcmp(argv[param_count], "-asid") || !strcmp(argv[param_count], "--use-asid"))
        {
            use_asid = true;
            use_serial = false;
            use_usbsid = false;
            use_cycles = false;
            param_count++;
            midi_port = argv[param_count];
        }
        else if (!strcmp(argv[param_count], "-c") || !strcmp(argv[param_count], "--use-cycles"))
        {
            //use_asid = false;  /* No cycles with ASID */
            //use_usbsid = true; /* Must be true */
            use_cycles = true;
            printf("Cycled buffer enabled\n");
        }
        else if (!strcmp(argv[param_count], "-sock2") || !strcmp(argv[param_count], "--socket-two"))
        {   /* NOTE: ONLY WORKS IF THERE IS ACTUALLY A SID IN SOCKET 2 AND WITH SINGLE SID TUNES ONLY */
            playonesockettwo = true;
        }
        else if (!strcmp(argv[param_count], "-v") || !strcmp(argv[param_count], "--verbose"))
        {
            verbose = true;
        }
        else if (!strcmp(argv[param_count], "-t") || !strcmp(argv[param_count], "--trace"))
        {
            trace = true;
        }
        else if (!strcmp(argv[param_count], "-d") || !strcmp(argv[param_count], "--debug"))
        {
            debug = true;
        }
        else if (!strcmp(argv[param_count], "-cc") || !strcmp(argv[param_count], "--customclock"))
        {
            param_count++;
            calculatedclock = false;
            custom_clock = atoi(argv[param_count]) - 1;
            printf("READ Decimal: %i\n", atoi(argv[param_count]) - 1);
        }
        else if (!strcmp(argv[param_count], "-ch") || !strcmp(argv[param_count], "--customhertz"))
        {
            param_count++;
            calculatedhz = false;
            custom_hertz = atoi(argv[param_count]) - 1;
            printf("READ Decimal: %i\n", atoi(argv[param_count]) - 1);
        }
        else if (!strcmp(argv[param_count], "-rr") || !strcmp(argv[param_count], "--realreads"))
        {
            real_read = true;
        }
        else if (!strcmp(argv[param_count], "-V") || !strcmp(argv[param_count], "--version"))
        {
            cout << endl;
            cout << "USBSIDBerry" << endl;
            cout << "MOS SID (MOS6581/8580) and clones player for USBSID-Pico" << endl;
            cout << "Hardware for USBSID-Pico  : LouD " << endl;
            cout << "Low-level interface       : Gianluca Ghettini, Alessio Lombardo " << endl;
            cout << "MOS6502 Emulator          : Gianluca Ghettini" << endl;
            cout << "Sid Player                : Gianluca Ghettini, Alessio Lombardo " << endl;
            cout << "Adaption for USBSID-Pico  : LouD " << endl;
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
            cout << " -cc,  --customclock  : Manually define the clockspeed " << endl;
            cout << " -ch,  --customhertz  : Manually define the refreshrate (Hz) by ms " << endl;
            cout << " -rr,  --realreads    : Reads the datapins when a SID needs to (unfinished, defaults to true for USBSID-Pico) " << endl;
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

    int sidflags = sid.GetSidFlags();
    uint32_t sidspeed = sid.GetSongSpeed(song_number); // + 1);
    int curr_sidspeed = sidspeed & (1 << song_number); // ? 1 : 0;  // 1 ~ 60Hz, 2 ~ 50Hz
    int ct = sid.GetChipType(1);
    int cs = sid.GetClockSpeed();
    int sv = sid.GetSidVersion();
    // 2 2 3 ~ Genius
    // 2 1 3 ~ Jump
    // printf("%d %d %d\n", ct, cs, sv);
    int clock_speed =
        calculatedclock == true
        ? clockSpeed[cs]
            : custom_clock > 0
            ? custom_clock
                : CLOCK_DEFAULT;
    int raster_lines = scanLines[cs];
    int rasterrow_cycles = scanlinesCycles[cs];
    int frame_cycles = raster_lines * rasterrow_cycles;
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
    cout << "Raster Lines       : " << dec << raster_lines << endl;
    cout << "Rasterrow Cycles   : " << dec << rasterrow_cycles << endl;
    cout << "Frame Cycles       : " << dec << frame_cycles << endl;
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
    cout << "Song Speed(s)      : $" << hex << curr_sidspeed << " $0x" << hex << sidspeed << " 0b" << bitset<32>{sidspeed} << endl;
    cout << "Timer              : " << (curr_sidspeed == 1 ? "CIA1" : "Clock") << endl;
    cout << "Selected Sub-Song  : " << dec << song_number + 1 << " / " << dec << sid.GetNumOfSongs() << endl;

    sidcount =
        sv == 3
        ? 2
            : sv == 4
            ? 3
                : sv == 78
                ? 4
                    : 1;
    sidno = 0;
    sidone = 0xD400;
    printf("SIDS: [1]$%04X ", sidone);
    if (sv == 3 || sv == 4 || sv == 78) {
        sidtwo = 0xD000 | (sid.GetSIDaddr(2) << 4);
        printf("[2]$%04X ", sidtwo);
        if (sv == 4 || sv == 78) {
            sidthree = 0xD000 | (sid.GetSIDaddr(3) << 4);
            // sidthree = (sidthree == sidone ? 0xD440 : sidthree);
            printf("[3]$%04X ", sidthree);
        }
        if (sv == 78) {
            sidfour = 0xD000 | (sid.GetSIDaddr(4) << 4);
            // sidfour = (sidfour == sidthree ? 0xD460 : sidfour);
            printf("[4]$%04X ", sidfour);
        }
    }
    printf("\n");

    if (use_usbsid) {
        USBSIDSetup();  /* Setup for playing SID files */

        if(us_sid->USBSID_GetClockRate() != clock_speed) {
            us_sid->USBSID_SetClockRate(clock_speed, true);
        }

        if(us_sid->USBSID_GetNumSIDs() < sidcount) {
            printf("[WARNING] Tune no.sids %d is higher then USBSID-Pico no.sids %d\n", sidcount, us_sid->USBSID_GetNumSIDs());
        }

        uint8_t socket_config[10];
        us_sid->USBSID_GetSocketConfig(socket_config);
        printf("SOCKET CONFIG: ");
        for (int i = 0; i < 10; i++) {
            printf("%02X ", socket_config[i]);
        }
        printf("\n");

        printf("SOCK1#.%d SID1:%d SID2:%d\nSOCK2#.%d SID1:%d SID2:%d\n",
            us_sid->USBSID_GetSocketNumSIDS(1, socket_config),
            us_sid->USBSID_GetSocketSIDType1(1, socket_config),
            us_sid->USBSID_GetSocketSIDType2(1, socket_config),
            us_sid->USBSID_GetSocketNumSIDS(2, socket_config),
            us_sid->USBSID_GetSocketSIDType1(2, socket_config),
            us_sid->USBSID_GetSocketSIDType2(2, socket_config)
        );

        sidssockone = us_sid->USBSID_GetSocketNumSIDS(1, socket_config);
        sidssocktwo = us_sid->USBSID_GetSocketNumSIDS(2, socket_config);
        sockonesidone = us_sid->USBSID_GetSocketSIDType1(1, socket_config);
        sockonesidtwo = us_sid->USBSID_GetSocketSIDType2(1, socket_config);
        socktwosidone = us_sid->USBSID_GetSocketSIDType1(2, socket_config);
        socktwosidtwo = us_sid->USBSID_GetSocketSIDType2(2, socket_config);
        fmoplsidno = us_sid->USBSID_GetFMOplSID();
        pcbversion = us_sid->USBSID_GetPCBVersion();
    }

    if (use_asid) {
        asid_init(midi_port, sidcount);
        sendSIDEnvironment((cs == 1));
        sendSIDType((ct == 1));
    }

    #if defined(UNIX_COMPILE)
    if (use_serial) {
        open_serialport();
	    uint8_t packet_size = (use_cycles ? 0x04 : 0x02);
        uint8_t initpacket[8] = {
	        0xFF,0xEE,0xDD,
            0x00, // packet size high byte
            packet_size, // packet size low byte
	        0xDD,0xEE,0xFF
	    };
	    serial_write_chars(initpacket,8);
    }
    #endif

    srand(0);
    mos6502 cpu(MemRead, MemWrite, CycleFn);

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
    if (pcbversion == 13) {
        cout << "A           : Toggle mono/stereo (Works during pause only!)" << endl;
    }
    cout << "Q or Escape : Quit " << endl
         << endl;

    printf("\rPlay Sub-Song %d / %d [%02d:%02d] @ Volume: %d            ", song_number + 1, sid.GetNumOfSongs(), min, sec, volume);
    fflush(stdout);

    int play_rate = 0;
    if (curr_sidspeed == 1) {
        play_rate = (memory[CIA_TIMER_HI] << 8 | memory[CIA_TIMER_LO]);  /* CIA timing */
        play_rate = play_rate == 0 ? refresh_rate : play_rate;
    } else {
        play_rate = refresh_rate;
    }
    // printf("\n%d %d %d\n", play_rate, memory[0xDC04] + memory[0xDC05] * 256, memory[0xDC05] << 8 | memory[0xDC04]);
    // printf("\n%d %d %d\n", play_rate, memory[0xDC06] + memory[0xDC07] * 256, memory[0xDC06] << 8 | memory[0xDC07]);
    gettimeofday(&c1, NULL);
    gettimeofday(&c2, NULL);
    while (!exit || !stop)
    {

        while (paused)
        {
            change_player_status(cpu, sid, getch_noecho_special_char(), &paused, &exit, &mode_vol_reg, &song_number, &sec, &min);
            std::this_thread::sleep_for(std::chrono::microseconds(100000));
        }

        gettimeofday(&t1, NULL);

        change_player_status(cpu, sid, getch_noecho_special_char(), &paused, &exit, &mode_vol_reg, &song_number, &sec, &min);

        if (exit || stop) {
            // if (use_asid) asid_flush();
            break;
        }
        if (paused) {
            // if (use_asid) asid_flush();
            continue;
        }
        // trigger IRQ interrupt
        cpu.IRQ();

        // execute the player routine
        cpu.RunN(0, cyclecount);
        // cpu.Run(1, cyclecount, cpu.CYCLE_COUNT); // 100000 clockcycles

        gettimeofday(&t2, NULL);

        // wait 1/50 sec.
        if (t1.tv_sec == t2.tv_sec)
            elaps = t2.tv_usec - t1.tv_usec;
        else
            elaps = clock_speed - t1.tv_usec + t2.tv_usec;
        if (elaps < play_rate /* frame_cycles */) {
            std::this_thread::sleep_for(std::chrono::microseconds(play_rate - elaps));
            // std::this_thread::sleep_for(std::chrono::microseconds(cyclecount - last_write_cyclecount));
        }
        // p_frames = frames;
        // frames = (cyclecount / play_rate);
        if (use_asid) {
            asid_flush();
            // if ((frames - p_frames) >= 1) asid_flush();
        }
        if (use_cycles) {
            us_sid->USBSID_SetFlush();
        }
        gettimeofday(&t3, NULL);
        if (t3.tv_sec != t4.tv_sec)
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
                fflush(stdout);
            }
        }
        gettimeofday(&t4, NULL);
    }

    return 0;
}
