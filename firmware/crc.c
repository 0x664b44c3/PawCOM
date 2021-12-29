#include <inttypes.h>
#include <util/crc16.h>
#include <crc.h>
#define POLY_CRC16 0x8408
/*
//                                      16   12   5
// this is the CCITT CRC 16 polynomial X  + X  + X  + 1.
// This works out to be 0x1021, but the way the algorithm works
// lets us use 0x8408 (the reverse of the bit pattern).  The high
// bit is always assumed to be set, thus we only use 16 bits to
// represent the 17 bit value.
*/


const unsigned char CRC7_POLY = 0x91;

uint16_t crc16(const void *p, uint16_t length)
{
	char* data_p = (char*) p;
	unsigned char i;
	uint16_t  data;
	uint16_t  crc = 0xffff;

	if (length == 0)
		return (~crc);

	do
	{
		for (i=0, data=(uint16_t)0xff & *data_p++;
			 i < 8;
			 i++, data >>= 1)
		{
			if ((crc & 0x0001) ^ (data & 0x0001))
				crc = (crc >> 1) ^ POLY_CRC16;
			else  crc >>= 1;
		}
	} while (--length);

	crc = ~crc;
	data = crc;
	crc = (crc << 8) | (data >> 8 & 0xff);

	return (crc);
}

uint8_t crc8(const void * p, uint16_t length)
{
	uint8_t crc = 0;
	unsigned char *d = (unsigned char *) p;

	for (uint16_t i=0;i<length;++i)
	{
		crc = _crc8_ccitt_update(crc, *d++);
	}
	return crc;
}
