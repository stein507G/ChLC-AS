#include "config.h"


// 1uS
void delay_us(unsigned int time)
{
	uint32_t i =0 , j = 0;
	for(j=0 ; j<time ; j++)
	{
		for(i=0; i<2; i++) {
			__asm volatile ("nop");
		}
	}
}

void delay_ms(unsigned int time)
{
	uint32_t i;
	for(i=0;i<time;i++)
	{
		delay_us(1000);		// 1uS
	}
}


int AsciiToHex(char c)
{
	int num = toupper(c);
	if(num >= 0x30 && num <= 0x39)
	{
		return num - 0x30;
	}
	if(num >= 'A' && num <= 'F')
	{
		return num - 'A';
	}
	return num;
}

char HexStringToByte(short value )
{
	char lower = AsciiToHex(value & 0xFF);
	char Upper = AsciiToHex( (value >> 8) & 0xFF);
	char nRet = Upper << 4 | lower;
	return nRet;
}

short HexStringToShort(int value)
{
	char lower = HexStringToByte(value & 0xFFFF);
	char Upper = HexStringToByte( (value >> 16) & 0xFFFF);
	short nRet = Upper << 8 | lower;
	return nRet;
}

