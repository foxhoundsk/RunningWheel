/* Library for 8051, but it's for EFM8LB12F64E-QFN32 specifically, hence other 8051 may have different
** implement mechanism */

/*  NOTE
*	1. The maximum packet wait time is 2 seconds(PACKET_WAIT_TIME).
*	2. The bug which cause recvBuffer has unexpected byte '\r' '\n' at first maybe is that SBUF0 should do a clean after transmission is complete, more info should go to spec.
*
*
*/
#include "esp8266.h"

#define NUM_SCANS 3 /* ADC use */

volatile Wifi wifi;
extern volatile Mcu mcu;
extern volatile Uart uart;
extern volatile Escalator escalator;

volatile uint8_t pdata wifiSendBuffer[SEND_BUFFER_SIZE] = {0};
volatile uint8_t pdata wifiRecvBuffer[RECV_BUFFER_SIZE] = {0};

/* Start from arm[0], is arm from left to right */
void wifiPosDataEncode(void) /* we are going to deprecate POS_5of5 since its value is too close, maybe is sample rate or resolution, but on volt meter it got almost same value */
{
	uint8_t index = 0;
	wifiSendBuffer[index++] = (escalator.arm[0].currentPos + 1); /* +1 is used to send actual pos, ex 1 at pos 1, 2 at pos 2, or it may be 3 at pos 4 */
	wifiSendBuffer[index++] = (escalator.arm[1].currentPos + 1); 
	wifiSendBuffer[index++] = (escalator.arm[2].currentPos + 1); 
}

void wifiInit(void)
{
	wifi.errorCount = 0;
	wifi.isDataChanged = 0;
	wifi.state = INIT;
	wifi.currentTick = 0;
}

// since this section is eating out SRAM, I turn it off. define it if you want to use in the future
#ifdef WIFI

/* @brief	Apply DAC data received from PC */
void wifiApplyDACdata(void)
{
	uint8_t savedpage = SFRPAGE;

	SFRPAGE = PG4_PAGE;

	DAC0L = wifiRecvBuffer[WIFI_DAC_DATA_SIZE - 6]; /* first byte PC sent */
	DAC0H = wifiRecvBuffer[WIFI_DAC_DATA_SIZE - 5];
	DAC1L = wifiRecvBuffer[WIFI_DAC_DATA_SIZE - 4];
	DAC1H = wifiRecvBuffer[WIFI_DAC_DATA_SIZE - 3];
	DAC2L = wifiRecvBuffer[WIFI_DAC_DATA_SIZE - 2];
	DAC2H = wifiRecvBuffer[WIFI_DAC_DATA_SIZE - 1];
	
	SFRPAGE = savedpage;
}

void wifiProcess(void)
{
	switch (wifi.state)
	{
		case INIT:
			wifiModuleInit();
			break;
		case RECV_CHECK:
			wifiRecvCheck();
			break;
		case DATA_SENDING:
			if (uart.state == RECV_DONE) /* RECV_DONE should be detected first and then SEND_DONE or RECV_DONE will never entered */
			{
				//IE &= ~IE_ES0__BMASK;
				/*
				IE_EA = 0;
            	IE &= ~IE_ES0__BMASK;
            	IE_EA = 1;
				*/
				wifi.state = RECV_CHECK;
				uart.state = STANDBY;
			}	
			else if (uart.state == SEND_DONE)
			{
				if (((wifi.currentTick + PACKET_WAIT_TIME) <= mcu.sysTick) && (wifi.Cstate < MODULE_RUNNING)) /* INIT state receive timeout detector */
				{					
					wifi.state = INIT;
					uart.state = STANDBY;
					//IE_EA = 0;
            		//IE &= ~IE_ES0__BMASK;
					SCON0 &= ~SCON0_REN__RECEIVE_ENABLED;
            		//IE_EA = 1;
					//for (index = 0; index < SEND_BUFFER_SIZE; index++)	wifiSendBuffer[index] = 0; /* (not sure) since memset seems got some implementation problem */
					//for (index = 0; index < RECV_BUFFER_SIZE; index++)	wifiRecvBuffer[index] = 0;
					memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE); /* this two line should move to end of this func and put return to if statement above */
					memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
					return;
				}
				else if ((wifi.currentTick + PACKET_WAIT_TIME) <= mcu.sysTick && wifi.Cstate == MODULE_RUNNING) /* this is in normal training state, and the module didn't respond */
				{
					wifi.state = RUNNING_TRAINING;	/* this state should at normal training state and no response is expected */
					uart.state = STANDBY;
					//IE_EA = 0;
            		//IE &= ~IE_ES0__BMASK;
					SCON0 &= ~SCON0_REN__RECEIVE_ENABLED;
            		//IE_EA = 1;
					memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
					memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
					return;
				}
				else if ((wifi.currentTick + PACKET_WAIT_TIME) <= mcu.sysTick && wifi.Cstate == MODULE_UNCONNECT) /* this is in normal training state, and the module didn't respond */
				{
					wifi.state = RUNNING_UNCONNECT;	/* this state should at normal training state and no response is expected */
					uart.state = STANDBY;
					//IE_EA = 0;
            		//IE &= ~IE_ES0__BMASK;
					SCON0 &= ~SCON0_REN__RECEIVE_ENABLED;
            		//IE_EA = 1;
					memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
					memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
					return;
				}
				else if ((wifi.currentTick + PACKET_WAIT_TIME + 2000) <= mcu.sysTick && wifi.Cstate == MODULE_CONNECTED_KNOCKING) /* this is in normal training state, and the module didn't respond */
				{
					SCON0 &= ~SCON0_REN__RECEIVE_ENABLED;
					wifi.state = RUNNING_CONNECTED;	/* this state should at normal training state and no response is expected */
					uart.state = STANDBY;
					uart.byteWaiting = 0;
					uart.currentPos = 0;
					//IE_EA = 0;
            		//IE &= ~IE_ES0__BMASK;
            		//IE_EA = 1;
					memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
					memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
					return;
				}
				else if ((wifi.currentTick + PACKET_WAIT_TIME) <= mcu.sysTick && wifi.Cstate == MODULE_RUNNING_POS_SEND) /* this is in normal training state, and the module didn't respond */
				{
					SCON0 &= ~SCON0_REN__RECEIVE_ENABLED;
					wifi.state = RUNNING_TRAINING;	/* this state should at normal training state and no response is expected */
					wifi.Cstate = MODULE_SEND_DATA_ATCOMMAND;
					uart.state = STANDBY;
					uart.byteWaiting = 0;
					uart.currentPos = 0;						
					memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
					memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
					return;
				}
				else if ((wifi.currentTick + PACKET_WAIT_TIME) <= mcu.sysTick && wifi.Cstate == MODULE_SEND_DATA_ATCOMMAND) /* this is in normal training state, and the module didn't respond */
				{
					SCON0 &= ~SCON0_REN__RECEIVE_ENABLED;
					wifi.state = RUNNING_TRAINING;	/* this state should at normal training state and no response is expected */
					uart.state = STANDBY;
					uart.byteWaiting = 0;
					uart.currentPos = 0;						
					memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
					memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
					return;
				}
				else if ((wifi.currentTick + PACKET_WAIT_TIME) <= mcu.sysTick && wifi.Cstate == MODULE_PARSE_DAC_DATA) /* this is in normal training state, and the module didn't respond */
				{
					SCON0 &= ~SCON0_REN__RECEIVE_ENABLED;
					wifi.state = RUNNING_TRAINING;	/* this state should at normal training state and no response is expected */
					wifi.Cstate = MODULE_SEND_DATA_ATCOMMAND;
					uart.state = STANDBY;
					uart.byteWaiting = 0;
					uart.currentPos = 0;						
					memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
					memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
					return;
				}
			}
			/*
			else if (uart.state == STANDBY) /* no recv check needed since there is no reponse this time 
			{
				memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
				wifi.state = RUNNING_TRAINING; /* turn to this state due to we have no no reponse send at INIT state 
			}
			*/
			/* TODO: a maximmum wait time should implement to prevent dead-end */
			break;
		case RUNNING_UNCONNECT:
			if ((wifi.currentTick + TYPICAL_WAIT_TIME + 500) >= mcu.sysTick)	return;
			wifiCommandEncode(AT_CIPSTART, "0,\"UDP\",\"192.168.4.2\",62222,3232,2");
			uartSend(&wifiSendBuffer, DEVICE_CONN_ESTABLISH_SIZE);
			break;
		case RUNNING_CONNECTED:
			wifi.currentTick = mcu.sysTick;
			uart.byteWaiting = KNOCK_RECEIVE_SIZE;
			wifi.state = DATA_SENDING;
			uart.state = SEND_DONE;	/* cheat interrupt since we need to go to recv directly */
			SCON0 |= SCON0_REN__RECEIVE_ENABLED;
			break;
		case RUNNING_TRAINING: /* we also parse DAC data and apply it inside if condition, in the other word,  */
			if (wifi.Cstate == MODULE_RUNNING_POS_SEND)
			{
				wifiPosDataEncode(); /* WARN: another reason to do "pos + 1" is that '0' cause uartSend stop parse sendbuffer */
				uartSend(&wifiSendBuffer, WIFI_DAC_DATA_SIZE);
				break;
			}
			if (wifi.isDataChanged == 1)
			{								
				wifiCommandEncode(AT_CIPSEND, "0,4"); /* TODO: we can reduce to 1 byte since we only want to represent pos from 1~4, but be aware of "pos + 1" at pos encode func */
				uartSend(&wifiSendBuffer, CIPSEND_START);
			}			
			break;
		case RUNNING_TRAINING_DONE:
			/* TODO: PC-end shall send a training done packet, then we enter this state (NOT DONE YET) */
			break;
	}	
}


/* TODO: WARN: command uses this func to encode should check if size of it exceeded size of sendbuffer, and either command or option can't has \0 since the func treat it as end of data */
void wifiCommandEncode(uint8_t* command, uint8_t* option)
{
	uint8_t index = 0, index2 = 0;

	while (command[index] != NULL)
	{
		wifiSendBuffer[index2] = command[index];
		index2++;
		index++;
	}
	index = 0;
	if (option[index] != NULL)
	{
		wifiSendBuffer[index2++] = '=';
		while (option[index] != NULL)
		{
			wifiSendBuffer[index2] = option[index];
			index2++;
			index++;
		}
		wifiSendBuffer[index2++] = '\r';
		wifiSendBuffer[index2++] = '\n';
		wifiSendBuffer[index2++] = '\0';
	}
	else
	{
		wifiSendBuffer[index2++] = '\r';
		wifiSendBuffer[index2++] = '\n';
		wifiSendBuffer[index2++] = '\0';
	}
}

void wifiRecvCheck(void)
{
	uint8_t index = 0;
	switch (wifi.Cstate) /* here exist some magic numbers which are buffer index offset */
	{
		case RST: 
			while (index < RST_RES_SIZE - 1) /* -1 is to prevent illegal access */
			{
				if (wifiRecvBuffer[index] == 'O' && wifiRecvBuffer[index + 1] == 'K')
				{
					wifi.Cstate++;
					wifi.state = INIT;
					return; /* check successed */
				}
				index++;
			}
			if (wifi.errorCount == 0)
			{
				wifi.errorCount++;
				wifi.state = INIT; /* back to wifiModuleInit() to try again */
			}
			else /* one time error tolerance */
			{
				wifi.errorCount = 0;
				wifi.state = INIT;
				wifi.Cstate = RST;
			}		
			break;
		case ATE0:
			if (wifiRecvBuffer[ECHO_TURN_OFF_EDGE - 3] == 'O' && wifiRecvBuffer[ECHO_TURN_OFF_EDGE - 2] == 'K')
			{
				wifi.Cstate++;
				wifi.state = INIT;
			}
			else if (wifi.errorCount == 0)
			{
				wifi.errorCount++;
				wifi.state = INIT; /* back to wifiModuleInit() to try again */
			}
			else /* one time error tolerance */
			{
				wifi.errorCount = 0;
				wifi.state = INIT;
				wifi.Cstate = RST;
			}
			break;
		case CWMODE:
			wifiTypicalPacketCheck();		
			break;
		case CWSAP:
			wifiTypicalPacketCheck();
			break;
		case CIPMUX:
			wifiTypicalPacketCheck();
			break;
		case CIPCLOSE:
			wifiTypicalPacketCheck();
			break;
		case CWDHCP:
			wifiTypicalPacketCheck();
			break;
		case CWLIF:
			while (index < TYPICAL_RECEIVE_SIZE)
			{
				if (wifiRecvBuffer[index] == '1')
				{
					wifi.Cstate = MODULE_UNCONNECT;
					wifi.state = RUNNING_UNCONNECT;
					memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
					memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
					return;
				}
				index++;
			}		
			if (wifi.state != RUNNING_TRAINING) /* check doesn't success */
			{
				wifi.state = INIT;
			}
			break;
		case MODULE_UNCONNECT:
			/* ref to stm32 */
			/* NOT SURE WHY FIRST CHAR IS '6', INTERRUPT FLAG?, IVE CLEAREED THE BUFFER ,HENCE I REDUCE THE OFFSET FROM 4 TO 3 */if (wifiRecvBuffer[DEVICE_CONN_ESTABLISH_SIZE - 3] == 'O' || wifiRecvBuffer[DEVICE_CONN_ESTABLISH_SIZE - 2] == 'K')
			{
				wifi.Cstate = MODULE_CONNECTED_KNOCKING;
				wifi.state = RUNNING_CONNECTED;
			}
			else 
			{
				wifi.state = RUNNING_UNCONNECT;
			}
			break;
		case MODULE_CONNECTED_KNOCKING: //KNOCK_RECEIVE_SIZE
			if (wifiRecvBuffer[KNOCK_RECEIVE_SIZE - 2] == 'r' || wifiRecvBuffer[KNOCK_RECEIVE_SIZE - 1] == 'd')
			{
				wifi.Cstate = MODULE_SEND_DATA_ATCOMMAND;
				wifi.state = RUNNING_TRAINING;
			}
			else 
			{
				wifi.state = RUNNING_CONNECTED;
			}
			break;
		case MODULE_RUNNING_POS_SEND: /* in this state, we parse and apply DAC data sent from PC */ /* AFTER POS DATA SENT, PC-END SHOULD SEND CORRESPONING DAC SPEED BACK */			
			wifiApplyDACdata();			
			wifi.isDataChanged = 0; /* this flag is cleared since whole operation of POS send recv has done properly */
			wifi.state = RUNNING_TRAINING;
			wifi.Cstate = MODULE_SEND_DATA_ATCOMMAND;
			break;
		case MODULE_SEND_DATA_ATCOMMAND: /* TODO: here we should check for "OK \r\n >" */
			if ((mcu.sysTick - wifi.currentTick) <= 50) /* this is a 50ms non-blocking delay to delay command send between the wifi module and mcu */
				return;
			wifi.state = RUNNING_TRAINING;
			wifi.Cstate = MODULE_RUNNING_POS_SEND;
			break;		
	}
	memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
	memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
}

/* magic number in this context is packet offset. And we only check first word which is 'O' for receive check since if failed there is no 'O' */
void wifiTypicalPacketCheck(void)
{
	uint8_t index = 0;

	while (index < TYPICAL_RECEIVE_SIZE - 1) /* -1 is to prevent illegal access */
	{
		if (wifiRecvBuffer[index] == 'O' && wifiRecvBuffer[index + 1] == 'K')
		{
			wifi.Cstate++;
			wifi.state = INIT;
			return; /* check successed */
		}
		index++;
	}
	if (wifi.errorCount == 0)
	{
		wifi.errorCount++;
		wifi.state = INIT; /* back to wifiModuleInit() to try again */
	}
	else /* one time error tolerance then restart init sequence */
	{
		wifi.errorCount = 0;
		wifi.state = INIT;
		wifi.Cstate = RST;
	}
}

void wifiModuleInit(void)
{
	switch (wifi.Cstate)
	{
		case RST: /* since reset cause too much data to receive, hence we only receive the first 15 bytes */
			if ((wifi.currentTick + RST_WAIT_TIME) >= mcu.sysTick)	return; /* hardware reset delay wait */			
			wifiCommandEncode(AT_RST, NULL);
			uartSend(&wifiSendBuffer, RST_RES_SIZE);
			break;
		case ATE0:
			if ((wifi.currentTick + RST_WAIT_TIME) >= mcu.sysTick)	return;
			wifiCommandEncode(AT_NO_ECHO, NULL);
			uartSend(&wifiSendBuffer, ECHO_TURN_OFF_EDGE);
			break;
		case CWMODE:
			if ((wifi.currentTick + TYPICAL_WAIT_TIME) >= mcu.sysTick)	return;
			wifiCommandEncode(AT_CWMODE, "2");
			uartSend(&wifiSendBuffer, TYPICAL_RECEIVE_SIZE);
			break;
		case CWSAP:
			if ((wifi.currentTick + TYPICAL_WAIT_TIME + 1000) >= mcu.sysTick)	return; /*this may take longer since this is wifi configuration */
			wifiCommandEncode(AT_CWSAP, "\"cLab-Escalator\",\"00001111\",9,3,2,0");
			uartSend(&wifiSendBuffer, TYPICAL_RECEIVE_SIZE);
			break;
		case CIPMUX:
			if ((wifi.currentTick + TYPICAL_WAIT_TIME) >= mcu.sysTick)	return;
			wifiCommandEncode(AT_CIPMUX, "1");
			uartSend(&wifiSendBuffer, TYPICAL_RECEIVE_SIZE);
			break;
		case CIPCLOSE:
			if ((wifi.currentTick + TYPICAL_WAIT_TIME) >= mcu.sysTick)	return;
			wifiCommandEncode(AT_CIPCLOSE, "5");
			uartSend(&wifiSendBuffer, TYPICAL_RECEIVE_SIZE);
			break;
		case CWDHCP:
			if ((wifi.currentTick + TYPICAL_WAIT_TIME) >= mcu.sysTick)	return;
			wifiCommandEncode(AT_CWDHCP, "0,1");
			uartSend(&wifiSendBuffer, TYPICAL_RECEIVE_SIZE);
			break;
		case CWLIF:
			if ((wifi.currentTick + TYPICAL_WAIT_TIME + 1500) >= mcu.sysTick)	return; /* this should wait much longer since sometimes device doesn't connected that fast */
			wifiCommandEncode(AT_CWLIF, NULL);
			uartSend(&wifiSendBuffer, TYPICAL_RECEIVE_SIZE); /* use this receive size to capture '1' of '192' and "OK" */
			break; 
	}
	//LED0 = ~LED0;
}

#endif

/*************flawless0714 * END OF FILE****/
