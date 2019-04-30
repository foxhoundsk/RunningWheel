#include "uart.h"
#include "esp8266.h"
#include "main.h"

extern volatile Wifi wifi;
extern volatile Escalator escalator;
extern volatile uint8_t pdata wifiSendBuffer[SEND_BUFFER_SIZE];
extern volatile uint8_t pdata wifiRecvBuffer[RECV_BUFFER_SIZE];
volatile Uart uart;
extern volatile Mcu mcu;
SI_SBIT(LED0, SFR_P1, 4);
/* WARN: TODO: if this func encountered 0(null) in sendbuffer it will treat it as end of data then stop counting data requeste to send */
void uartSend(uint8_t* buffer, uint8_t byteWaiting)
{
	//hard-coded since if dont we can't catch null data 
	uint8_t byteSend = 3, index = 3;

	while (buffer[index] != NULL) /* we don't use strlen cuz its implementation is not guaranteed and this is a safe way to check data size */
	{
		byteSend++;
		index++;	
	}

	if (byteSend > SEND_BUFFER_SIZE)	/* WARN: data requested to send has exceeded send buffer size, and the handler haven't implemented, just note that if your UART didn't send you should check this */
	{
		return;
	}
	
	uart.queuingByte = byteSend;

	if ((byteWaiting > 0) && (byteWaiting <= RECV_BUFFER_SIZE)) /* set the size intend to receive, zero is no receive needed */
	{
		uart.byteWaiting = byteWaiting;
	}
	else
	{
		uart.byteWaiting = 0;
	}

	uart.state = SEND_START; /* TODO (deprecated, this state can be used to indicate that uart is not idling) */
	wifi.state = DATA_SENDING;

	/* uart DAC temp */	uart.Tstate = TX_BUSY;

	//IE_EA = 0;
	//IE |= IE_ES0__BMASK;
	//SCON0_RI = 0;
	SCON0_TI = 1; /* trigger UART0 Tx interrupt */
	//IE_EA = 1;
	//SCON0_TI = 1;
}

void encode_IR_state(void)
{
	
	memset(&wifiSendBuffer, 0xff, 3); // we are sending 3 bytes per communication constantly

	if (p0_1)
		wifiSendBuffer[0] &= 0xfe;
	if (p0_3)
		wifiSendBuffer[0] &= 0xfd;
	if (p0_7)
		wifiSendBuffer[0] &= 0xfb;
	if (p1_0)
		wifiSendBuffer[0] &= 0xf7;
	if (p1_1)
		{	
			wifiSendBuffer[0] &= 0xef;
			//LED0 = ~LED0;
		}
	if (p1_2)
		wifiSendBuffer[0] &= 0xdf;
	if (p1_3)
		wifiSendBuffer[0] &= 0xbf;
	if (p1_5)
		wifiSendBuffer[0] &= 0x7f;
	if (p1_6)
		wifiSendBuffer[1] &= 0xfe;
	if (p1_7)
		wifiSendBuffer[1] &= 0xfd;
	if (p2_0)
		wifiSendBuffer[1] &= 0xfb;
	if (p2_1)
		wifiSendBuffer[1] &= 0xf7;
	if (p2_3)
		wifiSendBuffer[1] &= 0xef;
	if (p2_4)
		wifiSendBuffer[1] &= 0xdf;
	if (p2_5)
		wifiSendBuffer[1] &= 0xbf;
	if (wifiSendBuffer[0] == 0)
				LED0 = ~LED0;
}
/* this is a temporary func to replace wifi module's function, hence some func appear in this func has name prefixed "wifi" */
void uartIsDataQueue(void)
{
	if (wifi.isDataChanged) /* the flag is cleared once DAC data send back */
	{
		/* deprecated due to this version is IR calibration
		if (escalator.mode == NORMAL)
			wifiPosDataEncode();
		else if (escalator.mode == FREE_WAY)
			do {} while(0); // NOP
		*/
		encode_IR_state();
		uartSend(&wifiSendBuffer, UART_DAC_SIZE); // ch size
	}
}

void uartTransmission(void)
{
	switch (uart.Tstate)
	{
		case WAIT_KNOCK_DOOR:			
			if (uart.queuingByte != UART_KNOCK_DOOR_SIZE && uart.queuingByte != 0)	/* prevent truncated data */
			{
				if (wifi.currentTick == 0)
				{
					wifi.currentTick = mcu.sysTick;
				}
				if ((wifi.currentTick + UART_TRUNCATED_WAIT_TIME) <= mcu.sysTick)	/* uart data truncation confirmed */
				{
					uart.queuingByte = 0;
					uart.currentPos = 0;	/* this shall implemented as function since we usually use it to reset problemed uart */
					uart.byteWaiting = UART_KNOCK_DOOR_SIZE;
				}
			}
			break;
		case IDLE:
			//LED0 = 0;
			uartIsDataQueue();
			break;
		case TX_BUSY:

			break;
		case RX_BUSY:
			if ((wifi.currentTick + UART_DAC_MAX_WAIT_TIME) <= mcu.sysTick) /* recv timeout detect */
			{
				uart.Tstate = IDLE;
				memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
				memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
			}
			break;
		case RX_DONE:
			if (uartIsDataKnockDoor())	break;	/* this determination style may implement to esp8266.c */
			if (uartIsEndTrainData())	break;		
			//if (escalator.mode == NORMAL) uartApplyDACData(); deprecated due to this version is IR calibration
			wifi.isDataChanged = 0;
			uart.Tstate = IDLE;
			memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
			memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
			break;
	}
}
/*motor speed*/
void uartApplyDACData(void)
{
	uint8_t savedpage = SFRPAGE;

	SFRPAGE = PG4_PAGE;

	DAC0L = wifiRecvBuffer[UART_DAC_SIZE - 6];
	DAC0H = wifiRecvBuffer[UART_DAC_SIZE - 5];
	DAC1L = wifiRecvBuffer[UART_DAC_SIZE - 4];
	DAC1H = wifiRecvBuffer[UART_DAC_SIZE - 3];
	DAC2L = wifiRecvBuffer[UART_DAC_SIZE - 2];
	DAC2H = wifiRecvBuffer[UART_DAC_SIZE - 1];
	
	SFRPAGE = savedpage;
}

bool uartIsDataKnockDoor(void)	/* with this implementation, data similarity should be considered, AND WARN THAT IF THE PACKET IS MALFORMED*/
{
	if (wifiRecvBuffer[0] == 0x88) /* verify mode bit */
	{
		escalator.mode = FREE_WAY;
		wifiRecvBuffer[0] = 0x52; /* clear mode bit */
	}
	if ((wifiRecvBuffer[0] == 'R') && (wifiRecvBuffer[1] == 'D') && (wifiRecvBuffer[2] == 'Y'))	
	{		
		wifiSendBuffer[0] = 'A';
		wifiSendBuffer[1] = 'C';
		wifiSendBuffer[2] = 'K';
		uartSend(&wifiSendBuffer, NO_DATA_EXPECTED);
		while (uart.state != STANDBY) {}
		memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
		memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
		uart.Tstate = IDLE;	/* training is about to begin */
		wifi.isDataChanged = 0;
		return true;
	}
	/* WARN: with this implementation, we must guarantee that there are no other data sent at KNOCK_DOOR state, otherwise it will make mcu starts training once received some data (TODO) */
	return false;
}

bool uartIsEndTrainData(void)	/* with this implementation, data similarity should be considered */
{
	uint8_t index;
	if ((wifiRecvBuffer[0] == 'E') && (wifiRecvBuffer[1] == 'N') && (wifiRecvBuffer[2] == 'D'))	
	{		
		memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
		wifiSendBuffer[0] = 'A';
		wifiSendBuffer[1] = 'C';
		wifiSendBuffer[2] = 'K';
		uartSend(&wifiSendBuffer, NO_DATA_EXPECTED);
		while (uart.state != STANDBY); /* wait till ACK sent */
		/* reset state for get ready for next training*/
		uart.queuingByte = 0;
		uart.byteWaiting = UART_KNOCK_DOOR_SIZE;
		uart.Tstate = WAIT_KNOCK_DOOR;
		uart.state = STANDBY;
		/* re-init training args and reset DAC---- */		
		escalator.intervalFlag = 0;
    	for (index = 0; index < 3; index++)
    	{
    	    escalator.arm[index].variability[0] = 0;
    	    escalator.arm[index].variability[1] = 0;
    	    escalator.arm[index].variability[2] = 0;
    	    escalator.arm[index].variability[3] = 0;
			escalator.arm[index].variability[4] = 0;
    	    escalator.arm[index].lastPos = POS_INIT;
    	    escalator.arm[index].currentPos = 0;
    	}
		index = SFRPAGE; /* we continued use index to reduce memory usage and index here is simple enough */
		SFRPAGE = PG4_PAGE;
    	DAC0L = 0x0;
		DAC0H = 0x00;
		DAC1L = 0x0;
		DAC1H = 0x00;
		DAC2L = 0x0;
		DAC2H = 0x00;
		SFRPAGE = index;
		wifi.currentTick = mcu.sysTick;
		while ((wifi.currentTick + DAC_APPLY_TIME) >= mcu.sysTick);
		wifi.currentTick = 0;
		escalator.mode = NORMAL;
		/*--------------------------*/
		memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
		memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
		/*----------------------------------------------*/
		return true;
	}
	return false;
}
void uartInit(void)
{
	uart.state = STANDBY;
	uart.currentPos = 0;
	uart.queuingByte = 0; 
	uart.byteWaiting = UART_KNOCK_DOOR_SIZE;

	uart.Tstate = WAIT_KNOCK_DOOR;
	SCON0 |= SCON0_REN__RECEIVE_ENABLED; /* enable this permanently due to we need to receive training end notification during training from PC, wifi shall need this style */
}

/*************flawless0714 * END OF FILE****/
