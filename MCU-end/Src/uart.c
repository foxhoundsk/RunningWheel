#include "uart.h"
#include "esp8266.h"
#include "main.h"

extern volatile Wifi wifi;
extern volatile Escalator escalator;
extern volatile uint8_t pdata wifiSendBuffer[SEND_BUFFER_SIZE];
extern volatile uint8_t pdata wifiRecvBuffer[RECV_BUFFER_SIZE];
extern const uint16_t pdata dac_speed_table[11];
extern volatile uint16_t tick_in_sec;
extern volatile uint8_t pdata ssv_lv_idx_L;
extern volatile uint8_t pdata ssv_lv_idx_M;
extern volatile uint8_t pdata ssv_lv_idx_R;
extern volatile uint8_t pdata L_emer_flag, M_emer_flag, R_emer_flag;

volatile Uart uart;
extern volatile Mcu mcu;

volatile Successive_tv pdata ssv_tv;

SI_SBIT(LED0, SFR_P1, 4);

/* WARN: TODO: if this func encountered 0(null) in sendbuffer it will treat it as end of data then stop counting data requeste to send */
/* UART send function
 * @buffer: pointer to send buffer
 * @byteWaiting: bytes expect to received after this transmission
 */
void uartSend(uint8_t* buffer, uint8_t byteWaiting)
{
	uint8_t byteSend = 0, index = 0;

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
/* this is a temporary func to replace wifi module's function, hence some func appears in this func has name prefixed with "wifi" */
void uartIsDataQueue(void)
{
	if (wifi.isDataChanged) /* the flag is cleared once DAC data send back */
	{
		if (escalator.mode == NORMAL || escalator.mode == SUCCESSIVE)
			wifiPosDataEncode();

		uartSend(&wifiSendBuffer, UART_DAC_SIZE);
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
					wifi.currentTick = mcu.sysTick; /* FIXED: update wifi tick to prevent keep entering this statement once error occured */
				}
			}
			break;
		case IDLE:
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
		case END_IN_PROGRESS:
			/* this state is intend to make a grace period to let the MCU receive
			   the data of end training notification properly, without stay in
			   this state, if the GUI send end data and DAC data concurrently, MCU
			   may messed up by handling these data, so we stay here to receive
			   end data silently. 
			   
			   note: for more information, please refer to UART ISR at interrupt.c
			*/
			break;
		case RX_DONE:
			if (uartIsDataKnockDoor())	break;	/* this determination style may implement to esp8266.c */
			if (uartIsEndTrainData())	break;		
			if (escalator.mode == NORMAL) uartApplyDACData();
			wifi.isDataChanged = 0;
			uart.Tstate = IDLE;
			memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
			memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
			break;
	}
}

/* Apply motor speed */
void uartApplyDACData(void)
{
	uint8_t savedpage = SFRPAGE;

	SFRPAGE = PG4_PAGE;

	DAC0L = wifiRecvBuffer[UART_DAC_SIZE - 6] & 0x0fff;
	DAC0H = wifiRecvBuffer[UART_DAC_SIZE - 5] & 0x0fff;
	DAC1L = wifiRecvBuffer[UART_DAC_SIZE - 4] & 0x0fff;
	DAC1H = wifiRecvBuffer[UART_DAC_SIZE - 3] & 0x0fff;
	DAC2L = wifiRecvBuffer[UART_DAC_SIZE - 2] & 0x0fff;
	DAC2H = wifiRecvBuffer[UART_DAC_SIZE - 1] & 0x0fff;
	
	SFRPAGE = savedpage;
}

bool uartIsDataKnockDoor(void)	/* with this implementation, data similarity should be considered, AND WARN THAT IF THE PACKET IS MALFORMED*/
{
	uint8_t savedpage;
	if (wifiRecvBuffer[0] == 0x88) /* verify mode bit */
	{
		escalator.mode = FREE_WAY;
		wifiRecvBuffer[0] = 0x52; /* clear mode bit */
	}
	else if (wifiRecvBuffer[0] == 0x48)
	{
		escalator.mode = SUCCESSIVE;
		wifiRecvBuffer[0] = 0x52; /* clear mode bit, in the other hand, set it back to 'R' */

		// store time interval of successive mode
		ssv_tv.L_wheel = wifiRecvBuffer[2];
		ssv_tv.L_wheel <<= 8;
		ssv_tv.L_wheel |= wifiRecvBuffer[1];

		ssv_tv.M_wheel = wifiRecvBuffer[4];
		ssv_tv.M_wheel <<= 8;
		ssv_tv.M_wheel |= wifiRecvBuffer[3];

		ssv_tv.R_wheel = wifiRecvBuffer[6];
		ssv_tv.R_wheel <<= 8;
		ssv_tv.R_wheel |= wifiRecvBuffer[5];

		ssv_lv_idx_L = 2;
		ssv_lv_idx_M = 2;
		ssv_lv_idx_R = 2;

		savedpage = SFRPAGE;
		SFRPAGE = PG4_PAGE;

		// start from speed level 1
		DAC0L = 0x74;
		DAC0H = 0x01;
		DAC1L = 0x74;
		DAC1H = 0x01;
		DAC2L = 0x74;
		DAC2H = 0x01;
		
		SFRPAGE = savedpage;

		// EMER flags
		L_emer_flag = 0;
		M_emer_flag = 0;
		R_emer_flag = 0;

		mcu.sysTick = 0; // we need a clock start from zero to do time calculation
		tick_in_sec = 1; // start from 1 due to 1000ms ~ 0ms count as 0 when modulo with 1000
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
		return true;
	}
	/* WARN: with this implementation, we must guarantee that there are no other data sent at KNOCK_DOOR state, otherwise it will make mcu starts training once received some data (TODO) */
	return false;
}

bool uartIsEndTrainData(void)	/* with this implementation, data similarity should be considered */
{
	uint8_t index;
	if ((wifiRecvBuffer[0] == /*0x7E*/ '~') && (wifiRecvBuffer[1] == 'N') && (wifiRecvBuffer[2] == 'D'))	
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
		wifi.isDataChanged = 0;

		if (escalator.mode == SUCCESSIVE)
		{
			ssv_tv.L_wheel = 0;
			ssv_tv.L_wheel = 0;
			ssv_tv.L_wheel = 0;

			ssv_tv.M_wheel = 0;
			ssv_tv.M_wheel = 0;
			ssv_tv.M_wheel = 0;

			ssv_tv.R_wheel = 0;
			ssv_tv.R_wheel = 0;
			ssv_tv.R_wheel = 0;
		}

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
