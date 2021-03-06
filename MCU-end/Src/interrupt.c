#include <SI_EFM8LB1_Register_Enums.h>
#include "esp8266.h"
#include "uart.h"
#include "escalator.h"
#include "main.h"

#define NUM_SCANS 3 /* ADC use */

extern volatile Escalator escalator;
extern volatile uint8_t pdata wifiSendBuffer[SEND_BUFFER_SIZE];
extern volatile uint8_t pdata wifiRecvBuffer[RECV_BUFFER_SIZE];
extern volatile Uart uart;
extern volatile Mcu mcu;
extern volatile Wifi wifi;
extern volatile Successive_tv pdata ssv_tv;
extern const uint16_t pdata dac_speed_table[11];
volatile uint8_t pdata ssv_lv_idx_L = 2; // starts from lv2 which is the index of next DAC assignment
volatile uint8_t pdata ssv_lv_idx_M = 2;
volatile uint8_t pdata ssv_lv_idx_R = 2;
volatile uint16_t tick_in_sec;
sbit IR38K=P0^2;

SI_SBIT(LED0, SFR_P1, 4); // P3.4 LED0

void UART0_ISR(void) interrupt UART0_IRQn /* WARN: we only turn interrupt at needed */
{
    if (SCON0_TI == 1)
    {
        SCON0_TI = 0;
        if (uart.queuingByte > 0)
        {
            uart.queuingByte--;
            SBUF0 = wifiSendBuffer[uart.currentPos++];
        }
        else
        {
            uart.currentPos = 0;
            uart.state = SEND_DONE;
            //IE_EA = 0;			
			//IE_EA = 1;
            if (uart.byteWaiting > 0)
            {
                wifi.currentTick = mcu.sysTick; /* timeout detection */
                /* uart DAC use */  uart.Tstate = RX_BUSY;
                //SCON0 |= SCON0_REN__RECEIVE_ENABLED;
            }
            else
            {                
                wifi.state = RUNNING_TRAINING;
                uart.state = STANDBY;
                
            }
        }
    }
    if (SCON0_RI == 1)
    {
        SCON0_RI = 0;
        /* this is to capture the end training notification data, since there is no better solution concluded, hence... (TODO) */
			
        if (SBUF0 == '~')
        {
            uart.byteWaiting = UART_DAC_SIZE;
            uart.state = END_IN_PROGRESS;
        }
			
        if (uart.byteWaiting > 0)
        {
            wifiRecvBuffer[uart.currentPos] = SBUF0; 
            uart.currentPos++; /* TODO: since we are not sure how compiler implement the ++ suffix at last line so we seperate it */
            uart.byteWaiting--;
        }
        if(uart.byteWaiting == 0)
        {
            uart.currentPos = 0;
            uart.state = RECV_DONE;
            /* uart DAC use */  uart.Tstate = RX_DONE;
            //SCON0 &= ~SCON0_REN__RECEIVE_ENABLED;
        }
    }
    
}

/* This timer used as systick */
void TIMER2_ISR(void) interrupt TIMER2_IRQn
{
    uint8_t savedPage;
    TMR2CN0_TF2H = 0;
    
    mcu.sysTick++;

    if (escalator.mode == SUCCESSIVE)
    {
        if (mcu.sysTick % 1000 == 0 && mcu.sysTick > 1000)
            tick_in_sec++;

        if (tick_in_sec % ssv_tv.L_wheel == 0 && mcu.sysTick % 1000 == 0)
        {
            savedPage = SFRPAGE;
	        SFRPAGE = PG4_PAGE;
            DAC0L = (uint8_t) dac_speed_table[ssv_lv_idx_L % MAX_SPEED_LV] & 0xff;
	        DAC0H = (uint8_t) (dac_speed_table[ssv_lv_idx_L % MAX_SPEED_LV] >> 8);
            ssv_lv_idx_L++;
            
            SFRPAGE = savedPage;
        } 

        if (tick_in_sec % ssv_tv.M_wheel == 0 && mcu.sysTick % 1000 == 0)
        {
            savedPage = SFRPAGE;
	        SFRPAGE = PG4_PAGE;

            DAC1L = (uint8_t) dac_speed_table[ssv_lv_idx_M % MAX_SPEED_LV] & 0xff;
	        DAC1H = (uint8_t) (dac_speed_table[ssv_lv_idx_M % MAX_SPEED_LV] >> 8);
            ssv_lv_idx_M++;
            
            SFRPAGE = savedPage;
        }

        if (tick_in_sec % ssv_tv.R_wheel == 0 && mcu.sysTick % 1000 == 0)
        {
            savedPage = SFRPAGE;
	        SFRPAGE = PG4_PAGE;

            DAC2L = (uint8_t) dac_speed_table[ssv_lv_idx_R % MAX_SPEED_LV] & 0xff;
	        DAC2H = (uint8_t) (dac_speed_table[ssv_lv_idx_R % MAX_SPEED_LV] >> 8);
            ssv_lv_idx_R++;
            
            SFRPAGE = savedPage;
        }
    }
    
    //if (/*(wifi.state == RUNNING_TRAINING) &&*/ (mcu.sysTick % 1000 == 0))
    //{
    //    LED0 = ~LED0;
        //LED0 = 0;
        
    //}
    /* This mechanism is no longer used due to we are not using IR distance sensor now
    if ((mcu.sysTick % 1000) == 0) //500ms clear array value
    {
        escalator.intervalFlag = 1;
    }
    */
    /* ADC scan trigger
    if ((mcu.sysTick % 10) == 0)
    {
        savedPage = SFRPAGE;
        SFRPAGE = 0x30;
        if (~(ADC0CN0 & 0x10))  keep ADC wake since we use a mode which should keep set the ADC_BUSY flag 
		{
			ADC0CN0 |= 0x10;  trigger ADC on 
		}
        SFRPAGE = savedPage;
    }
    */
    /*
    if (adc_buf[0] > 950)
    {
        LED0 = 1;
    }
    else
    {
        LED0 = 0;
    }
    */
    /* escalator.intervalFlag set here */
}
/* IR pulser */
void Timer3_ISR (void) interrupt TIMER3_IRQn
{
    uint8_t savedpage = SFRPAGE;
	
    SFRPAGE = 0x0;
	
    TMR3CN0 &= 0x7f; //clear TF3H because the timer are overflow		
    SFRPAGE = savedpage;

    IR38K = ~IR38K;
}




/*************flawless0714 * END OF FILE****/

