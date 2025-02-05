//============================================================================
// Description : SID file parser
// Authors     : Gianluca Ghettini, LouD
// Last update : 2024
//============================================================================

#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
using namespace std;

#define PSID_MIN_HEADER_LENGTH 118 // Version 1
#define PSID_MAX_HEADER_LENGTH 125 // Version 2 (124), 3 & 4

// Offsets of fields in header (all fields big-endian)
enum
{
    SIDFILE_PSID_ID = 0,          // 'PSID'
    SIDFILE_PSID_VERSION_H = 4,   // 1, 2, 3 or 4
    SIDFILE_PSID_VERSION_L = 5,   // 1, 2, 3 or 4
    SIDFILE_PSID_LENGTH_H = 6,    // Header length
    SIDFILE_PSID_LENGTH_L = 7,    // Header length
    SIDFILE_PSID_START_H = 8,     // C64 load address
    SIDFILE_PSID_START_L = 9,     // C64 load address
    SIDFILE_PSID_INIT_H = 10,     // C64 init routine address
    SIDFILE_PSID_INIT_L = 11,     // C64 init routine address
    SIDFILE_PSID_MAIN_H = 12,     // C64 replay routine address
    SIDFILE_PSID_MAIN_L = 13,     // C64 replay routine address
    SIDFILE_PSID_NUMBER_H = 14,   // Number of subsongs
    SIDFILE_PSID_NUMBER_L = 15,   // Number of subsongs
    SIDFILE_PSID_DEFSONG_H = 16,  // Main subsong number
    SIDFILE_PSID_DEFSONG_L = 17,  // Main subsong number
    SIDFILE_PSID_SPEED = 18,      // Speed flags (1 bit/song)
    SIDFILE_PSID_NAME = 22,       // Module name (ISO Latin1 character set)
    SIDFILE_PSID_AUTHOR = 54,     // Author name (dto.)
    SIDFILE_PSID_COPYRIGHT = 86,  // Release year and Copyright info (dto.)

    SIDFILE_PSID_FLAGS_H = 118,    // WORD Flags (only in version 2, 3 & 4 header)
    SIDFILE_PSID_FLAGS_L = 119,    // WORD Flags (only in version 2, 3 & 4 header)
    SIDFILE_PSID_STARTPAGE = 120,  // BYTE startPage (relocStartPage)
    SIDFILE_PSID_PAGELENGTH = 121, // BYTE pageLength (relocPages)
    SIDFILE_PSID_SECONDSID = 122,  // BYTE secondSIDAddress $42..$FE
    SIDFILE_PSID_THIRDSID = 123,   // BYTE thirdSIDAddress $42..$FE
    SIDFILE_PSID_FOURTHSID = 124,  // BYTE fourthSIDAddress $42..$FE
};

enum
{
    SIDFILE_OK,
    SIDFILE_ERROR_FILENOTFOUND,
    SIDFILE_ERROR_MALFORMED
};

enum
{
    SIDFILE_SPEED_50HZ,
    SIDFILE_SPEED_60HZ
};

class SidFile
{
private:
    string moduleName;
    string authorName;
    string copyrightInfo;
    string sidType;
    int numOfSongs;
    int firstSong;
    uint16_t sidVersion;
    uint16_t dataOffset;
    uint16_t initAddr;
    uint16_t playAddr;
    uint64_t loadAddr;
    uint32_t speedFlags;
    uint8_t dataBuffer[0x10000];
    uint16_t dataLength;
    uint16_t sidFlags;
    uint16_t clockSpeed;
    uint16_t chipType;
    uint16_t chipType2;
    uint16_t chipType3;
    uint8_t startPage;
    uint8_t pageLength;
    uint8_t secondSID;
    uint8_t thirdSID;
    uint8_t fourthSID;

    uint8_t Read8(const uint8_t *p, int offset);
    uint16_t Read16(const uint8_t *p, int offset);
    uint32_t Read32(const uint8_t *p, int offset);
    bool IsPSIDHeader(const uint8_t *p);
    unsigned char reverse(unsigned char b);

public:
    SidFile();
    int Parse(string file);
    string GetSidType();
    string GetModuleName();
    string GetAuthorName();
    string GetCopyrightInfo();
    int GetSongSpeed(int songNum);
    int GetNumOfSongs();
    int GetFirstSong();
    uint8_t *GetDataPtr();
    uint16_t GetDataLength();
    uint16_t GetSidVersion();
    uint16_t GetSidFlags();
    uint16_t GetChipType(int n);
    uint16_t GetSIDaddr(int n);
    uint16_t GetClockSpeed();
    uint16_t GetDataOffset();
    uint16_t GetLoadAddress();
    uint16_t GetInitAddress();
    uint16_t GetPlayAddress();
};
