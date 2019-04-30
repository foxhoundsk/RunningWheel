#include "Escalator.h"
#include "esp8266.h"


volatile Escalator escalator;
extern volatile uint8_t CONVERSION_COMPLETE;
extern volatile uint8_t pdata wifiSendBuffer[SEND_BUFFER_SIZE];
extern volatile Wifi wifi;
extern volatile Uart uart;




/* TODO: if very early state the escalator should run? */
void escalatorProcess(void)
{   
    uint8_t index; /* for loop and array index use (WARN) */
    int8_t index2;
	/*	WARN: debug use (known issue: this section should uncommented at release version(wifi ver), it prevent this func(escalatorProcess()) run before wifi module done its initialization and ready to send training data, although it is harmless) */
    if (uart.Tstate == WAIT_KNOCK_DOOR)
    {
        return;
    }
    
    if (escalator.mode == FREE_WAY)
    {
        for (index = 0; index < 3; index++)
        {
            switch (index)
            {
                case 0:
                    if (escalator.arm[0].currentPos == 0)
                    {
                        if (!p0_1)
                            escalator.arm[0].currentPos++;
                    }
                    else if (escalator.arm[0].currentPos == 1)
                    {
                        if (p0_1)
                        {
                            escalator.arm[0].currentPos = 0;
                            wifiSendBuffer[0] = 0x80; /* TODO: this was suppose to make no 0x0 byte to send, since our firmware got bug, which can't send 0x0(bug at uart.c, uartSend()), since it's related with infrastructure, we don't fix this ASAP */
                            wifiSendBuffer[1] = 0x40;
                            wifiSendBuffer[2] = 0x40;
                            wifi.isDataChanged = 1;                            
                        }
                    }
                    break;
                case 1:
                    if (escalator.arm[1].currentPos == 0)
                    {
                        if (!p1_2)
                            escalator.arm[1].currentPos++;
                    }
                    else if (escalator.arm[1].currentPos == 1)
                    {
                        if (p1_2)
                        {
                            escalator.arm[1].currentPos = 0;
                            wifiSendBuffer[0] = 0x40;
                            wifiSendBuffer[1] = 0x80;
                            wifiSendBuffer[2] = 0x40;
                            wifi.isDataChanged = 1;                            
                        }
                    }
                    break;
                case 2:
                    if (escalator.arm[2].currentPos == 0)
                    {
                        if (!p2_0)
                            escalator.arm[2].currentPos++;
                    }
                    else if (escalator.arm[2].currentPos == 1)
                    {
                        if (p2_0)
                        {
                            escalator.arm[2].currentPos = 0;
                            wifiSendBuffer[0] = 0x40;
                            wifiSendBuffer[1] = 0x40;
                            wifiSendBuffer[2] = 0x80;
                            wifi.isDataChanged = 1;                            
                        }
                    }
                    break;
            }
        }
        return;
    }

    if (escalator.intervalFlag == 1)
    {
        escalator.intervalFlag = 0;
        for (index = 0; index < 3; index++)
        {
            escalator.arm[index].variability[0] = 0;    /* our buffer size is 5 */
            escalator.arm[index].variability[1] = 0;
            escalator.arm[index].variability[2] = 0;
			escalator.arm[index].variability[3] = 0;
			escalator.arm[index].variability[4] = 0;		
        }
    }
    for (index = 0; index < 3; index++) /* Since all sensors have its characteristic, so we done the detection seperately */
    {
        switch (index) /* p1_1 to p1_3 stands for escalator arm from left to right respectively*/
        {
            case 0:                
                if (p0_1)  escalator.arm[0].variability[POS_1of5]++; else escalator.arm[0].variability[POS_1of5] = 0;
								if (p0_3)  escalator.arm[0].variability[POS_2of5]++; else escalator.arm[0].variability[POS_2of5] = 0;
                //if (p0_7)  escalator.arm[0].variability[POS_3of5]++; else escalator.arm[0].variability[POS_3of5] = 0;
								//if (p1_0)  escalator.arm[0].variability[POS_4of5]++; else escalator.arm[0].variability[POS_4of5] = 0;
								//if (p1_1)  escalator.arm[0].variability[POS_5of5]++; else escalator.arm[0].variability[POS_5of5] = 0;
                for (index2 = 4; index2 >= 0; index2--)
                {
                    if (escalator.arm[0].variability[index2] >= 1250)
                    {
                        escalator.arm[0].variability[POS_1of5] = 0; /* clear all variability to prevent near-side trigger*/
						escalator.arm[0].variability[POS_2of5] = 0;
						escalator.arm[0].variability[POS_3of5] = 0;
						escalator.arm[0].variability[POS_4of5] = 0;
						escalator.arm[0].variability[POS_5of5] = 0;
                        escalator.arm[0].currentPos = index2; 
                        if (escalator.arm[0].lastPos != escalator.arm[0].currentPos) /* check if the current pos equal to previous pos, if it doesn't, trigger isDataChanged flag which inform wifi module we got data to send */
                        {
                            escalator.arm[0].lastPos = escalator.arm[0].currentPos;                            
                            wifi.isDataChanged = 1; /* since we have three place uses this flag, so be aware that if you put this flag detector into interrupt */
                        }                        
                    }
                }  
                break;
					
            case 1:
                //shiftedADC = (uint8_t) (adc_buf[0].p1_2 >> 8);
                if (p1_2==0)  escalator.arm[1].variability[POS_1of5]++;
                else if (p1_3==0) escalator.arm[1].variability[POS_2of5]++;
                else if (p1_5==0) escalator.arm[1].variability[POS_3of5]++;
                else if (p1_6==0) escalator.arm[1].variability[POS_4of5]++;
								else if (p1_7==0) escalator.arm[1].variability[POS_5of5]++;
                for (index2 = 4; index2 >= 0; index2--)
                {
                    if (escalator.arm[1].variability[index2] >= 750)
                    {
                        escalator.arm[1].variability[POS_1of5] = 0;
						escalator.arm[1].variability[POS_2of5] = 0;
						escalator.arm[1].variability[POS_3of5] = 0;
						escalator.arm[1].variability[POS_4of5] = 0;
						escalator.arm[1].variability[POS_5of5] = 0;
                        escalator.arm[1].currentPos = index2;
                        if (escalator.arm[1].lastPos != escalator.arm[1].currentPos)
                        {
                            escalator.arm[1].lastPos = escalator.arm[1].currentPos;                            
                            wifi.isDataChanged = 1;
                        }                        
                    }
                }  
                break;
            case 2:   
                if (p2_0==0)  escalator.arm[2].variability[POS_1of5]++;
                else if (p2_1==0) escalator.arm[2].variability[POS_2of5]++;
                else if (p2_3==0) escalator.arm[2].variability[POS_3of5]++;
				else if (p2_4==0) escalator.arm[2].variability[POS_4of5]++;
				else if (p2_5==0) escalator.arm[2].variability[POS_5of5]++;
                for (index2 = 4; index2 >= 0; index2--)
                {
                    if (escalator.arm[2].variability[index2] >= 750)
                    {
                        escalator.arm[2].variability[POS_1of5] = 0;
						escalator.arm[2].variability[POS_2of5] = 0;
						escalator.arm[2].variability[POS_3of5] = 0;
						escalator.arm[2].variability[POS_4of5] = 0;
						escalator.arm[2].variability[POS_5of5] = 0;
                        escalator.arm[2].currentPos = index2;
                        if (escalator.arm[2].lastPos != escalator.arm[2].currentPos)
                        {
                            escalator.arm[2].lastPos = escalator.arm[2].currentPos;                            
                            wifi.isDataChanged = 1;
                        }                        
                    }
                }                                    
                break;
							
        }
    }
}


















/*************flawless0714 * END OF FILE****/