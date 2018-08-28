//==========================================================================
// Mouse Injector Plugin
//==========================================================================
// Copyright (C) 2018 Carnivorous
// All rights reserved.
//
// Mouse Injector is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, visit http://www.gnu.org/licenses/gpl-2.0.html
//==========================================================================
#define LOAD_SHALF_PARAM(param) *(short *) ((unsigned char *)rdramptr[(((param) >> 12))] + ( (param ) ^ 2))
#define LOAD_SWORD_PARAM(param) *(int *)((unsigned char *)rdramptr[(((param) >> 12))] + (param ))
#define LOAD_UWORD_PARAM(param) *(unsigned int *)((unsigned char *)rdramptr[(((param) >> 12))] + (param ))

#define WITHINRANGE rdramptr != 0 && addr / 0x1000000 == 0x80
#define GARBAGESHORT (short)0xABAD
#define GARBAGEINT (int)0xABADC0DE

//==========================================================================
// Purpose: read short from memory and then return memory value
// Parameter: address location plus offset
//==========================================================================
static inline short EMU_ReadShort(const unsigned int addr, const unsigned int offset)
{
	return WITHINRANGE ? LOAD_SHALF_PARAM(addr + offset + 2) : GARBAGESHORT; // return garbage if request invalid location
}
//==========================================================================
// Purpose: read int from memory and then return memory value
// Parameter: address location plus offset
//==========================================================================
static inline int EMU_ReadInt(const unsigned int addr, const unsigned int offset)
{
	if (WITHINRANGE)
	{
		return LOAD_SWORD_PARAM(addr + offset);
		auto off = addr + offset;
		unsigned char* ram = (unsigned char*)rdramptr;

		if (ram[off >> 12] == 0)
			return 0;

		auto fucker = off >> 12;

		//int* place2 = ((unsigned char *)rdramptr[off >> 12] + (off & 0x00000FFF));

		auto val = ram[fucker];
		auto final = off & 0x00000FFF;
		auto final2 = off ^ 3;

		int ret = *(int *)(val + final);

		//return *place;
		//*(int *)((unsigned char *)rdramptr[(((param) >> 12))] + (((param) & 0x00000FFF)))
		return 0;// LOAD_SWORD_PARAM(addr + offset);
	}
	return GARBAGEINT;
	//return WITHINRANGE ? LOAD_SWORD_PARAM(addr + offset) : GARBAGEINT;
}
//==========================================================================
// Purpose: read float from memory and then return memory value
// Parameter: address location plus offset
//==========================================================================
static inline float EMU_ReadFloat(const unsigned int addr, const unsigned int offset)
{
	return WITHINRANGE ? *((float *)&LOAD_UWORD_PARAM(addr + offset)) : GARBAGEINT;
}
//==========================================================================
// Purpose: read int from emulator's copy of rom and then return memory value
// Parameter: address location plus offset
//==========================================================================
static inline unsigned EMU_ReadROM(const unsigned int addr)
{
	return (unsigned)romptr[addr / 0x4];
}
//==========================================================================
// Purpose: write short to memory location
// Parameter: address location, offset and value
//==========================================================================
static inline void EMU_WriteShort(const unsigned int addr, const unsigned int offset, const short value)
{
	if(WITHINRANGE)
		LOAD_SHALF_PARAM(addr + offset + 2) = value;
}
//==========================================================================
// Purpose: write int to memory location
// Parameter: address location, offset and value
//==========================================================================
static inline void EMU_WriteInt(const unsigned int addr, const unsigned int offset, const int value)
{
	if(WITHINRANGE)
		LOAD_SWORD_PARAM(addr + offset) = value;
}
//==========================================================================
// Purpose: write float to memory location
// Parameter: address location, offset and value
//==========================================================================
static inline void EMU_WriteFloat(const unsigned int addr, const unsigned int offset, const float value)
{
	if(WITHINRANGE)
		LOAD_UWORD_PARAM(addr + offset) = *(unsigned int *)(&value);
}
//==========================================================================
// Purpose: write to emulator's copy of rom
// Parameter: address location and value
//==========================================================================
static inline void EMU_WriteROM(const unsigned int addr, const unsigned int value)
{
	if(romptr != 0)
		romptr[addr / 0x4] = (void *)value;
}