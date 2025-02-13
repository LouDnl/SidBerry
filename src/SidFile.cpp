//============================================================================
// Description : SID file parser
// Authors     : Gianluca Ghettini, LouD
// Last update : 2024
//============================================================================

#include "SidFile.h"

SidFile::SidFile()
{
}

/* Source: https://stackoverflow.com/a/2602885 */
unsigned char SidFile::reverse(unsigned char b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

uint16_t SidFile::Read16(const uint8_t *p, int offset)
{
    return (p[offset] << 8) | p[offset + 1];
}

uint8_t SidFile::Read8(const uint8_t *p, int offset)
{
    return p[offset];
}

uint32_t SidFile::Read32(const uint8_t *p, int offset)
{
    return (p[offset] << 24) | (p[offset + 1] << 16) | (p[offset + 2] << 8) | p[offset + 3];
}

bool SidFile::IsPSIDHeader(const uint8_t *p)
{
    uint32_t id = Read32(p, SIDFILE_PSID_ID);
    uint16_t version = Read16(p, SIDFILE_PSID_VERSION_H);
    /* Add v3 Dual SID, v4 Triple SID, v5 Quad SID file support */
    return id == 0x50534944 && (version >= 1 || version <= 4);
}

int SidFile::Parse(std::string file)
{
    FILE *f = fopen(file.c_str(), "rb");

    if (f == NULL)
    {
        return SIDFILE_ERROR_FILENOTFOUND;
    }

    uint8_t header[PSID_MAX_HEADER_LENGTH];
    memset(header, 0, PSID_MAX_HEADER_LENGTH);

    size_t read = fread(header, 1, PSID_MAX_HEADER_LENGTH, f);

    if (read < PSID_MIN_HEADER_LENGTH || !IsPSIDHeader(header))
    {
        fclose(f);
        return SIDFILE_ERROR_MALFORMED;
    }

    numOfSongs = Read16(header, SIDFILE_PSID_NUMBER_H);

    if (numOfSongs == 0)
    {
        numOfSongs = 1;
    }

    firstSong = Read16(header, SIDFILE_PSID_DEFSONG_H);
    if (firstSong)
    {
        firstSong--;
    }
    if (firstSong >= numOfSongs)
    {
        firstSong = 0;
    }

    dataOffset = Read16(header, SIDFILE_PSID_LENGTH_H);
    initAddr = Read16(header, SIDFILE_PSID_INIT_H);
    playAddr = Read16(header, SIDFILE_PSID_MAIN_H);
    speedFlags = Read32(header, SIDFILE_PSID_SPEED);

    moduleName = (char *)(header + SIDFILE_PSID_NAME);

    authorName = (char *)(header + SIDFILE_PSID_AUTHOR);

    copyrightInfo = (char *)(header + SIDFILE_PSID_COPYRIGHT);

    sidType = (char *)(header + SIDFILE_PSID_ID);
    sidVersion = Read16(header, SIDFILE_PSID_VERSION_H);

    // Seek to start of module data
    fseek(f, Read16(header, SIDFILE_PSID_LENGTH_H), SEEK_SET);

    // Find load address
    loadAddr = Read16(header, SIDFILE_PSID_START_H);
    if (loadAddr == 0)
    {
        uint8_t lo = fgetc(f);
        uint8_t hi = fgetc(f);
        loadAddr = (hi << 8) | lo;
    }

    if (initAddr == 0)
    {
        initAddr = loadAddr;
    }

    // Load module data
    dataLength = fread(dataBuffer, 1, 0x10000, f);

    // flags start at 0x76
    sidFlags = Read16(header, SIDFILE_PSID_FLAGS_H);

    // - Bit 0 specifies format of the binary data (musPlayer):
    // 0 = built-in music player,
    // 1 = Compute!'s Sidplayer MUS data, music player must be merged.

    // x = 20; // 0x0014
    // x = x & 1;

    // - Bit 1 specifies whether the tune is PlaySID specific, e.g. uses PlaySID
    // samples (psidSpecific):
    // 0 = C64 compatible,
    // 1 = PlaySID specific (PSID v2NG, v3, v4)
    // 1 = C64 BASIC flag (RSID)

    // x = x & 2;

    // - Bits 2-3 specify the video standard (clock):
    // 00 = Unknown,
    // 01 = PAL,
    // 10 = NTSC,
    // 11 = PAL and NTSC.

    // clockSpeed = (reverse(sidFlags) >> 5) & 3; // 0b00001100 bits 2 & 3 ~ but from reverse bits
    // clockSpeed = (sidFlags >> 2) & 3; // 0b00001100 bits 2 & 3
    clockSpeed = ((sidFlags & 0xC) >> 2) & 3; // 0b00001100 bits 2 & 3

    // - Bits 4-5 specify the SID version (sidModel):
    // 00 = Unknown,
    // 01 = MOS6581,
    // 10 = MOS8580,
    // 11 = MOS6581 and MOS8580.

    // chipType = (reverse(sidFlags) >> 3) & 3; // 0b00110000 bits 4 & 5 ~ but from reverse bits
    chipType = (sidFlags >> 4) & 3; // 0b00110000 bits 4 & 5

    // This is a v2NG specific field.
    // - Bits 6-7 specify the SID version (sidModel) of the second SID:
    // 00 = Unknown, <- then same as SID 1
    // 01 = MOS6581,
    // 10 = MOS8580,
    // 11 = MOS6581 and MOS8580.
    chipType2 = (sidFlags >> 6) & 3; // 0b00110000 bits 4 & 5

    // This is a v3 specific field.
    // - Bits 8-9 specify the SID version (sidModel) of the third SID:
    // 00 = Unknown, <- then same as SID 1
    // 01 = MOS6581,
    // 10 = MOS8580,
    // 11 = MOS6581 and MOS8580.
    chipType3 = (sidFlags >> 8) & 3; // 0b00110000 bits 4 & 5

    startPage = Read8(header, SIDFILE_PSID_STARTPAGE);
    pageLength = Read8(header, SIDFILE_PSID_PAGELENGTH);
    if (sidVersion == 3 || sidVersion == 4 || sidVersion == 78) {
        secondSID = Read8(header, SIDFILE_PSID_SECONDSID);
        thirdSID = Read8(header, SIDFILE_PSID_THIRDSID);
        fourthSID = Read8(header, SIDFILE_PSID_FOURTHSID);
    }

    fclose(f);

    return SIDFILE_OK;
}

std::string SidFile::GetSidType()
{
    return sidType;
}

std::string SidFile::GetModuleName()
{
    return moduleName;
}

std::string SidFile::GetAuthorName()
{
    return authorName;
}

std::string SidFile::GetCopyrightInfo()
{
    return copyrightInfo;
}

int SidFile::GetSongSpeed(int songNum)
{
    // return (speedFlags & (1 << songNum)) ? SIDFILE_SPEED_60HZ : SIDFILE_SPEED_50HZ;
    return speedFlags;
}

int SidFile::GetNumOfSongs()
{
    return numOfSongs;
}

int SidFile::GetFirstSong()
{
    return firstSong;
}

uint8_t *SidFile::GetDataPtr()
{
    return dataBuffer;
}

uint16_t SidFile::GetDataLength()
{
    return dataLength;
}

uint16_t SidFile::GetSidVersion()
{
    return sidVersion;
}

uint16_t SidFile::GetSidFlags()
{
    return sidFlags;
}

uint16_t SidFile::GetChipType(int n)
{
    return n == 3 ? chipType3 : n == 2 ? chipType2 : chipType;
}

uint16_t SidFile::GetSIDaddr(int n)
{
    return n == 2 ? secondSID : n == 3 ? thirdSID : fourthSID;
}

uint16_t SidFile::GetClockSpeed()
{
    return clockSpeed;
}

uint16_t SidFile::GetDataOffset()
{
    return dataOffset;
}

uint16_t SidFile::GetLoadAddress()
{
    return loadAddr;
}

uint16_t SidFile::GetInitAddress()
{
    return initAddr;
}

uint16_t SidFile::GetPlayAddress()
{
    return playAddr;
}
