#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <SI_EFM8LB1_Register_Enums.h>
#include <stdlib.h>
#include "uart.h"
#include "esp8266.h"
#include "Escalator.h"

#define DAC_APPLY_TIME 25
	 
sbit p0_1=P0^1;
sbit p0_3=P0^3;
sbit p0_7=P0^7;
sbit p1_0=P1^0;
sbit p1_1=P1^1;

sbit p1_2=P1^2;
sbit p1_3=P1^3;
sbit p1_5=P1^5;
sbit p1_6=P1^6;
sbit p1_7=P1^7;

sbit p2_0=P2^0;
sbit p2_1=P2^1;
sbit p2_3=P2^3;
sbit p2_4=P2^4;
sbit p2_5=P2^5;
	

	 
void Init(void);



typedef struct
{
    uint32_t sysTick; /* tick per second */
}Mcu;

	
	




#ifdef __cplusplus
}
#endif

#endif

/*************flawless0714 * END OF FILE****/
