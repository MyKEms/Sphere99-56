//
// CCryptBase.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
// Common for client and server.
//

#include "stdafx.h"
#include "spherecommon.h"
#include "spheremul.h"
#include "sphereproto.h"
#include "ccrypt.h"

//**************************************************
// CCryptNew
// derived from HackersQuest.gomp.ch 2000

const DWORD CCryptNew::sm_InitData1[4] =
{ 0, 0x10, 0x10, 0x10 };

const BYTE CCryptNew::sm_InitData2[2][0x100] =
{
	// @014f160 in v2.0.3
	// @0150610 in v2.0.4
	{
		0xA9, 0x67, 0xB3, 0xE8, 0x04, 0xFD, 0xA3, 0x76, 0x9A, 0x92, 0x80, 0x78, 0xE4, 0xDD, 0xD1, 0x38,
		0x0D, 0xC6, 0x35, 0x98, 0x18, 0xF7, 0xEC, 0x6C, 0x43, 0x75, 0x37, 0x26, 0xFA, 0x13, 0x94, 0x48,
		0xF2, 0xD0, 0x8B, 0x30, 0x84, 0x54, 0xDF, 0x23, 0x19, 0x5B, 0x3D, 0x59, 0xF3, 0xAE, 0xA2, 0x82,
		0x63, 0x01, 0x83, 0x2E, 0xD9, 0x51, 0x9B, 0x7C, 0xA6, 0xEB, 0xA5, 0xBE, 0x16, 0x0C, 0xE3, 0x61,
		0xC0, 0x8C, 0x3A, 0xF5, 0x73, 0x2C, 0x25, 0x0B, 0xBB, 0x4E, 0x89, 0x6B, 0x53, 0x6A, 0xB4, 0xF1,
		0xE1, 0xE6, 0xBD, 0x45, 0xE2, 0xF4, 0xB6, 0x66, 0xCC, 0x95, 0x03, 0x56, 0xD4, 0x1C, 0x1E, 0xD7,
		0xFB, 0xC3, 0x8E, 0xB5, 0xE9, 0xCF, 0xBF, 0xBA, 0xEA, 0x77, 0x39, 0xAF, 0x33, 0xC9, 0x62, 0x71,
		0x81, 0x79, 0x09, 0xAD, 0x24, 0xCD, 0xF9, 0xD8, 0xE5, 0xC5, 0xB9, 0x4D, 0x44, 0x08, 0x86, 0xE7,
		0xA1, 0x1D, 0xAA, 0xED, 0x06, 0x70, 0xB2, 0xD2, 0x41, 0x7B, 0xA0, 0x11, 0x31, 0xC2, 0x27, 0x90,
		0x20, 0xF6, 0x60, 0xFF, 0x96, 0x5C, 0xB1, 0xAB, 0x9E, 0x9C, 0x52, 0x1B, 0x5F, 0x93, 0x0A, 0xEF,
		0x91, 0x85, 0x49, 0xEE, 0x2D, 0x4F, 0x8F, 0x3B, 0x47, 0x87, 0x6D, 0x46, 0xD6, 0x3E, 0x69, 0x64,
		0x2A, 0xCE, 0xCB, 0x2F, 0xFC, 0x97, 0x05, 0x7A, 0xAC, 0x7F, 0xD5, 0x1A, 0x4B, 0x0E, 0xA7, 0x5A,
		0x28, 0x14, 0x3F, 0x29, 0x88, 0x3C, 0x4C, 0x02, 0xB8, 0xDA, 0xB0, 0x17, 0x55, 0x1F, 0x8A, 0x7D,
		0x57, 0xC7, 0x8D, 0x74, 0xB7, 0xC4, 0x9F, 0x72, 0x7E, 0x15, 0x22, 0x12, 0x58, 0x07, 0x99, 0x34,
		0x6E, 0x50, 0xDE, 0x68, 0x65, 0xBC, 0xDB, 0xF8, 0xC8, 0xA8, 0x2B, 0x40, 0xDC, 0xFE, 0x32, 0xA4,
		0xCA, 0x10, 0x21, 0xF0, 0xD3, 0x5D, 0x0F, 0x00, 0x6F, 0x9D, 0x36, 0x42, 0x4A, 0x5E, 0xC1, 0xE0
	},
	{
		0x75, 0xF3, 0xC6, 0xF4, 0xDB, 0x7B, 0xFB, 0xC8, 0x4A, 0xD3, 0xE6, 0x6B, 0x45, 0x7D, 0xE8, 0x4B,
		0xD6, 0x32, 0xD8, 0xFD, 0x37, 0x71, 0xF1, 0xE1, 0x30, 0x0F, 0xF8, 0x1B, 0x87, 0xFA, 0x06, 0x3F,
		0x5E, 0xBA, 0xAE, 0x5B, 0x8A, 0x00, 0xBC, 0x9D, 0x6D, 0xC1, 0xB1, 0x0E, 0x80, 0x5D, 0xD2, 0xD5,
		0xA0, 0x84, 0x07, 0x14, 0xB5, 0x90, 0x2C, 0xA3, 0xB2, 0x73, 0x4C, 0x54, 0x92, 0x74, 0x36, 0x51,
		0x38, 0xB0, 0xBD, 0x5A, 0xFC, 0x60, 0x62, 0x96, 0x6C, 0x42, 0xF7, 0x10, 0x7C, 0x28, 0x27, 0x8C,
		0x13, 0x95, 0x9C, 0xC7, 0x24, 0x46, 0x3B, 0x70, 0xCA, 0xE3, 0x85, 0xCB, 0x11, 0xD0, 0x93, 0xB8,
		0xA6, 0x83, 0x20, 0xFF, 0x9F, 0x77, 0xC3, 0xCC, 0x03, 0x6F, 0x08, 0xBF, 0x40, 0xE7, 0x2B, 0xE2,
		0x79, 0x0C, 0xAA, 0x82, 0x41, 0x3A, 0xEA, 0xB9, 0xE4, 0x9A, 0xA4, 0x97, 0x7E, 0xDA, 0x7A, 0x17,
		0x66, 0x94, 0xA1, 0x1D, 0x3D, 0xF0, 0xDE, 0xB3, 0x0B, 0x72, 0xA7, 0x1C, 0xEF, 0xD1, 0x53, 0x3E,
		0x8F, 0x33, 0x26, 0x5F, 0xEC, 0x76, 0x2A, 0x49, 0x81, 0x88, 0xEE, 0x21, 0xC4, 0x1A, 0xEB, 0xD9,
		0xC5, 0x39, 0x99, 0xCD, 0xAD, 0x31, 0x8B, 0x01, 0x18, 0x23, 0xDD, 0x1F, 0x4E, 0x2D, 0xF9, 0x48,
		0x4F, 0xF2, 0x65, 0x8E, 0x78, 0x5C, 0x58, 0x19, 0x8D, 0xE5, 0x98, 0x57, 0x67, 0x7F, 0x05, 0x64,
		0xAF, 0x63, 0xB6, 0xFE, 0xF5, 0xB7, 0x3C, 0xA5, 0xCE, 0xE9, 0x68, 0x44, 0xE0, 0x4D, 0x43, 0x69,
		0x29, 0x2E, 0xAC, 0x15, 0x59, 0xA8, 0x0A, 0x9E, 0x6E, 0x47, 0xDF, 0x34, 0x35, 0x6A, 0xCF, 0xDC,
		0x22, 0xC9, 0xC0, 0x9B, 0x89, 0xD4, 0xED, 0xAB, 0x12, 0xA2, 0x0D, 0x52, 0xBB, 0x02, 0x2F, 0xA9,
		0xD7, 0x61, 0x1E, 0xB4, 0x50, 0x04, 0xF6, 0xC2, 0x16, 0x25, 0x86, 0x56, 0x55, 0x09, 0xBE, 0x91
	}
};

DWORD CCryptNew::sm_CodingData[4][0x100];
bool  CCryptNew::sm_fInitTables = false;

static void InitTables()
{
	// Init the sm_CodingData table.
	for(int i=0; i<0x100; i++)
	{
		BYTE val0 = CCryptNew::sm_InitData2[0][i];                                              // 5
		BYTE val1 = val0 ^ (val0>>2) ^ ((val0 & 2)?0xB4:0) ^ ((val0 & 1)?0x5A:0); // 3
		BYTE val2 = val1 ^ (val0>>1) ^ ((val0 & 1)?0xB4:0);                       // 2

		CCryptNew::sm_CodingData[1][i] = val2 | (val2<<8) | (val1 << 16) | (val0 << 24);
		CCryptNew::sm_CodingData[3][i] = val1 | (val0<<8) | (val2 << 16) | (val1 << 24);

		val0 = CCryptNew::sm_InitData2[1][i];                                                   // 1
		val1 = val0 ^ (val0>>2) ^ ((val0 & 2)?0xB4:0) ^ ((val0 & 1)?0x5A:0);      // 4
		val2 = val1 ^ (val0>>1) ^ ((val0 & 1)?0xB4:0);                            // 0

		CCryptNew::sm_CodingData[0][i] = val0 | (val1<<8) | (val2 << 16) | (val2 << 24);
		CCryptNew::sm_CodingData[2][i] = val1 | (val2<<8) | (val0 << 16) | (val2 << 24);
	}
	CCryptNew::sm_fInitTables = true;
}

static int function02(DWORD v1, DWORD v2)
{
	DWORD val = v1;

	int i;
	for(i=0; i<4; i++)
	{
		BYTE val0  = val >> 24;
		BYTE val1 = (val0<<1) ^ ((val0 & 0x80)?0x4D:0);
		BYTE val2 = ((val0 & 0x01)?0xA6:0) ^ (val0>>1) ^ val1;

		val = (val << 8) ^ (val2 << 24) ^ (val1 << 16) ^ (val2 << 8) ^ val0;
	}

	val ^= v2;

	for(i=0; i<4; i++)
	{
		BYTE val0  = val >> 24;
		BYTE val1 = (val0<<1) ^ ((val0 & 0x80)?0x4D:0);
		BYTE val2 = ((val0 & 0x01)?0xA6:0) ^ (val0>>1) ^ val1;

		val = (val << 8) ^ (val2 << 24) ^ (val1 << 16) ^ (val2 << 8) ^ val0;
	}
	return val;
}

static void function03(CCryptSubData1 & subData1, DWORD param2)
{
	for (DWORD i = 0; i < subData1.size2/2; i++)
	{
		DWORD tmp = subData1.data3[4+i][0];
		subData1.data3[4+i][0] = subData1.data3[4+subData1.size2-i-1][0];
		subData1.data3[4+subData1.size2-i-1][0] = tmp;

		tmp = subData1.data3[4+i][1];
		subData1.data3[4+i][1] = subData1.data3[4+subData1.size2-i-1][1];
		subData1.data3[4+subData1.size2-i-1][1] = tmp;
	}
}

static void BlockXOR(BYTE * dst, const BYTE * src, BYTE value)
{
	for (DWORD i=0; i<0x100; i++)
	{
		dst[i] = src[i] ^ value;
	}
}

static DWORD function05( CCryptSubData1 & subData1 )
{
	if ( ! CCryptNew::sm_fInitTables)
	{
		InitTables();
	}

	DWORD size = (subData1.size1 + 0x3F) >> 6;
	if(size)
	{
		DWORD i;
		for (i=0; i<size; i++)
		{
			subData1.data2[size-i-1].u_dw = function02(subData1.data1[i][0].u_dw, subData1.data1[i][1].u_dw);
		}
		DWORD initCode = 0;

		for (i=0; i<subData1.size2+4; i++)
		{
			CCryptDWord code;
			code.u_dw = initCode;
			DWORD result1, result2;
			switch(size)
			{
			case 4:
				code.u_ch[0] = CCryptNew::sm_InitData2[1][code.u_ch[0]] ^ subData1.data1[3][0].u_ch[0];
				code.u_ch[1] = CCryptNew::sm_InitData2[0][code.u_ch[1]] ^ subData1.data1[3][0].u_ch[1];
				code.u_ch[2] = CCryptNew::sm_InitData2[0][code.u_ch[2]] ^ subData1.data1[3][0].u_ch[2];
				code.u_ch[3] = CCryptNew::sm_InitData2[1][code.u_ch[3]] ^ subData1.data1[3][0].u_ch[3];
			case 3:
				code.u_ch[0] = CCryptNew::sm_InitData2[1][code.u_ch[0]] ^ subData1.data1[2][0].u_ch[0];
				code.u_ch[1] = CCryptNew::sm_InitData2[1][code.u_ch[1]] ^ subData1.data1[2][0].u_ch[1];
				code.u_ch[2] = CCryptNew::sm_InitData2[0][code.u_ch[2]] ^ subData1.data1[2][0].u_ch[2];
				code.u_ch[3] = CCryptNew::sm_InitData2[0][code.u_ch[3]] ^ subData1.data1[2][0].u_ch[3];
			case 2:
				BYTE i3 = CCryptNew::sm_InitData2[1][CCryptNew::sm_InitData2[1][code.u_ch[3]] ^ subData1.data1[1][0].u_ch[3]] ^ subData1.data1[0][0].u_ch[3];
				BYTE i2 = CCryptNew::sm_InitData2[1][CCryptNew::sm_InitData2[0][code.u_ch[2]] ^ subData1.data1[1][0].u_ch[2]] ^ subData1.data1[0][0].u_ch[2];
				BYTE i1 = CCryptNew::sm_InitData2[0][CCryptNew::sm_InitData2[1][code.u_ch[1]] ^ subData1.data1[1][0].u_ch[1]] ^ subData1.data1[0][0].u_ch[1];
				BYTE i0 = CCryptNew::sm_InitData2[0][CCryptNew::sm_InitData2[0][code.u_ch[0]] ^ subData1.data1[1][0].u_ch[0]] ^ subData1.data1[0][0].u_ch[0];

				result1 = CCryptNew::sm_CodingData[3][i3] ^ CCryptNew::sm_CodingData[2][i2] ^ CCryptNew::sm_CodingData[1][i1] ^ CCryptNew::sm_CodingData[0][i0];
			}
			initCode += 0x01010101;

			code.u_dw = initCode;
			switch(size)
			{
			case 4:
				code.u_ch[0] = CCryptNew::sm_InitData2[1][code.u_ch[0]] ^ subData1.data1[3][1].u_ch[0];
				code.u_ch[1] = CCryptNew::sm_InitData2[0][code.u_ch[1]] ^ subData1.data1[3][1].u_ch[1];
				code.u_ch[2] = CCryptNew::sm_InitData2[0][code.u_ch[2]] ^ subData1.data1[3][1].u_ch[2];
				code.u_ch[3] = CCryptNew::sm_InitData2[1][code.u_ch[3]] ^ subData1.data1[3][1].u_ch[3];
			case 3:
				code.u_ch[0] = CCryptNew::sm_InitData2[1][code.u_ch[0]] ^ subData1.data1[2][1].u_ch[0];
				code.u_ch[1] = CCryptNew::sm_InitData2[1][code.u_ch[1]] ^ subData1.data1[2][1].u_ch[1];
				code.u_ch[2] = CCryptNew::sm_InitData2[0][code.u_ch[2]] ^ subData1.data1[2][1].u_ch[2];
				code.u_ch[3] = CCryptNew::sm_InitData2[0][code.u_ch[3]] ^ subData1.data1[2][1].u_ch[3];
			case 2:
				BYTE i3 = CCryptNew::sm_InitData2[1][CCryptNew::sm_InitData2[1][code.u_ch[3]] ^ subData1.data1[1][1].u_ch[3]] ^ subData1.data1[0][1].u_ch[3];
				BYTE i2 = CCryptNew::sm_InitData2[1][CCryptNew::sm_InitData2[0][code.u_ch[2]] ^ subData1.data1[1][1].u_ch[2]] ^ subData1.data1[0][1].u_ch[2];
				BYTE i1 = CCryptNew::sm_InitData2[0][CCryptNew::sm_InitData2[1][code.u_ch[1]] ^ subData1.data1[1][1].u_ch[1]] ^ subData1.data1[0][1].u_ch[1];
				BYTE i0 = CCryptNew::sm_InitData2[0][CCryptNew::sm_InitData2[0][code.u_ch[0]] ^ subData1.data1[1][1].u_ch[0]] ^ subData1.data1[0][1].u_ch[0];

				result2 = CCryptNew::sm_CodingData[3][i3] ^ CCryptNew::sm_CodingData[2][i2] ^ CCryptNew::sm_CodingData[1][i1] ^ CCryptNew::sm_CodingData[0][i0];
			}
			initCode += 0x01010101;

			result2 = (result2 << 8) | (result2 >> 24);
			subData1.data3[i][0] = result1 + result2;

			result1 += 2 * result2;
			subData1.data3[i][1] = (result1 << 9) | (result1 >> 23);

			code.u_dw += 0x01010101;
		}
		BYTE buffer[0x100];

		switch(subData1.size1)
		{
			DWORD j;
		case 0x100:
			for(j=0; j<4; j++)
			{
				BlockXOR( buffer, CCryptNew::sm_InitData2[j==0 || j==3], subData1.data2[3].u_ch[j]);
				for(i=0; i<0x100; i++)
				{
					buffer[i] = CCryptNew::sm_InitData2[j>>1][buffer[i]];
				}
				BlockXOR(buffer, buffer, subData1.data2[2].u_ch[j]);
				for(i=0; i<0x100; i++)
				{
					subData1.data4[j>>1][i][j&1] = CCryptNew::sm_CodingData[j][CCryptNew::sm_InitData2[j>>1][CCryptNew::sm_InitData2[j&1][buffer[i]] ^ subData1.data2[1].u_ch[j]] ^ subData1.data2[0].u_ch[j]];
				}
			}
			break;
		case 0xC0:
			for(j=0; j<4; j++)
			{
				BlockXOR(buffer, CCryptNew::sm_InitData2[1-(j>>1)], subData1.data2[2].u_ch[j]);
				for(i=0; i<0x100; i++)
				{
					subData1.data4[j>>1][i][j&1] = CCryptNew::sm_CodingData[j][CCryptNew::sm_InitData2[j>>1][CCryptNew::sm_InitData2[j&1][buffer[i]] ^ subData1.data2[1].u_ch[j]] ^ subData1.data2[0].u_ch[j]];
				}
			}
			break;
		case 0x80:
			for(j=0; j<4; j++)
			{
				BlockXOR(buffer, CCryptNew::sm_InitData2[j&1], subData1.data2[1].u_ch[j]);
				for(i=0; i<0x100; i++)
				{
					subData1.data4[j>>1][i][j&1] = CCryptNew::sm_CodingData[j][CCryptNew::sm_InitData2[j>>1][buffer[i]] ^ subData1.data2[0].u_ch[j]];
				}
			}
			break;
		}
		if(subData1.type == 0) function03(subData1, 0);
	}
	return 1;
}

#if 0

static int function06(DWORD size, const BYTE * initData, DWORD * result, BYTE * initCopy)
{
	if(size > 0)
	{
		for(DWORD i=0; i*4 < size; i++)
		{
			result[i] = 0;
		}
		for(i=0; i<size; i++)
		{
			BYTE value = initData[i];
			if(initCopy) initCopy[i] = value;
			if(value >= '0' && value <= '9')
			{
				value -= '0';
			}
			else if(value >= 'a' && value <= 'f')
			{
				value -= 'a' - 10;
			}
			else if(value >= 'A' && value <= 'F')
			{
				value -= 'A' - 10;
			}
			else
			{
				return -2;
			}
			result[i >> 3] |= value << (((i ^ 1) & 7) << 2);
		}
	}
	return 0;
}

#endif

static int InitSubData1(CCryptSubData1 & subData1, BYTE type, DWORD size, const BYTE * initString)
{
	// 0x04dcbe0 in v2.0.3
	subData1.type  = type;
	subData1.size1 = (size + 0x3F) & ~0x3F;
	subData1.size2 = CCryptNew::sm_InitData1[(size - 1)>>6];
	for(int i=0; i<4; i++)
	{
		subData1.data1[i][0].u_dw = subData1.data1[i][1].u_dw = 0;
	}
	subData1.zero  = 0;

#if 1
	ASSERT(initString == NULL );
#else
	if(initString && *initString)
	{
		if(!function06(size, initString, (DWORD *)subData1.data1, subData1.initCopy))
		{
			return function05(subData1);
		}
	}
#endif

	return 1;
}

static int InitSubData2(CCryptSubData2 & subData2, BYTE type, const BYTE * initString)
{
#if 1
	ASSERT(initString == NULL );
#else
	if(type != 1)
	{
		if(initString)
		{
			if(!function06(0x80, initString, (DWORD *)&(subData2.data2), NULL))
			{
				return -7;
			}
			memcpy(subData2.data1, &subData2.data2, 16);
		}
	}
#endif
	subData2.type = type;
	return 1;
}

static void function09(CCryptSubData2 & subData2, CCryptSubData1 & data1, const BYTE * data3, DWORD size, BYTE * tmpBuff)
{
	if ( subData2.type == 3 )
	{
		subData2.type = 1;
		BYTE buffer[0x10];
		for(DWORD i=0; i<size; i++)
		{
			function09(subData2, data1, subData2.data1, 0x80, buffer);

			DWORD index = i >> 7;
			DWORD shift = i & 7;
			DWORD mask  = 0x80 >> shift;
			BYTE newBit = ((data3[index] ^ (buffer[0] >> shift)) & mask);

			tmpBuff[index] = (tmpBuff[index] & ~mask) | newBit;

			for(int i=0; i<0xF; i++)
			{
				subData2.data1[i] = (subData2.data1[i] << 1) | (subData2.data1[i+1] >> 7);
			}
			subData2.data1[0xF] = (subData2.data1[0xF] << 1) | (newBit >> (7-shift));
		}
		subData2.type = 3;
	}
	else
	{
		if ( data1.type != 0 )
		{
			function03(data1, 0);
		}

		struct	
		{
			BYTE  u_ch[2][0x10];
			DWORD u_dw[8][4];
		} buffer;
		BYTE  coding[0x10];

		memcpy(&buffer, data1.data3, 0x20 + (size >> 4));

		if (subData2.type == 2)
		{
			memcpy(coding, &subData2.data2, sizeof(coding));
		}
		else
		{
			memset(coding, 0, sizeof(coding));
		}

		DWORD * dwTmpBuff = (DWORD *)tmpBuff;

		for(DWORD i=0; i < size>>7; i++)
		{
			union
			{
				BYTE u_ch[16];
				DWORD u_dw[4];
			} coding1;
			int j;
			for(j=0x10;j--; )
			{
				coding1.u_ch[j] = data3[i*0x10+j] ^ coding[j] ^ buffer.u_ch[0][j];
			}
			for(j=8; j--;)
			{
				DWORD r1 = data1.data4[0][coding1.u_ch[0x0]][0] ^
				data1.data4[0][coding1.u_ch[0x1]][1] ^
				data1.data4[1][coding1.u_ch[0x2]][0] ^
				data1.data4[1][coding1.u_ch[0x3]][1];
				DWORD r2 = data1.data4[0][coding1.u_ch[0x7]][0] ^
				data1.data4[0][coding1.u_ch[0x4]][1] ^
				data1.data4[1][coding1.u_ch[0x5]][0] ^
				data1.data4[1][coding1.u_ch[0x6]][1];

				coding1.u_dw[2] ^= buffer.u_dw[j][2] + r1 + r2;
				coding1.u_dw[2] = (coding1.u_dw[2] >> 1) | (coding1.u_dw[2] << 31);

				coding1.u_dw[3] = (coding1.u_dw[3] << 1) | (coding1.u_dw[3] >> 31);
				coding1.u_dw[3] ^= buffer.u_dw[j][3] + r1 + 2 * r2;

				DWORD r3 = data1.data4[0][coding1.u_ch[0x8]][0] ^
				data1.data4[0][coding1.u_ch[0x9]][1] ^
				data1.data4[1][coding1.u_ch[0xA]][0] ^
				data1.data4[1][coding1.u_ch[0xB]][1];
				DWORD r4 = data1.data4[0][coding1.u_ch[0xF]][0] ^
				data1.data4[0][coding1.u_ch[0xC]][1] ^
				data1.data4[1][coding1.u_ch[0xD]][0] ^
				data1.data4[1][coding1.u_ch[0xE]][1];

				coding1.u_dw[0] ^= buffer.u_dw[j][0] + r3 + r4;
				coding1.u_dw[0] = (coding1.u_dw[0] >> 1) | (coding1.u_dw[0] << 31);

				coding1.u_dw[1] = (coding1.u_dw[1] << 1) | (coding1.u_dw[1] >> 31);
				coding1.u_dw[1] ^= buffer.u_dw[j][1] + r3 + 2 * r4;
			}
			DWORD tmp = coding1.u_dw[0];
			coding1.u_dw[0] = coding1.u_dw[2];
			coding1.u_dw[2] = tmp;
			tmp = coding1.u_dw[1];
			coding1.u_dw[1] = coding1.u_dw[3];
			coding1.u_dw[3] = tmp;

			for(j=0x10;j--; )
			{
				tmpBuff[i*0x10+j] = coding1.u_ch[j] ^ buffer.u_ch[1][j];
			}
			if(subData2.type == 2)
			{
				memcpy(coding, tmpBuff+i*0x10+j, 0x10);
			}
		}
		if(subData2.type == 2)
		{
			memcpy( &subData2.data2, coding, sizeof(subData2.data2));
		}
	}
}

void CCryptNew::InitCrypt( DWORD dwIP )	// 4c4530
{
	dwIP = UNPACKDWORD( ((BYTE*) & dwIP ) );

	InitSubData1(m_subData1, 0, 0x80, NULL );	// 0x4d8820
	InitSubData2(m_subData2, 1, NULL);		//

	m_subData1.data1[0][0].u_dw = m_subData1.data1[0][1].u_dw =
	m_subData1.data1[1][0].u_dw = m_subData1.data1[1][1].u_dw = dwIP;

	function05(m_subData1);	// 4d7be0

	for( int i=0; i<0x100; i++)
	{
		m_subData3[i] = i;
	}

	BYTE tmpBuff[0x100];
	function09(m_subData2, m_subData1, m_subData3, 0x800, tmpBuff); // call 04d8900
	memcpy(m_subData3, tmpBuff, 0x100);

	m_pos = 0;
}

BYTE CCryptNew::CodeNewByte(BYTE code)
{
	if(m_pos == 0x100)
	{
		BYTE tmpBuff[0x100];
		function09(m_subData2, m_subData1, m_subData3, 0x800, tmpBuff);
		memcpy(m_subData3, tmpBuff, 0x100);
		m_pos = 0;
	}
	return code ^= m_subData3[m_pos++];
}


// Standard UO Huffman compression table (from SphereServer 0.56d CEncrypt.cpp)
const WORD CCompressTree::sm_xCompress_Base[COMPRESS_TREE_SIZE] =
{
	0x0002, 0x01f5, 0x0226, 0x0347, 0x0757, 0x0286, 0x03b6, 0x0327,
	0x0e08, 0x0628, 0x0567, 0x0798, 0x19d9, 0x0978, 0x02a6, 0x0577,
	0x0718, 0x05b8, 0x1cc9, 0x0a78, 0x0257, 0x04f7, 0x0668, 0x07d8,
	0x1919, 0x1ce9, 0x03f7, 0x0909, 0x0598, 0x07b8, 0x0918, 0x0c68,
	0x02d6, 0x1869, 0x06f8, 0x0939, 0x1cca, 0x05a8, 0x1aea, 0x1c0a,
	0x1489, 0x14a9, 0x0829, 0x19fa, 0x1719, 0x1209, 0x0e79, 0x1f3a,
	0x14b9, 0x1009, 0x1909, 0x0136, 0x1619, 0x1259, 0x1339, 0x1959,
	0x1739, 0x1ca9, 0x0869, 0x1e99, 0x0db9, 0x1ec9, 0x08b9, 0x0859,
	0x00a5, 0x0968, 0x09c8, 0x1c39, 0x19c9, 0x08f9, 0x18f9, 0x0919,
	0x0879, 0x0c69, 0x1779, 0x0899, 0x0d69, 0x08c9, 0x1ee9, 0x1eb9,
	0x0849, 0x1649, 0x1759, 0x1cd9, 0x05e8, 0x0889, 0x12b9, 0x1729,
	0x10a9, 0x08d9, 0x13a9, 0x11c9, 0x1e1a, 0x1e0a, 0x1879, 0x1dca,
	0x1dfa, 0x0747, 0x19f9, 0x08d8, 0x0e48, 0x0797, 0x0ea9, 0x0e19,
	0x0408, 0x0417, 0x10b9, 0x0b09, 0x06a8, 0x0c18, 0x0717, 0x0787,
	0x0b18, 0x14c9, 0x0437, 0x0768, 0x0667, 0x04d7, 0x08a9, 0x02f6,
	0x0c98, 0x0ce9, 0x1499, 0x1609, 0x1baa, 0x19ea, 0x39fa, 0x0e59,
	0x1949, 0x1849, 0x1269, 0x0307, 0x06c8, 0x1219, 0x1e89, 0x1c1a,
	0x11da, 0x163a, 0x385a, 0x3dba, 0x17da, 0x106a, 0x397a, 0x24ea,
	0x02e7, 0x0988, 0x33ca, 0x32ea, 0x1e9a, 0x0bf9, 0x3dfa, 0x1dda,
	0x32da, 0x2eda, 0x30ba, 0x107a, 0x2e8a, 0x3dea, 0x125a, 0x1e8a,
	0x0e99, 0x1cda, 0x1b5a, 0x1659, 0x232a, 0x2e1a, 0x3aeb, 0x3c6b,
	0x3e2b, 0x205a, 0x29aa, 0x248a, 0x2cda, 0x23ba, 0x3c5b, 0x251a,
	0x2e9a, 0x252a, 0x1ea9, 0x3a0b, 0x391b, 0x23ca, 0x392b, 0x3d5b,
	0x233a, 0x2cca, 0x390b, 0x1bba, 0x3a1b, 0x3c4b, 0x211a, 0x203a,
	0x12a9, 0x231a, 0x3e0b, 0x29ba, 0x3d7b, 0x202a, 0x3adb, 0x213a,
	0x253a, 0x32ca, 0x23da, 0x23fa, 0x32fa, 0x11ca, 0x384a, 0x31ca,
	0x17ca, 0x30aa, 0x2e0a, 0x276a, 0x250a, 0x3e3b, 0x396a, 0x18fa,
	0x204a, 0x206a, 0x230a, 0x265a, 0x212a, 0x23ea, 0x3acb, 0x393b,
	0x3e1b, 0x1dea, 0x3d6b, 0x31da, 0x3e5b, 0x3e4b, 0x207a, 0x3c7b,
	0x277a, 0x3d4b, 0x0c08, 0x162a, 0x3daa, 0x124a, 0x1b4a, 0x264a,
	0x33da, 0x1d1a, 0x1afa, 0x39ea, 0x24fa, 0x373b, 0x249a, 0x372b,
	0x1679, 0x210a, 0x23aa, 0x1b8a, 0x3afb, 0x18ea, 0x2eca, 0x0627,
	0x00d4 // terminator
};
