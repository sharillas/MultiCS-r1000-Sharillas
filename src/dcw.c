#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdint.h>

extern unsigned long crc32(unsigned long crc, const unsigned char *buf, unsigned int len);

// ============================================================
// FILTRO EMBUTIDO (NAO CONFIGURAVEL)
// Existe um servidor (multics modificado) que cria CWs falsos
// alterando o ULTIMO byte do CW com XOR 0xF0 e envia-os a todos
// os peers. Este filtro vem compilado na build e nao e configuravel.
// ============================================================

// assinatura do fake: ultimo byte == (soma dos 3 anteriores) XOR 0xF0
inline int isfakecw_xorF0(uint8_t *data)
{
	return ( data[15] == (uint8_t)(((data[12] + data[13] + data[14]) & 0xFF) ^ 0xF0) );
}

// CW viaccess nano e0 (caid 0500): os 2 ultimos bytes sao o CRC32
// (top 16 bits, big-endian) dos primeiros 14 bytes - como no oscam
inline int acceptDCW_nanoe0(uint8_t *data)
{
	uint32_t crc = (uint32_t)crc32(0L, data, 14);
	if ( data[14] != (uint8_t)(crc>>24) ) return 0;
	if ( data[15] != (uint8_t)((crc>>16)&0xFF) ) return 0;
	return 1;
}


inline int checksumDCW(uint8_t *data)
{
    if(data[3] != (uint8_t)((data[0] + data[1] + data[2]) & 0xFF) || data[7] != (uint8_t)((data[4] + data[5] + data[6]) & 0xFF)) {
      return 0;
    }
    if(data[11] != (uint8_t)((data[8] + data[9] + data[10]) & 0xFF) || data[15] != (uint8_t)((data[12] + data[13] + data[14]) & 0xFF)) {
      return 0;
    }
    return 1;
}


inline int isnullDCW(uint8_t *data)
{
	int a0 = ( !data[0] && !data[1] && !data[2] );
	int a1 = ( !data[4] && !data[5] && !data[6] );
	int b0 = ( !data[8] && !data[9] && !data[10] );
	int b1 = ( !data[12] && !data[13] && !data[14] );
	return ( (a0||a1) && (b0||b1) );
}

inline int isbadDCW(uint8_t *data)
{
	if ( data[0]!=0 && data[0]==data[1] && data[0]==data[2] ) return 1;
	if ( data[4]!=0 && data[4]==data[5] && data[4]==data[6] ) return 1;
	if ( data[8]!=0 && data[8]==data[9] && data[8]==data[10] ) return 1;
	if ( data[12]!=0 && data[12]==data[13] && data[12]==data[14] ) return 1;
	return 0;
}

int acceptDCWnonblockCRC(uint8_t *data)
{
        if ( isnullDCW(data) ) return 0;
        if ( isbadDCW(data) ) return 0;
        // FILTRO EMBUTIDO: fake cw com ultimo byte XOR 0xF0
        if ( isfakecw_xorF0(data) ) {
                mlogf(LOGWARNING,0," !!! fake cw detected (last byte XOR 0xF0) %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8],data[9],data[10],data[11],data[12],data[13],data[14],data[15] );
                return 0;
        }
        // nano e0 (CRC32) OU checksum normal - ambos validos sao aceites
        if ( acceptDCW_nanoe0(data) ) return 1;
        if ( checksumDCW(data) ) return 1;
/*
        struct dcw_data *dcw = cfg.bad_dcw;
        while (dcw) {
                if ( dcwcmp16(dcw->dcw, data) ) return 0;
                dcw = dcw->next;
        }
*/
        return 0;
}



int acceptDCW(uint8_t *data, int isnanoe0)
{
	// FILTRO EMBUTIDO: fake cw com ultimo byte XOR 0xF0
	if ( isfakecw_xorF0(data) )
	{
		mlogf(LOGWARNING,0," !!! fake cw detected (last byte XOR 0xF0) %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8],data[9],data[10],data[11],data[12],data[13],data[14],data[15] );
		return 0;
	}
	if(!isnanoe0) {
		if (!checksumDCW(data))
		{
			mlogf(LOGTRACE,0," !!! Bad cw checksum and isnanoe0 is NOT active %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8],data[9],data[10],data[11],data[12],data[13],data[14],data[15] );
			return 0;
		}
	}
	else
	{
		// nano e0: valida com o CRC32 dos ultimos 2 bytes (ou checksum
		// normal para cws 0500 que nao sejam realmente nano e0)
		if ( !acceptDCW_nanoe0(data) && !checksumDCW(data) )
		{
			mlogf(LOGWARNING,0," !!! bad nano e0 cw (crc) %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8],data[9],data[10],data[11],data[12],data[13],data[14],data[15] );
			return 0;
		}
	}

	if ( isnullDCW(data) ) return 0;
	if ( isbadDCW(data) ) return 0;
/*
	struct dcw_data *dcw = cfg.bad_dcw;
	while (dcw) {
		if ( dcwcmp16(dcw->dcw, data) ) return 0;
		dcw = dcw->next;
	}
*/
	return 1;
}

int similarcw( uint8_t *cw1, uint8_t *cw2 )
{
	int i;
	int count = 0;
	for(i=0; i<8; i++) if (cw1[i]==cw2[i]) count++;
	if (count>3) return 1;
	return 0;
}

// for nds
int ishalfnulledcw( uint8_t dcw[16] )
{
	char nullcw[8] = "\0\0\0\0\0\0\0\0";
	if ( !memcmp(dcw,nullcw,8) || !memcmp(dcw+8,nullcw,8) ) return 1;
	return 0;
}

