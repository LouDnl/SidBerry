
/* #define MIDI_DEBUG */
#if defined(MIDI_DEBUG)
#define __RTMIDI_DEBUG__
// #define RTMIDI_DEBUG
#endif

#ifndef UNIX_COMPILE
#define UNIX_COMPILE
#endif

#if defined(UNIX_COMPILE) /* && defined(USE_ALSA) */
#define __LINUX_ALSA__
// #define __UNIX_JACK__
#endif
#if defined(WINDOWS_COMPILE)
#define __WINDOWS_MM__
#endif


#if defined(UNIX_COMPILE) && defined(__LINUX_ALSA__) || defined(WINDOWS_COMPILE) && defined(__WINDOWS_MM__)
// extern "C" {

#include <stdio.h>
#include "RtMidi.h"

/* https://www.music.mcgill.ca/~gary/rtmidi/ */

#define ASID_MAXSID 4

int regmap[] = {0,1,2,3,5,6,7,8,9,10,12,13,14,15,16,17,19,20,21,22,23,24,4,11,18,25,26,27};
RtMidiOut *midiout;
// static unsigned char sid_register[(ASID_MAXSID * 0x20)];  // static for auto zero
// static unsigned char sid_modified[(ASID_MAXSID * 0x20)];  // static for auto zero
struct asid_chip {
  // asid_chip() : message(64) {}
  unsigned char sid_register[28];
  unsigned char sid_modified[28];
  // std::vector<unsigned char> *message; //vector<int *> *vec = new vector<int *>();
  // vector<unsigned char*> *message = new vector<unsigned char*>(); //vector<int *> *vec = new vector<int *>();
};
// struct asid_chip sid_chip[ASID_MAXSID] = {};
static struct asid_chip sid_chip[4];

int nosids = 1;
const unsigned char sidaddr[4] = {0x4E, 0x50, 0x51, 0x52};

std::vector<unsigned char> message;
std::vector<unsigned char> basic_message;

#define EMULATION_CYCLES_PER_SECOND_DFLT  1000000
#define EMULATION_CYCLES_PER_SECOND_PAL   985248
#define EMULATION_CYCLES_PER_SECOND_NTSC  1022727

// static FILE *dump_fd = NULL;

void list_ports(void)
{
    RtMidiOut *l_midiout = new RtMidiOut();
    unsigned int nPorts = l_midiout->getPortCount();
    printf( "[ASID] Available ports: %u\n", nPorts);
    std::string portName;
    for(int i = 0; i < nPorts; i++) {
        portName = l_midiout->getPortName(i);
        printf("[ASID] Port %d : %s\n", i, portName.c_str());
    }
    delete midiout;
}

int asid_init(char *param, int no_sids)
{
    nosids = no_sids;
    printf("[ASID DBG][param]%s[no_sids]%d\n", param, nosids);
    /* No stereo capability. */
    int channels = 1;
    int i, nports, asidport = 0;
    printf("[ASID] open: %s\n", param);

    asidport = atoi(param);
    midiout = new RtMidiOut();
    nports = midiout->getPortCount();
    printf("[ASID] Available ports:\n");
    for(i = 0; i < nports; i++) {
        printf("[ASID] Port %d : %s\n", i, midiout->getPortName(i).c_str());
    }

    if (asidport >= nports) {
        printf("[ASID] Requested port: %d not available\n", asidport);
        asidport = 0;
    }

    printf("[ASID] Using port: %d: %s\n", asidport, midiout->getPortName(asidport).c_str());

    midiout->openPort(asidport);

    // disabled
    // for (i = 0; i < (ASID_MAXSID * 0x20); i++) {  /* Set 0 here!? */
    //     sid_register[i] = 0;
    //     sid_modified[i] = 0;
    // }

    basic_message.clear();
    basic_message.push_back(0xf0);
    basic_message.push_back(0x2d);
    basic_message.push_back(0x4c);
    basic_message.push_back(0xf7);
    midiout->sendMessage(&basic_message); //start sid play mode

    // sid_chip[0].message.reserve(64);
    // sid_chip[1].message.reserve(64);
    // sid_chip[2].message.reserve(64);
    // sid_chip[3].message.reserve(64);
    // sid_chip[0].message.capacity(64);
    // sid_chip[1].message.capacity(64);
    // sid_chip[2].message.capacity(64);
    // sid_chip[3].message.capacity(64);

    return 0;
}

void sendSIDEnvironment(bool isPAL)
{
  // Physical out buffer, including protocol overhead
  unsigned char ASidOutBuffer[8];
  int index = 0;

  // No UI exist to request buffering or change speed multiplier
  const bool isBufferingRequested = false;
  const int speedMultiplier = nosids; // 1x

  // Time between two frames
  const int frameDeltaUs = isPAL ? (long)1000000*63*312/EMULATION_CYCLES_PER_SECOND_PAL : (long)1000000*65*263/EMULATION_CYCLES_PER_SECOND_NTSC;

  // Sysex start data for an ASID message
  ASidOutBuffer[index++] = 0xf0;
  ASidOutBuffer[index++] = 0x2d;
  ASidOutBuffer[index++] = 0x31; // SID environment

  // Payload

  /* data0: settings
    bit0    : 0 = PAL, 1 = NTSC
    bits4-1 : speed, 1x to 16x (value 0-15)
    bit5    : 1 = custom speed (only framedelta valid)
    bit6    : 1 = buffering requested by user
    */
  ASidOutBuffer[index++] =
    ((isBufferingRequested ? 1:0)   << 6) |
    ((speedMultiplier & 0x0f)       << 1) |
    ((isPAL? 0:1)                   << 0);

  /* data1: framedelta uS, total 7+7+2=16 bits, slowest time = 65535us = 15Hz
    bits6-0: framedelta uS (LSB)

      data2:
    bits6-0: framedelta uS

      data3:
    bits1-0: framedelta uS (MSB)
    bits6-2: 5 bits (reserved)
  */
  ASidOutBuffer[index++] = (frameDeltaUs >> 7*0) & 0x7f;
  ASidOutBuffer[index++] = (frameDeltaUs >> 7*1) & 0x7f;
  ASidOutBuffer[index++] = (frameDeltaUs >> 7*2) & 0x03;

  // Sysex end marker
  ASidOutBuffer[index++] = 0xf7;

  // Send to physical MIDI port
  if (midiout->isPortOpen())
    midiout->sendMessage(ASidOutBuffer, index);
}

void sendSIDType(bool is6581)
{
  // Physical out buffer, including protocol overhead
  unsigned char ASidOutBuffer[6];
  int index = 0;

  // Sysex start data for an ASID message
  ASidOutBuffer[index++] = 0xf0;
  ASidOutBuffer[index++] = 0x2d;
  ASidOutBuffer[index++] = 0x32; // SID type

  // Payload
  ASidOutBuffer[index++] = 0; // Chip index, only one chip
  ASidOutBuffer[index++] = is6581? 0x00 : 0x01; // bits 7-1 reserved

  // Sysex end marker
  ASidOutBuffer[index++] = 0xf7;

  // Send to physical MIDI port
  if (midiout->isPortOpen())
    midiout->sendMessage(ASidOutBuffer, index);
}

int asid_flush(void);

int asid_dump(unsigned short addr, unsigned char byte, int sidno)
{
  // static CLOCK maincpu_clk_prev;
  int reg,data;
  sidno-=1;

  reg=addr & 0x1f;
  data=byte;
  // fprintf(stdout, "[%d][W]@%02x [D]%02x\n", sidno, reg, data);
  if(sid_chip[sidno].sid_modified[reg]==0)
  {
    sid_chip[sidno].sid_register[reg]=data & 0xff;
    sid_chip[sidno].sid_modified[reg]++;
    // fprintf(stdout, "[A%d][W]@%02x [D]%02x\n", sidno, reg, data);
  }
  else
  {
    switch(reg)
    {
      case 0x04:
        // if already written to secondary,move back to original one
        if(sid_chip[sidno].sid_modified[0x19]!=0) sid_chip[sidno].sid_register[0x04]=sid_chip[sidno].sid_register[0x19];
        sid_chip[sidno].sid_register[0x19]=data & 0xff;
        sid_chip[sidno].sid_modified[0x19]++;
        break;
      case 0x0b:
        // if already written to secondary,move back to original one
        if(sid_chip[sidno].sid_modified[0x1a]!=0) sid_chip[sidno].sid_register[0x0b]=sid_chip[sidno].sid_register[0x1a];
        sid_chip[sidno].sid_register[0x1a]=data & 0xff;
        sid_chip[sidno].sid_modified[0x1a]++;
        break;
      case 0x12:
        // if already written to secondary,move back to original one
        if(sid_chip[sidno].sid_modified[0x1b]!=0) sid_chip[sidno].sid_register[0x12]=sid_chip[sidno].sid_register[0x1b];
        sid_chip[sidno].sid_register[0x1b]=data & 0xff;
        sid_chip[sidno].sid_modified[0x1b]++;
        break;
      case 0x16 ... 0x18:
        // If we're trying to update a control register that is already mapped, flush it directly
        asid_flush();

      // default:
      //     sid_chip[sidno].sid_register[reg]=data & 0xff;
      //     sid_chip[sidno].sid_modified[reg]++;
    }
  }
  return 0;
}


// int asid_flush(char *state)
int asid_flush(void)
{
  for (int sid = 0; sid < nosids; sid++) {
    // static CLOCK maincpu_clk_prev;
    int i,j;
    // unsigned int r=0;
    unsigned int mask=0;
    unsigned int msb=0;
    // size_t cap = sid_chip[sid].message.capacity();
    message.clear();
    // sid_chip[sid].message.reserve(256);
    // sid_chip[sid].message.empty();
    message.push_back(0xf0);
    message.push_back(0x2d);
    message.push_back(sidaddr[sid]);
    // set bits in mask for each register that has been written to
    // write last bit of each register into msb
    for(i=0;i<28;i++)
    {
        j=regmap[i];
        // fprintf(stdout, "[%d] %02X [W]@%02x [D]%02x\n", sid, message[2], j, sid_chip[sid].sid_register[j]);
        if(sid_chip[sid].sid_modified[j]!=0)
        {
            mask=mask | (1<<i);
        }
        if(sid_chip[sid].sid_register[j]>0x7f)
        {
            msb=msb | (1<<i);
        }
    }
    message.push_back(mask & 0x7f);
    message.push_back((mask>>7)&0x7f);
    message.push_back((mask>>14)&0x7f);
    message.push_back((mask>>21)&0x7f);
    message.push_back(msb & 0x7f);
    message.push_back((msb>>7)&0x7f);
    message.push_back((msb>>14)&0x7f);
    message.push_back((msb>>21)&0x7f);
    for(i=0;i<28;i++)
    {
        j=regmap[i];
        if(sid_chip[sid].sid_modified[j]!=0)
        {
            message.push_back(sid_chip[sid].sid_register[j]&0x7f);
        }
    }
    message.push_back(0xf7);
    midiout->sendMessage(&message);
    for(i=0;i<28;i++)
    {
        sid_chip[sid].sid_modified[i]=0;
    }
  }
  return 0;

}

void asid_close(void)
{
    //printf("asidclose!\n");

    basic_message.clear();
    basic_message.push_back(0xf0);
    basic_message.push_back(0x2d);
    basic_message.push_back(0x4d);
    basic_message.push_back(0xf7);
    midiout->sendMessage(&basic_message);
    delete midiout;

}

// static sound_device_t asid_device =
// {
//     "asid\n",
//     asid_init,
//     asid_write,
//     asid_dump,
//     asid_flush,
//     NULL,
//     asid_close,
//     NULL,
//     NULL,
//     0
// };

// int sound_init_asid_device(void)
// {
//     return sound_register_device(&asid_device);
// }

// } // extern "C"

#endif /* UNIX_COMPILE && __LINUX_ALSA__|| WINDOWS_COMPILE && __WINDOWS_MM__ */
