#ifndef __ESCLATOR_H
#define __ESCLATOR_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "esp8266.h"


	 
	 
void escalatorProcess(void);

typedef enum
{
  LEFT_WHEEL = 0,
  MIDDLE_WHEEL,
  RIGHT_WHEEL
} wheel_num;

typedef enum
{ 
    POS_1of5 = 0,
    POS_2of5,
    POS_3of5,
	POS_4of5,
    POS_5of5,
    POS_INIT
}RAT_POS;

typedef enum
{
    NORMAL = 1,
    FREE_WAY
}MODE;

typedef struct
{
    uint32_t variability[5];
    uint8_t lastPos;
    uint8_t currentPos;
}Arm;

typedef struct
{
    Arm arm[3];
    MODE mode;
    uint8_t intervalFlag;
}Escalator;

















#ifdef __cplusplus
}
#endif

#endif

/*************flawless0714 * END OF FILE****/