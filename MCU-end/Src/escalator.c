#include "escalator.h"
#include "esp8266.h"


volatile Escalator escalator;
extern volatile uint8_t CONVERSION_COMPLETE;
extern volatile uint8_t pdata wifiSendBuffer[SEND_BUFFER_SIZE];
extern volatile Wifi wifi;
extern volatile Uart uart;

const uint16_t pdata dac_speed_table[11] = { 0x0000, 0x0174, 0x02E8, 0x045D, 0x5D1, 0x0745, 0x08B9, 0x0A2D, 0x0BA2, 0x0D16, 0x0E8A};
volatile uint8_t pdata L_emer_flag, M_emer_flag, R_emer_flag;
volatile uint16_t pdata L_dac_reg, M_dac_reg, R_dac_reg;


/* TODO: if very early state the escalator should run? */
void escalatorProcess(void)
{   
    uint8_t index; /* for loop and array index use (WARN) */
    int8_t index2;
    uint8_t savedpage;

	/*	WARN: debug use (known issue: this section should uncommented at release version(wifi ver), it prevent this func(escalatorProcess()) run before wifi module done its initialization and ready to send training data, although it is harmless) */
    if (uart.Tstate == WAIT_KNOCK_DOOR)
    {
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
		
	if (escalator.mode == FREE_WAY)
    {
        for (index = 0; index < 3; index++)
        {
            switch (index)
            {
                case 0:
                    if (escalator.arm[0].currentPos == 0)
                    {
                        if (!p0_1) escalator.arm[0].variability[POS_1of5]++; else escalator.arm[0].variability[POS_1of5] = 0;
                        if (escalator.arm[0].variability[0 /* we use p0_1 as trigger point */] >= 1250)
                        {
                                escalator.arm[0].currentPos++;
                                escalator.arm[0].variability[0] = 0;
                        }
					}
                    else if (escalator.arm[0].currentPos == 1)
                    {
                        if (p0_1) escalator.arm[0].variability[POS_1of5]++; else escalator.arm[0].variability[POS_1of5] = 0;
											
                        if (escalator.arm[0].variability[0 /* we use 30 degree sensor as trigger point */] >= 1250)
                        {
                            escalator.arm[0].variability[0] = 0;
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
                        if (!p1_2) escalator.arm[1].variability[POS_1of5]++; else escalator.arm[1].variability[POS_1of5] = 0;
                        if (escalator.arm[1].variability[POS_1of5] >= 1250)
                        {
                                escalator.arm[1].currentPos++;
                                escalator.arm[1].variability[POS_1of5] = 0;
                        }
					}
                    else if (escalator.arm[1].currentPos == 1)
                    {
                        if (p1_2) escalator.arm[1].variability[POS_1of5]++; else escalator.arm[1].variability[POS_1of5] = 0;
											
                        if (escalator.arm[1].variability[POS_1of5] >= 1250)
                        {
                            escalator.arm[1].variability[POS_1of5] = 0;
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
                        if (!p2_0) escalator.arm[2].variability[POS_1of5]++; else escalator.arm[2].variability[POS_1of5] = 0;
                        if (escalator.arm[2].variability[POS_1of5] >= 1250)
                        {
                                escalator.arm[2].currentPos++;
                                escalator.arm[2].variability[POS_1of5] = 0;
                        }
					}
                    else if (escalator.arm[2].currentPos == 1)
                    {
                        if (p2_0) escalator.arm[2].variability[POS_1of5]++; else escalator.arm[2].variability[POS_1of5] = 0;
											
                        if (escalator.arm[2].variability[POS_1of5] >= 1250)
                        {
                            escalator.arm[2].variability[POS_1of5] = 0;
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

    // if we are in successive mode, and the emer_flag is set and the DAC value is not zero, then it means
    // dac speed is changed in the interrupt updater, we should change it back to zero to keep safety of
    // rat. Once rat arriving another position (!=1), we restore the dac value back. Note that we got a
    // small chance to restore the old dac value, which occured when flag is set and rat is arriving new
    // position, and the new dac value is just updated after this statement.
    if (escalator.mode == SUCCESSIVE)
    {
        savedpage = SFRPAGE;
        SFRPAGE = PG4_PAGE;

        // left wheel
        if ((DAC0L != 0x00 || DAC0H != 0x00) && L_emer_flag == 1)
        {
            L_dac_reg = DAC0H;
            L_dac_reg <<= 8;
            L_dac_reg |= DAC0L;

            DAC0L = 0;
            DAC0H = 0;
        }

        // middle wheel
        if ((DAC1L != 0x00 || DAC1H != 0x00) && M_emer_flag == 1)    
        {
            M_dac_reg = DAC1H;
            M_dac_reg <<= 8;
            M_dac_reg |= DAC1L;

            DAC1L = 0;
            DAC1H = 0;
        }

        // right wheel
        if ((DAC2L != 0x00 || DAC2H != 0x00) && R_emer_flag == 1)    
        {
            R_dac_reg = DAC2H;
            R_dac_reg <<= 8;
            R_dac_reg |= DAC2L;

            DAC2L = 0;
            DAC2H = 0;
        }
        SFRPAGE = savedpage;
    }

    // TODO: you may find out that stuff inside this for loop can wrap with func. 
    for (index = 0; index < 3; index++) /* Since all sensors have its characteristic, so we done the detection seperately */
    {
        switch (index) /* p1_1 to p1_3 stands for escalator arm from left to right respectively*/
        {
            case LEFT_WHEEL:                
                if (p0_1)  escalator.arm[0].variability[POS_1of5]++; else escalator.arm[0].variability[POS_1of5] = 0;
				if (p0_3)  escalator.arm[0].variability[POS_2of5]++; else escalator.arm[0].variability[POS_2of5] = 0;
                if (p0_7)  escalator.arm[0].variability[POS_3of5]++; else escalator.arm[0].variability[POS_3of5] = 0;
                if (p1_0)  escalator.arm[0].variability[POS_4of5]++; else escalator.arm[0].variability[POS_4of5] = 0;
                if (p1_1)  escalator.arm[0].variability[POS_5of5]++; else escalator.arm[0].variability[POS_5of5] = 0;
                for (index2 = 4; index2 >= 0; index2--)
                {
                    if (escalator.arm[0].variability[index2] >= 1500)
                    {                        
                        escalator.arm[0].variability[POS_1of5] = 0; /* clear all variability to prevent near-side trigger*/
						escalator.arm[0].variability[POS_2of5] = 0;
						escalator.arm[0].variability[POS_3of5] = 0;
						escalator.arm[0].variability[POS_4of5] = 0;
						escalator.arm[0].variability[POS_5of5] = 0;
                        escalator.arm[0].currentPos = index2;                        

                        if (escalator.arm[0].lastPos != escalator.arm[0].currentPos) /* check if the current pos equal to previous pos, if it doesn't, trigger isDataChanged flag which inform wifi module we got data to send */
                        {
                            if (escalator.mode == SUCCESSIVE) // emergency stop. reson why I add here is to prevent duplicate detection
                            {
                                if (index2 == POS_1of5) // emergency stop. reson why I add here is to prevent duplicate detection
                                { 
                                    L_dac_reg = 0; // reset for new save

                                    savedpage = SFRPAGE;
	                                SFRPAGE = PG4_PAGE;

                                    L_dac_reg = DAC0H;
                                    L_dac_reg <<= 8;
                                    L_dac_reg |= DAC0L;

                                    DAC0L = 0;
                                    DAC0H = 0;

                                    SFRPAGE = savedpage;

                                    L_emer_flag = 1;
                                }
                                else if (L_emer_flag) // restore the value of DAC reg of L_wheel
                                {
                                    savedpage = SFRPAGE;
                                    SFRPAGE = PG4_PAGE;

                                    DAC0L = L_dac_reg & 0xff;
                                    DAC0H = L_dac_reg >> 8;

                                    SFRPAGE = savedpage;

                                    L_emer_flag = 0; // clear the flag for next use
                                }
                            }
                            else // normal mode
                            {
                                if (index2 == POS_1of5) // emergency stop. reson why I add here is to prevent duplicate detection
                                { 
                                    L_dac_reg = 0; // reset for new save

                                    savedpage = SFRPAGE;
                                    SFRPAGE = PG4_PAGE;

                                    DAC0L = 0;
                                    DAC0H = 0;

                                    SFRPAGE = savedpage;

                                    L_emer_flag = 1;
                                }
                                else if (L_emer_flag) // rat arrived a new position which is not position 1,
                                                      // we just reset the flag since the new speed value is 
                                                      // coming in short time
                                {
                                    L_emer_flag = 0; // clear the flag for next use
                                }
                            }
                            
                            escalator.arm[0].lastPos = escalator.arm[0].currentPos;                            
                            wifi.isDataChanged = 1; /* since we have three place uses this flag, so be aware that if you put this flag detector into interrupt */
                        }                        
                    }
                }  
                break;
					
            case MIDDLE_WHEEL:
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
                            if (escalator.mode == SUCCESSIVE)
                            {
                                if (index2 == POS_1of5) // emergency stop. reson why I add here is to prevent duplicate detection
                                {
                                    M_dac_reg = 0; // reset for new save

                                    savedpage = SFRPAGE;
                                    SFRPAGE = PG4_PAGE;

                                    M_dac_reg = DAC1H;
                                    M_dac_reg <<= 8;
                                    M_dac_reg |= DAC1L;

                                    DAC1L = 0;
                                    DAC1H = 0;

                                    SFRPAGE = savedpage;

                                    M_emer_flag = 1;
                                }
                                else if (M_emer_flag) // restore the value of DAC reg of M_wheel
                                {
                                    savedpage = SFRPAGE;
                                    SFRPAGE = PG4_PAGE;

                                    DAC1L = M_dac_reg & 0xff;
                                    DAC1H = M_dac_reg >> 8;

                                    SFRPAGE = savedpage;

                                    M_emer_flag = 0; // clear the flag for next use
                                }
                            }
                            else
                            {
                                if (index2 == POS_1of5) // emergency stop. reson why I add here is to prevent duplicate detection
                                { 
                                    M_dac_reg = 0; // reset for new save

                                    savedpage = SFRPAGE;
                                    SFRPAGE = PG4_PAGE;

                                    DAC1L = 0;
                                    DAC1H = 0;

                                    SFRPAGE = savedpage;

                                    M_emer_flag = 1;
                                }
                                else if (M_emer_flag)
                                {
                                    M_emer_flag = 0; // clear the flag for next use
                                }
                            }

                            escalator.arm[1].lastPos = escalator.arm[1].currentPos;                            
                            wifi.isDataChanged = 1;
                        }                        
                    }
                }  
                break;
            case RIGHT_WHEEL:   
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
                            if (escalator.mode == SUCCESSIVE)
                            {
                                if (index2 == POS_1of5) // emergency stop. reson why I add here is to prevent duplicate detection
                                {
                                    R_dac_reg = 0; // reset for new save

                                    savedpage = SFRPAGE;
                                    SFRPAGE = PG4_PAGE;

                                    R_dac_reg = DAC2H;
                                    R_dac_reg <<= 8;
                                    R_dac_reg |= DAC2L;

                                    DAC2L = 0;
                                    DAC2H = 0;

                                    SFRPAGE = savedpage;

                                    R_emer_flag = 1;
                                }
                                else if (R_emer_flag) // restore the value of DAC reg of R_wheel
                                {
                                    savedpage = SFRPAGE;
                                    SFRPAGE = PG4_PAGE;

                                    DAC2L = R_dac_reg & 0xff;
                                    DAC2H = R_dac_reg >> 8;

                                    SFRPAGE = savedpage;

                                    R_emer_flag = 0; // clear the flag for next use
                                }
                            }
                            else
                            {
                                if (index2 == POS_1of5) // emergency stop. reson why I add here is to prevent duplicate detection
                                { 
                                    R_dac_reg = 0; // reset for new save

                                    savedpage = SFRPAGE;
                                    SFRPAGE = PG4_PAGE;

                                    DAC2L = 0;
                                    DAC2H = 0;

                                    SFRPAGE = savedpage;

                                    R_emer_flag = 1;
                                }
                                else if (R_emer_flag)
                                {
                                    R_emer_flag = 0; // clear the flag for next use
                                }
                            }
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
