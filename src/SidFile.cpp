//============================================================================
// Description : SID file parser
// Authors     : Gianluca Ghettini, LouD
// Last update : 2024
//============================================================================

#include "SidFile.h"

SidFile::SidFile()
{
}

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

uint32_t SidFile::Read32(const uint8_t *p, int offset)
{
	return (p[offset] << 24) | (p[offset + 1] << 16) | (p[offset + 2] << 8) | p[offset + 3];
}

bool SidFile::IsPSIDHeader(const uint8_t *p)
{
	uint32_t id = Read32(p, SIDFILE_PSID_ID);
	uint16_t version = Read16(p, SIDFILE_PSID_VERSION);
	return id == 0x50534944 && (version == 1 || version == 2);
}

int SidFile::Parse(string file)
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

	numOfSongs = Read16(header, SIDFILE_PSID_NUMBER);

	if (numOfSongs == 0)
	{
		numOfSongs = 1;
	}

	firstSong = Read16(header, SIDFILE_PSID_DEFSONG);
	if (firstSong)
	{
		firstSong--;
	}
	if (firstSong >= numOfSongs)
	{
		firstSong = 0;
	}

	dataOffset = Read16(header, SIDFILE_PSID_LENGTH);
	initAddr = Read16(header, SIDFILE_PSID_INIT);
	playAddr = Read16(header, SIDFILE_PSID_MAIN);
	speedFlags = Read32(header, SIDFILE_PSID_SPEED);

	moduleName = (char *)(header + SIDFILE_PSID_NAME);

	authorName = (char *)(header + SIDFILE_PSID_AUTHOR);

	copyrightInfo = (char *)(header + SIDFILE_PSID_COPYRIGHT);

	sidType = (char *)(header + SIDFILE_PSID_ID);
	sidVersion = Read16(header, SIDFILE_PSID_VERSION);

	// Seek to start of module data
	fseek(f, Read16(header, SIDFILE_PSID_LENGTH), SEEK_SET);

	// Find load address
	loadAddr = Read16(header, SIDFILE_PSID_START);
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
	sidFlags = Read16(header, SIDFILE_PSID_FLAGS);

	// - Bit 0 specifies format of the binary data (musPlayer):
	// 0 = built-in music player,
	// 1 = Compute!'s Sidplayer MUS data, music player must be merged.

	// x = 20; // 0x0014
	// x = x & 1;

	// - Bit 1 specifies whether the tune is PlaySID specific, e.g. uses PlaySID
	// samples (psidSpecific):
	// 0 = C64 compatible,
	// 1 = PlaySID specific (PSID v2NG)
	// 1 = C64 BASIC flag (RSID)

	// x = x & 2;

	// - Bits 2-3 specify the video standard (clock):
	// 00 = Unknown,
	// 01 = PAL,
	// 10 = NTSC,
	// 11 = PAL and NTSC.

	// clockSpeed = (reverse(sidFlags) >> 5) & 3; // 0b00001100 bits 2 & 3 ~ but from reverse bits
	clockSpeed = (sidFlags >> 2) & 3; // 0b00001100 bits 2 & 3 ~ but from reverse bits

	// - Bits 4-5 specify the SID version (sidModel):
	// 00 = Unknown,
	// 01 = MOS6581,
	// 10 = MOS8580,
	// 11 = MOS6581 and MOS8580.

	// chipType = (reverse(sidFlags) >> 3) & 3; // 0b00110000 bits 4 & 5 ~ but from reverse bits
	chipType = (sidFlags >> 4) & 3; // 0b00110000 bits 4 & 5 ~ but from reverse bits

	fclose(f);

	return SIDFILE_OK;
}

string SidFile::GetSidType()
{
	return sidType;
}

string SidFile::GetModuleName()
{
	return moduleName;
}

string SidFile::GetAuthorName()
{
	return authorName;
}

string SidFile::GetCopyrightInfo()
{
	return copyrightInfo;
}

int SidFile::GetSongSpeed(int songNum)
{
	return (speedFlags & (1 << songNum)) ? SIDFILE_SPEED_60HZ : SIDFILE_SPEED_50HZ;
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

uint16_t SidFile::GetChipType()
{
	return chipType;
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
