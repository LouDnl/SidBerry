//============================================================================
// Description : SidBerry player & driver
// Authors     : Gianluca Ghettini, Alessio Lombardo, LouD
// Last update : 2024
//============================================================================

#ifdef TERMIWIN_DONOTREDEFINE
#include "winsock2.h"
#include "windows.h"
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
int custom_clock = 0;          // default custom clock to 0
int custom_hertz = 0;          // default custom hertz to 0
int volume = 15;               // init volume at 10. max is 15

bool verbose = false;          // init verbose boolean
bool trace = false;            // init trace boolean

bool calculatedclock = false;  // init calculated clock speed boolean
bool calculatedhz = false;     // init calculated refresh boolean
volatile sig_atomic_t stop;    // init variable for ctrl+c
bool real_read = true;         // use actual pin reading when USBSID-Pico
timeval t1, t2;
long int elaps;

uint64_t cyclecount;
static uint32_t frames;


void exitPlayer(void)
{
    fprintf(stdout, "\n** Exit **\n");
    for (int j = 0; j < sidcount; j++) {
        for (int i = 0xD400; i < 0xD418; i++) {
            MemWrite((i + (j * 0x20)), 0);
        }
    }
    delete us_sid;  /* Executes us_sid->USBSID_Close(); */
}

void inthand(int signum)
{
    stop = 1;
    exitPlayer();
}

uint8_t addr_translation(uint16_t addr)
{
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
            if (addr >= sidone && addr < (sidone + 0x20)) {
                sidno = 1;
                return (addr & 0x1F);
            };
            break;
        case 2: /* Intermediate remap all above 0x20 to sidno 2 */
            if (addr >= sidone && addr < (sidone + 0x20)) {
                sidno = 1;
                return (addr & 0x1F);
            } else if (addr >= sidtwo && addr < (sidtwo + 0x20)) {
                sidno = 2;
                return (0x20 + (addr & 0x1F));
            };
            break;
        case 3: /* Advanced */
            if (addr >= sidone && addr < (sidone + 0x20)) {
                sidno = 1;
                return (addr & 0x1F);
            } else if (addr >= sidtwo && addr < (sidtwo + 0x20)) {
                sidno = 2;
                return (0x20 + (addr & 0x1F));
            } else if (addr >= sidthree && addr < (sidthree + 0x20)) {
                sidno = 3;
                return (0x40 + (addr & 0x1F));
            };
            break;
        case 4: /* Expert */
            if (addr >= sidone && addr < (sidone + 0x20)) {
                sidno = 1;
                return (addr & 0x1F);
            } else if (addr >= sidtwo && addr < (sidtwo + 0x20)) {
                sidno = 2;
                return (0x20 + (addr & 0x1F));
            } else if (addr >= sidthree && addr < (sidthree + 0x20)) {
                sidno = 3;
                return (0x40 + (addr & 0x1F));
            } else if (addr >= sidfour && addr < (sidfour + 0x20)) {
                sidno = 4;
                return (0x60 + (addr & 0x1F));
            };
            break;
        default:
            break;
    };
    return 0xFE;
}

void MemWrite(uint16_t addr, uint8_t byte)  // NOTICE: USB devices only
{
    if (addr >= 0xD400 && addr <= 0xD5FF || addr >= 0xDE00 && addr <= 0xDFFF)
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

        uint8_t phyaddr = addr_translation(addr) & 0xFF;  /* 4 SIDs max */
        unsigned char buff[3] = { 0x0, phyaddr, byte };   /* 3 Byte buffer */
        us_sid->USBSID_Write(buff, 3);

        if (verbose && trace)
        {
            fprintf(stdout, "[%d][W]@%02x [D]%02x\n", sidno, phyaddr, byte);
        }
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
                uint8_t result = us_sid->USBSID_Read(buff);
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

int load_sid(mos6502 cpu, SidFile sid, int song_number)
{
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
    cpu.RunN(CLOCK_CYCLES, cyclecount); // 100000 clockcycles
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
            us_sid->USBSID_UnMute();
            *paused = false;
        }
        else
        {
            printf("\rPlay Sub-Song %d / %d [%02d:%02d] @ Volume: %d [PAUSED]   ", (*song_number) + 1, sid.GetNumOfSongs(), *min, *sec, volume);
            fflush(stdout);
            for (int i = 0; i < sidcount; i++) {
                MemWrite((VOL_ADDR + (i * 0x20)), 0);
            }
            us_sid->USBSID_Mute();
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
        if (pcbversion == 13) {
            us_sid->USBSID_ToggleStereo();
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
    if (us_sid->USBSID_Init(false, false) < 0) {
        printf("USBSID-Pico not found, exiting...");
        exit(1);
    }
    /* Sleep 400ms for Reset to settle */
    struct timespec tv = { .tv_sec = 0, .tv_nsec = (400 * 1000 * 1000) };
    nanosleep(&tv, &tv);
    #ifndef NO_REBOOT
    us_sid->USBSID_Mute();
    us_sid->USBSID_ClearBus();
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
    sidno = 0;
    sidone = 0xD400;
    /* printf("[1]$%04X ", sidone); */
    if (sv == 3 || sv == 4 || sv == 78) {
        sidtwo = 0xD000 | (sid.GetSIDaddr(2) << 4);
        /* printf("[2]$%04X ", sidtwo); */
        if (sv == 4 || sv == 78) {
            sidthree = 0xD000 | (sid.GetSIDaddr(3) << 4);
            /* printf("[3]$%04X ", sidthree); */
        }
        if (sv == 78) {
            sidfour = 0xD000 | (sid.GetSIDaddr(4) << 4);
            /* printf("[4]$%04X ", sidfour); */
        }
    }
    printf("\n");

    USBSIDSetup();  /* Setup for playing SID files */

    if(us_sid->USBSID_GetClockRate() != clock_speed) {
        us_sid->USBSID_SetClockRate(clock_speed, true);
    }

    if(us_sid->USBSID_GetNumSIDs() < sidcount) {
        printf("[WARNING] Tune no.sids %d is higher then USBSID-Pico no.sids %d\n", sidcount, us_sid->USBSID_GetNumSIDs());
    }

    fmoplsidno = us_sid->USBSID_GetFMOplSID();
    pcbversion = us_sid->USBSID_GetPCBVersion();

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
    if (pcbversion == 13) {
        cout << "A           : Toggle mono/stereo " << endl;
    }
    cout << "Q or Escape : Quit " << endl
         << endl;

    printf("\rPlay Sub-Song %d / %d [%02d:%02d] @ Volume: %d            ", song_number + 1, sid.GetNumOfSongs(), min, sec, volume);
    fflush(stdout);
    while (!exit || !stop)
    {

        while (paused)
        {
            change_player_status(cpu, sid, getch_noecho_special_char(), &paused, &exit, &mode_vol_reg, &song_number, &sec, &min);
            /* usleep(100000); */
            std::this_thread::sleep_for(std::chrono::microseconds(100000));
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
        cpu.RunN(0, cyclecount);

        gettimeofday(&t2, NULL);

        // wait 1/50 sec.
        if (t1.tv_sec == t2.tv_sec)
            elaps = t2.tv_usec - t1.tv_usec;
        else
            elaps = clock_speed - t1.tv_usec + t2.tv_usec;
        if (elaps < refresh_rate /* frame_cycles */) {
            // usleep(refresh_rate /* frame_cycles */ - elaps); // 50Hz refresh rate is 20us
            std::this_thread::sleep_for(std::chrono::microseconds(refresh_rate - elaps));
        }
        time_unit++;
        frames = (cyclecount / refresh_rate);
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
                fflush(stdout);
            }
        }
    }

    return 0;
}
