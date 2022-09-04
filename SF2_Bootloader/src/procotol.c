
#include "config.h"

#define	MAX_BUFFER	64

#define	WAIT_CHECK_UPDATE_TIME		3000	//ms

#define	STX_SIZE		1
#define	CHECKSUM_SIZE	1
#define	ETX_SIZE		1

static uint32_t g_nCheckStartTime = 0;

unsigned char CalcChkSum(uint8_t* pBuffer, int offset, int size);

#if 0
void uart0_rx_handler(mss_uart_instance_t * this_uart)
{
    uint8_t buffer[MAX_BUFFER] = {0};
    uint32_t nSize  = 0;

    nSize = MSS_UART_get_rx(this_uart, buffer, sizeof(buffer));
    enqueue(buffer, 0, nSize);

	NVIC_ClearPendingIRQ(UART0_IRQn);
}

extern volatile size_t g_tx_size;
extern volatile const uint8_t * g_tx_buffer;

__attribute__((__interrupt__)) void uart_tx_handler(
		mss_uart_instance_t * this_uart)
{
	size_t size_in_fifo;

	if (g_tx_size > 0)
	{
		size_in_fifo = MSS_UART_fill_tx_fifo(this_uart,
				(const uint8_t *) g_tx_buffer, g_tx_size);

		if (size_in_fifo == g_tx_size)
		{
			g_tx_size = 0;
			MSS_UART_disable_irq(this_uart, MSS_UART_TBE_IRQ);
		}
		else
		{
			g_tx_buffer = &g_tx_buffer[size_in_fifo];
			g_tx_size = g_tx_size - size_in_fifo;
		}
	}
	else
	{
		g_tx_size = 0;
		MSS_UART_disable_irq(this_uart, MSS_UART_TBE_IRQ);
	}
}
#endif

int GenPacket(uint8_t* pBuffer, uint16_t cmd, uint8_t flag, uint8_t *pData, int size)
{
	int nPos =0;
	uint16_t chkSum = 0;

	pBuffer[nPos++] = PKT_STX;
	pBuffer[nPos++] = cmd & 0xFF;
	pBuffer[nPos++] = ( cmd >> 8 ) & 0xFF;
	pBuffer[nPos++] = flag;
	pBuffer[nPos++] = size & 0xFF;
	pBuffer[nPos++] = (size >> 8) & 0xFF;

	if (NULL != pData && size > 0) {
		memcpy(&pBuffer[nPos], pData, size);
		nPos += size;
	}

	chkSum = CalcChkSum(pBuffer, 1, nPos - 1);
	pBuffer[nPos++] = chkSum & 0xFF;
	//pBuffer[nPos++] = (chkSum >> 8) & 0xFF;
	pBuffer[nPos++] = PKT_ETX;

	return nPos;
}


int GenHeaderPacket(uint8_t* pBuffer, uint16_t cmd, uint8_t flag, uint8_t *pData, int size)
{
	int nPos =0;
	uint16_t chkSum = 0;

	pBuffer[nPos++] = PKT_STX;
	pBuffer[nPos++] = cmd & 0xFF;
	pBuffer[nPos++] = ( cmd >> 8 ) & 0xFF;
	pBuffer[nPos++] = flag;

	if (NULL != pData && size > 0) {
		pBuffer[nPos++] = size & 0xFF;
		pBuffer[nPos++] = (size >> 8) & 0xFF;

		memcpy(&pBuffer[nPos], pData, size);
		nPos += size;
	}

	return nPos;
}

#if 0
unsigned short CalcChkSum(uint8_t* pBuffer, int offset, int size)
{
	short word16 ,  i;
	unsigned int sum = 0;
	for( i = 0; i < size; i = i+2 )
	{
		word16 = ( ( pBuffer[offset + i]<<8) & 0xFF00 )+( pBuffer[offset + i+1] & 0xFF );
		sum = sum + (unsigned int) word16;
	}

	while( sum >> 16 )
			sum = ( sum & 0xFFFF ) + ( sum >> 16 );

	sum = ~sum;
	return ((u_short) sum);

}
#else
unsigned char CalcChkSum(uint8_t* pBuffer, int offset, int size)
{
	int sum = 0, temp = 0, result = 0;

	int i = 0;
	for (i = 0; i < size; i++) {
		temp = pBuffer[offset + i] & 0xFF;
		sum += temp;
	}
	result = ~(sum & 0xFF) + 1;

	return result;

}
#endif

int CheckPacket(uint8_t* pBuffer, int size)
{
	int nRet = 0;
	unsigned char chkSum = 0, calcChkSum = 0;
	PHEADER_PKT pPktHeader = (PHEADER_PKT) pBuffer;

	if(pPktHeader->Stx == PKT_STX) {
		calcChkSum = CalcChkSum(pBuffer, 1, size - (STX_SIZE + CHECKSUM_SIZE + ETX_SIZE)) ;
		chkSum = (pBuffer[size - 2]  & 0xFF) ;

		if (chkSum != calcChkSum) {
#ifdef __DEBUG_CONSOLE__
			//printf("Invalid CheckSum [%X, %X]\n", chkSum, calcChkSum);
#endif

			nRet = ERROR_INVALID_CHKSUM;
		}

	} else {
		nRet = ERROR_INVALID_STX;
	}

	return nRet;
}

int RecvData( mss_uart_instance_t * this_uart, uint8_t * rx_buff, int buff_size)
{
#define	STX_SIZE		1
#define	CHECKSUM_SIZE	1
#define	ETX_SIZE		1

	int rxSize = 0U;
    int size = 0, hdrSize = sizeof(HEADER_PKT), dataSize = 0;

    do {
    	while ( ((this_uart->hw_reg->LSR) & 0x1) != 0U  )
    	{
    	   rx_buff[rxSize] = this_uart->hw_reg->RBR;
    	   ++rxSize;
    	}

    	if(rxSize == hdrSize) {
    	   PHEADER_PKT pPkt = (PHEADER_PKT) rx_buff;

    	   dataSize = pPkt->Length;
    	   size = hdrSize + dataSize + CHECKSUM_SIZE + ETX_SIZE;
    	}

    	if( rxSize > hdrSize && (rxSize >= size) ) {
    	   if(rx_buff[rxSize - 1] == PKT_ETX)
    		   break;
    	}

    } while ( 0 != rxSize && (diff_mili_time(g_nCheckStartTime) < WAIT_CHECK_UPDATE_TIME));


    return rxSize;
}

bool check_fwpdate()
{

#define	RETRY_CHECK_UPDATE_TIME		500	//ms
#define	PROGRESS_CHECK_UPDATE 		100	//ms

	uint32_t time = 0;
	uint8_t buffer[MAX_BUFFER] = {0};

	bool bRet = false;
	int nSize = 0, nRet = 0;

#ifdef DEBUG_MESSAGE
	MSS_UART_polled_tx_string(get_uart(),(uint8_t *)"Wait 3 second for fw update\n\r");
#endif

	send_update_check();
	g_nCheckStartTime = get_mili_time();
	do {

		nSize = RecvData ( get_uart(), &buffer[nSize], MAX_BUFFER );

		if(nSize > 0) {

			PUPDATE_RESP_PKT pPktResp = (PUPDATE_RESP_PKT)buffer;

			nRet = CheckPacket((uint8_t *)pPktResp, nSize);
			if(nRet >= 0) {

				switch(pPktResp->Cmd) {
					case PKT_CMD_UPDATE_CHECK :
						bRet = true;
						memset(buffer, 0, MAX_BUFFER);
						nSize = 0;
						break;
				}

			} else {

			}
		} else {

			if(0 == (diff_mili_time(g_nCheckStartTime) % RETRY_CHECK_UPDATE_TIME) ) {
				//delay_ms(1000);
				//send_update_check();

			}
		}
#ifdef DEBUG_MESSAGE
		if(0 == ((diff_mili_time(g_nCheckStartTime) % PROGRESS_CHECK_UPDATE))) {
			MSS_UART_polled_tx_string(get_uart(),(uint8_t *)".");
		}
#endif

	} while( !bRet && (diff_mili_time(g_nCheckStartTime) < WAIT_CHECK_UPDATE_TIME) );

#ifdef DEBUG_MESSAGE
	MSS_UART_polled_tx_string(get_uart(),(uint8_t *)"\n\r");
	if(!bRet) {
		MSS_UART_polled_tx_string(get_uart(),(uint8_t *)"Timeout 3 second\n\r");
	}
#endif

	return bRet;
}


int send_update_check()
{
	int nRet = 0;
	UPDATE_COMMON_PKT pkt;

	nRet = GenPacket((uint8_t*)&pkt, PKT_CMD_UPDATE_CHECK, FLAG_SEND, NULL, 0 );
	MSS_UART_polled_tx( get_uart(), (uint8_t*)&pkt, nRet );

	return nRet;
}

int post_prog_run()
{
	int nRet = 0;
	RUN_PROGRAM_T tProgram;
	RUN_PROGRAM_PKT pkt;

	tProgram.Kind = PROGRAM_BL;
	memcpy(tProgram.Version , BL_FIRMWARE_VERSION_STRING, sizeof(BL_FIRMWARE_VERSION_STRING));

	nRet = GenPacket((uint8_t*)&pkt, PKT_CMD_RUN_PROGRAM, FLAG_SEND, (uint8_t*)&tProgram, sizeof(tProgram));
	MSS_UART_polled_tx( get_uart(), (uint8_t*)&pkt, nRet );

	return nRet;
}
