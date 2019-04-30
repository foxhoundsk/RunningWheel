#ifndef __ESP8266_H
#define __ESP8266_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <SI_EFM8LB1_Register_Enums.h>
#include <string.h> // memset() use
#include "uart.h"
#include "main.h"

#define SSID "CCC_Lab\0"
#define PASSWORD "00001111\0"
#define MODULE_ACK  "ACK\0"
#define RECV_BUFFER_SIZE            45  /* may be smaller (43 bytes used to receive msg from PC)*/
#define SEND_BUFFER_SIZE            50  /* may be smaller */
#define TYPICAL_RECEIVE_SIZE        6   /* for OK-like command's return */
#define ECHO_TURN_OFF_EDGE          13  /* at echo turn off edge, 8266 will have last echo so that the buffer should larger than typical */
#define DEVICE_CONNECTED_SIZE       37  /* only for one device */
#define DEVICE_CONN_ESTABLISH_SIZE  17  /* indicate that connection is established */
#define RST_RES_SIZE                15
#define RST_WAIT_TIME               1700
#define PACKET_WAIT_TIME            2000
#define TYPICAL_WAIT_TIME           500
#define KNOCK_RECEIVE_SIZE          13
#define POS_DATA_SIZE               3
#define CIPSEND_START               16
#define CIPSEND_DATA_RECV__DAC      27 
#define WIFI_DAC_DATA_SIZE          17/* this included header from esp8266 which is +IPD... */
#define NO_DATA_EXPECTED            0

/* AT command----------------------------------*/
#define AT_RST      "AT+RST\0"
#define AT          "AT\0"
#define AT_OK       "OK\0"
#define AT_CIPMUX   "AT+CIPMUX\0" /*  note that single connection maybe also work */
#define AT_NO_ECHO  "ATE0\0"
#define AT_CWMODE   "AT+CWMODE\0"
#define AT_CWSAP    "AT+CWSAP\0"
#define AT_CWDHCP   "AT+CWDHCP\0"
#define AT_CIPSTART "AT+CIPSTART\0"
#define AT_CWLIF    "AT+CWLIF\0"
#define AT_CIPSEND  "AT+CIPSEND\0"
#define AT_CIPCLOSE "AT+CIPCLOSE\0"

void wifiProcess(void);
void wifiRecvCheck(void);
void wifiModuleInit(void);
void wifiInit(void);
void wifiCommandEncode(uint8_t* buffer, uint8_t* option);
void wifiTypicalPacketCheck(void);
void wifiPosDataEncode(void);
void wifiApplyDACdata(void);

typedef enum
{
  INIT = 20,
  ATE0_DONE,
  CHECK_PASS,
  CHECK_FAILED,
  RECV_CHECK,
  RUNNING_UNCONNECT,
  RUNNING_CONNECTED,
  RUNNING_ESTABLISHED,
  RUNNING_START_BUTTON_WAITING,
  RUNNING_TRAINING_READY,
  RUNNING_TRAINING,
  RUNNING_TRAINING_DONE,
  DATA_SENDING,
  MODULE_ERROR,
  DEBUG
}ESP8266_state; 

/* this state is used in recvCheck, make it check various response of AT-command */
typedef enum
{
    RST = 0,
    ATE0,
    CWMODE,
    CWSAP,
    CIPMUX,
    CIPCLOSE,
    CWDHCP,
    CWLIF,
    MODULE_RUNNING,
    MODULE_RUNNING_POS_SEND,
    MODULE_SEND_DATA_ATCOMMAND,
    MODULE_PARSE_DAC_DATA,
    MODULE_UNCONNECT,
    MODULE_CONNECTED_KNOCKING,
}CommandState;

typedef struct
{
    ESP8266_state state;
    CommandState Cstate; /* check declaration for detail */
    uint8_t errorCount;
    uint8_t isDataChanged;    
    uint32_t currentTick;
}Wifi;








#ifdef __cplusplus
}
#endif

#endif

/*************flawless0714 * END OF FILE****/
