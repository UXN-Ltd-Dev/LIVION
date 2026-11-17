/*******************************************************************************
* @file  app_defines.h
* @brief
*******************************************************************************
* # License
* <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
*******************************************************************************
*
* The licensor of this software is Silicon Laboratories Inc. Your use of this
* software is governed by the terms of Silicon Labs Master Software License
* Agreement (MSLA) available at
* www.silabs.com/about-us/legal/master-software-license-agreement. This
* software is distributed to you in Source Code format and is governed by the
* sections of the MSLA applicable to Source Code.
*
******************************************************************************/
/**
 * @file         app_defines.h
 * @version      0.1
 *
 *  @brief : This file contains user configurable details to configure the device
 *
 *  @section Description  This file contains user configurable details to configure the device
 *
 *
 */
#ifndef APP_DEFINES_H
#define APP_DEFINES_H


#define AWS_FW_VERSION    "26-10-26D 1.3.0V"


#define DEBUG_USER_MSG      0
#define DEBUG_ASYNC_MSG     0
#define DEBUG_ASYNC_PACKET  0

#define NO_EVENT_ID   (-1)

#define SUCCESS 1
#define FAIL    0

#define API_TRUE         1
#define API_FALSE       0


#define CB_EVENT_SET    1
#define CB_EVENT_CLR    0

#define SET_USART_EVENT   1
#define CLR_USART_EVENT   0

#define TIME_OVER         0
#define USART_TMOUT       2

#define make_Byte(a, b)   ((uint8_t) ((a) >> (8*b)))      // word or DWord daya -> byte
#define make_High(val)    ((uint8_t)((val) >> 8))
#define make_Low(val)     ((uint8_t)((val) & 0xFF)) // 하위 8비트 추출

#define WS_SEND_DATA_FAIL        0
#define WS_SEND_DATA_SUCCESS    1

#define WS_CONNECT_FAIL        0
#define WS_CONNECT_TRUE        1

#define WS_SEND_CLEAR     0
#define WS_SEND_STOP      0
#define WS_SEND_START     1

#define DISCONNECT         0
#define CONNECT            1


typedef enum  {
  ERROR_NO_ERROR = 0,
  ERROR_FORMAT,
  ERROR_PARAMETER,
  ERROR_REGULAR,
  ERROR_COMMAND_CANCEL
} error_code_state_e;



// Enumeration for states in application
typedef enum  {
  USER_APP_INIT_STATE = 0,
  USER_APP_WS_INIT_STATE,
  USER_APP_WS_OPEN_STATE,
  USER_APP_WS_SEND_DATA,
  USER_APP_WS_SEND_SUCCESS,
  USER_APP_WS_SEND_FAIL,
  USER_APP_WS_CLOSE,
  USER_APP_TIME_EVENT,
  USER_APP_CMD_MAX
} user_app_state_e;




typedef enum  {
  WS_APP_IDLE_STATE=0,
  WS_APP_INIT_STATE,
  WS_APP_INIT_SUCCESS,
  WS_APP_INIT_FAIL,
  WS_APP_DATA_SEND_MODE,
  WS_APP_OPEN_SUCCESS,
  WS_APP_OPEN_FAIL,
  WS_APP_SEND_DATA_STATE,
  WS_APP_SEND_DATA_DONE,
  WS_APP_SEND_DATA_TMOUT,
  WS_APP_PING_STATE,
  WS_APP_CLOSE_STATE,
  WS_APP_DEINIT_STATE,
  WS_APP_UNCONNECT_STATE,
  WS_APP_RECONNECT_STATE,
  WS_APP_STATE_MAX
} ws_app_state_e;


typedef enum  {
  WS_JSON_MSG_IDLE=0,
  WS_JSON_MSG_INIT,
  WS_JSON_MSG_idTAG,
  WS_JSON_MSG_AS614,
  WS_JSON_MSG_BASIC_INFO,
  WS_JSON_MSG_STATUS_INFO,
  WS_JSON_MSG_SENSOR_INFO,
  WS_JSON_MSG_VERIFICATION,
  WS_JSON_MSG_OUTPUT_CAPACITY,
  WS_JSON_MSG_HEARTBEAT,
  WS_JSON_MSG_RESPONSE,
  WS_JSON_MSG_MAX
} ws_json_message_e;


#endif
