/*******************************************************************************
* @file  app_globals.h
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
 * @file         app_globals.h
 * @version      0.1
 *
 *  @brief : This file contains user configurable details to configure the device
 *
 *  @section Description  This file contains user configurable details to configure the device
 *
 *
 */
#ifndef APP_GLOBALS_H
#define APP_GLOBALS_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "app_defines.h"

typedef struct {
  uint8_t bit0:1;
  uint8_t bit1:1;
  uint8_t bit2:1;
  uint8_t bit3:1;
  uint8_t bit4:1;
  uint8_t bit5:1;
  uint8_t bit6:1;
  uint8_t bit7:1;
} tyFLAGBITS;

typedef union {
  uint8_t bAll;
  tyFLAGBITS b;
} tyBITS;

extern tyBITS  wlan_bit_state;
extern tyBITS  device_bit_state;
extern tyBITS  module_bit_state;
extern tyBITS  module_bit_error;
extern tyBITS  active_bit_state;
extern tyBITS  valve_bit_state;
extern tyBITS  ws_send_bit_state;
extern tyBITS  user_info_bit_state;

#define GET_HOT_WATER_SWITCH()      device_bit_state.b.bit7   // 온수 스위치
#define GET_CARBON_GAS_STATE()      device_bit_state.b.bit6   // 탄소 가스 상태
#define GET_CARBON_GAS_LEAK()       device_bit_state.b.bit5   // 탄소 외부 수위
#define GET_CARBON_GAS_LEVEL()      device_bit_state.b.bit4   // 탄소 모듈 급수
#define GET_COMPRESSOR_MOTOR()      device_bit_state.b.bit3   // 콤프레샤 가동
#define GET_COLD_WATER_TEMP()       device_bit_state.b.bit2   // 냉수 온도 센서
#define GET_COLD_WATER_LEVEL()      device_bit_state.b.bit1   // 냉수 습수 상태
#define GET_COOLING_STATE()         device_bit_state.b.bit0   // 냉각 불량 상태

#define GET_CALIBRATION_STATE()     module_bit_state.b.bit7   // 캘리브레이션 상태
#define GET_HOT_WATER_SOL_STATE()   module_bit_state.b.bit5   // 순간 온수 솔 상태
#define GET_MODULE_STOP_AND_GO()    module_bit_state.b.bit4   // 모듈 상태 정지 중 재 동작 가능
#define GET_MODULE_STOP_FAULT()     module_bit_state.b.bit3   // 모듈 상태 정지 중 재 동작 불가
#define GET_MODULE_RUNNING()        module_bit_state.b.bit2   // 모듈 상태 동작 중
#define GET_MODULE_STOP()           module_bit_state.b.bit1   // 모듈 상태 정지
#define GET_MODULE_INIT()           module_bit_state.b.bit0   // 모듈 상태 초기화

#define GET_COMM_SENSOR_ERROR()     module_bit_error.b.bit7   // 통신 센서 에러
#define GET_FLUX_SENSOR_ERROR()     module_bit_error.b.bit6   // 유량 센서 에러
#define GET_WATER_OUTLET_ERROR()    module_bit_error.b.bit3   // 출수 센서 에러
#define GET_INNER_SENSOR_ERROR()    module_bit_error.b.bit2   // 내부 센서 에러
#define GET_WATER_INLET_ERROR()     module_bit_error.b.bit1   // 입수 센서 에러
#define GET_ZERO_CROSS_ERROR()      module_bit_error.b.bit0   // 제로 크로스 에러

#define GET_ALL_SENSOR_STATE()      active_bit_state.b.bit5   // 제품 정상 작동 유무
#define GET_UNUSED_LONG_TIME()      active_bit_state.b.bit4   // 무 출력 상태 지속
#define GET_UVC_LIFETIME_OVER()     active_bit_state.b.bit3   // UVC 수명 상태
#define GET_WATER_VALVE_ERROR()     active_bit_state.b.bit2   // 단수 밸브 불랭 상태
#define GET_WATER_LEAKAGE()         active_bit_state.b.bit1   // 누수 발생
#define GET_CONTINUOUS_HOT_WATER()  active_bit_state.b.bit0   // 온수 모듈 연속 가동 상태

#define GET_VALVE_FLOW_STATUS()     valve_bit_state.b.bit3
#define GET_RAW_WATER_SOLENOID()    valve_bit_state.b.bit2
#define GET_SPK_WATER_SOLENOID()    valve_bit_state.b.bit1
#define GET_COLD_WATER_SOLENOID()   valve_bit_state.b.bit0

//------------------------------------------------------------------------------
#define SET_WS_RTC_UPDATE(val)      ws_send_bit_state.b.bit7=val
#define GET_WS_RTC_UPDATE()         ws_send_bit_state.b.bit7

#define SET_WS_HEARTBEAT(val)       ws_send_bit_state.b.bit6=val
#define GET_WS_HEARTBEAT()          ws_send_bit_state.b.bit6

#define SET_WS_VERIFICATION(val)    ws_send_bit_state.b.bit5=val
#define GET_WS_VERIFICATION()       ws_send_bit_state.b.bit5

#define SET_WS_SENSOR_INFO(val)     ws_send_bit_state.b.bit4=val
#define GET_WS_SENSOR_INFO()        ws_send_bit_state.b.bit4

#define SET_WS_STATUS_INFO(val)     ws_send_bit_state.b.bit3=val
#define GET_WS_STATUS_INFO()        ws_send_bit_state.b.bit3

#define SET_WS_OUTPUT_CAPACITY(val) ws_send_bit_state.b.bit2=val
#define GET_WS_OUTPUT_CAPACITY()    ws_send_bit_state.b.bit2

#define SET_WS_BASIC_INFO(val)      ws_send_bit_state.b.bit1=val
#define GET_WS_BASIC_INFO()         ws_send_bit_state.b.bit1

#define SET_WS_idTAG_STATUS(val)    ws_send_bit_state.b.bit0=val
#define GET_WS_idTAG_STATUS()       ws_send_bit_state.b.bit0

//------------------------------------------------------------------------------


#define WLAN_WS_DATA_SEND_STATE()   wlan_bit_state.b.bit2
#define WLAN_WS_CONNECT_STATE()     wlan_bit_state.b.bit1
#define WLAN_WIFI_CONNECT_STATE()   wlan_bit_state.b.bit0


//------------------------------------------------------------------------------
#define SET_OVER_ONE_DAY(val)       user_info_bit_state.b.bit5=val        // 1일이 되었는지 확인...
#define GET_OVER_ONE_DAY()          user_info_bit_state.b.bit5


#define SET_RCV_WATER_AMOUNT(val)   user_info_bit_state.b.bit4=val
#define GET_RCV_WATER_AMOUNT()      user_info_bit_state.b.bit4

#define SET_RCV_SW_VERIFIC(val)     user_info_bit_state.b.bit3=val
#define GET_RCV_SW_VERIFIC()        user_info_bit_state.b.bit3

#define SET_RCV_SENSOR_INFO(val)    user_info_bit_state.b.bit2=val
#define GET_RCV_SENSOR_INFO()       user_info_bit_state.b.bit2

#define SET_RCV_STATUS_INFO(val)    user_info_bit_state.b.bit1=val
#define GET_RCV_STATUS_INFO()       user_info_bit_state.b.bit1

#define SET_RCV_BASIC_INFO(val)     user_info_bit_state.b.bit0=val
#define GET_RCV_BASIC_INFO()        user_info_bit_state.b.bit0
//------------------------------------------------------------------------------

extern uint32_t ws_send_elapsed_time;
extern uint16_t ws_send_waiting_time;

extern uint16_t water_amount_time;

extern char authorize_idTag[32];
extern char authorize_api_key[32];


// 동시 신로를 보내지 않기 위해서...약간의 시간 갭을 준다..
#define WS_INTERVAL_5SEC         5       //  10sec...
#define WS_INTERVAL_10SEC       10       //  10sec...
#define WS_INTERVAL_25SEC       25
#define WS_INTERVAL_30SEC       30
#define WS_INTERVAL_45SEC       45
#define WS_INTERVAL_60SEC       60
#define WS_INTERVAL_10MIN      600

#define WS_INTERVAL_idTAG       100      //  10sec...
#define WS_INTERVAL_HEAERBEAT   600      //  300sec...

#define WS_INTERVAL_BASIC       30       //
#define WS_INTERVAL_STATUS      60       //  60sec...
#define WS_INTERVAL_SENSOR      90       // 120sec...
#define WS_INTERVAL_VERIFY     600       // 600sec
#define WS_INTERVAL_AMOUNT     300       // 600sec

#define WS_INTERVAL_HOUR      3600       //  60sec...

#define WS_INTERVAL_DAY      864000    //  1day
#define WS_CALLBACK_10SEC       100       //  10sec...

//------------------------------------------------------------------------------

typedef struct {
  int8_t  cb_25ms_event;
  int8_t  cb_50ms_event;
  int8_t  cb_100ms_event;
  int8_t  cb_1sec_event;

  int8_t  cb_20ms_count;
  int8_t  cb_50ms_count;
  int8_t  cb_100ms_count;
  int8_t  cb_1sec_count;
}periodic_timer_t;

extern periodic_timer_t periodic_timer;

typedef struct {
  uint8_t Year;
  uint8_t Month;
  uint8_t Day;
  uint8_t Hour;
  uint8_t Minute;
  uint8_t Second;
  uint8_t update;
  uint8_t over_day;
}user_calendar_t;
extern user_calendar_t  user_calendar;

#define USART_BUFF_SIZE  128
typedef struct {
  uint8_t read_buff[USART_BUFF_SIZE];
  uint8_t read_len;
  uint8_t read_map;
  uint8_t read_tmout;

}user_usart_event_t;
extern user_usart_event_t  user_usart_event;



/****************************************************
 * WEBSOCKET  INFORMATION
 ****************************************************/
#define CONNECT_SUCCESS     1
#define CONNECT_FAIL        0

#define WS_WIFI_CONNECT     1
#define WS_WIFI_UNCONNECT   0

#define WS_RECONNECT_START  1
#define WS_RECONNECT_STOP   0

#define WS_SEND_DATA_START  1
#define WS_SEND_DATA_STOP   0

#define WS_CONNECT          1
#define WS_UNCONNECT        0

#define WS_SUCCESS          1
#define WS_FAIL             0

#define WS_INIT_SUCCESS     2
#define WS_INIT_FAIL        1
#define WS_INIT_IDLE        0

#define WS_OPEN_SUCCESS     2
#define WS_OPEN_FAIL        1
#define WS_OPEN_IDLE        0

#define WS_SEND_DONE        3       // callback receive...
#define WS_SEND_SUCCESS     2
#define WS_SEND_FAIL        1
#define WS_SEND_IDLE        0


typedef struct {
  uint8_t connect;                // ws connect state
  uint8_t need_reconnect ;
  uint8_t retry_tmout;            //
  uint8_t retry_count;            // 최대 시도 횟수 이후..

  uint8_t send_data_flag;
  uint8_t send_data_mode;
  uint8_t send_data_tmout;
  uint8_t send_data_state;

  uint8_t now_step;             // 현재 단계
  uint8_t old_step;             // 이전 단계
  uint8_t init_state;
  uint8_t idTag_status;           // idTag 수신이 되어야 그 다음 단계로...

} user_ws_app_t;
extern user_ws_app_t user_ws_app;


typedef struct {
  uint8_t  cold_flow;
  uint8_t  warm_flow;
  uint16_t sparkling;
} water_recipe_t;

typedef struct {
  uint8_t  cold_flow;
  uint8_t  warm_flow;
  uint16_t sparkling;
} water_output_t;

typedef struct {
  int8_t calendar_update;

  water_output_t    water_output;
  water_recipe_t    water_recipe;
} user_info_t;

extern user_info_t user_info;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///
/// 영원 프로토콜에서 사용되는 변수 선언문..POINTER 형 변수
///

typedef struct {
  int8_t  p_name[8];
  int8_t  ver_main;
  int8_t  ver_sub1;
  int8_t  ver_sub2;

  uint32_t device_id;

  int8_t  year;
  int8_t  month;
  int8_t  day;
  int8_t  dummy1;

} basic_info_t;

typedef struct {
  uint8_t flux_state;         // 유량 상태
  uint8_t module_state;       // 모듈 상태
  uint8_t module_error;       // 모듈 에러
  uint8_t active_state;       // 동작 상태
} state_info_t;

typedef struct {
  uint8_t cold_water_Temp;      // 냉수 온도
  uint8_t warm_water_Temp;      // 온수 온도
  uint8_t ambient_1_Temp ;      // 주변 온도 1
  uint8_t ambient_2_Temp;      // 주변 온도 2

  uint8_t BF_NTU;               // 필터 전 수질
  uint8_t AF_NTU;               // 필터 후 수질
  uint8_t water_Quality;        // 수질 상태
  uint8_t flow_value;           // 유량 동작 값..

}sensor_info_t;

typedef struct {
  uint32_t cold_Watt_hr;        // 냉수 소비 전력량
  uint32_t warm_Watt_hr;        // 온수 소비 전력량
  uint32_t life_filter;         // 수명 필터.

  uint8_t active_state;         // 밸브 동작.
  uint8_t dummy1;
  uint8_t dummy2;
  uint8_t dummy3;
} valve_info_t;

typedef struct {
  uint16_t cold_water;        // 냉수 사용량
  uint16_t warm_water;        // 온수 사용량
  uint16_t sparkling;         // 탄산수 사용량
} water_amount_t;

typedef struct {

  basic_info_t  basic_info;
  state_info_t  state_info;
  sensor_info_t sensor_info;
  valve_info_t  valve_info;
  water_amount_t  water_amount;

}youngone_device_t;
extern youngone_device_t *pYoungWon;


/*******************************************************************************
 *  user setup  --> Serial Flash save
 ******************************************************************************/
enum {
  WATER_NAME_COLD=0,
  WATER_NAME_HOT,
  WATER_SPARKLING,
  WATER_NAME_MAX
};

enum {
  BOOT_POWER_ON=0,
  BOOT_SOFTWARE_RESET,
  BOOT_HARDWARE_RESET,
  BOOT_UPDATE_RESET,
  BOOT_MODE_MAX
};

#define FLASH_REGISTERED_FAIL     0
#define FLASH_REGISTERED_SUCCESS  10

#define Wi_Fi_LOAD_AP_FAIL      0
#define Wi_Fi_LOAD_AP_SUCCESS   1

#define Wi_Fi_AP_SCANNING       0
#define Wi_Fi_AP_FOUND          1
#define Wi_Fi_AP_CONNECT        1


#define COLD_WATER_MODE    2
#define HOT_WATER_MODE     3
#define SPARKLING_MODE     4

typedef struct {
  char register_mode;         // flash save mode
  char boot_mode;             // POWER ON | SOFTWARE RESET | UPDATE RESET | WATCHDOG RESET
  char  dummy1;
  char  dummy2;

  char wifi_mode;
  char wifi_ap_mode;
  char wifi_sec_type;         // security
  char dummy3;

  char wifi_ssid[64];         // 사용자 ID
  char wifi_password[64];     // 비밀번호 또는 토큰
  char wifi_url[128];         // Web Server URL

  uint16_t water_flow[WATER_NAME_MAX];
  uint8_t  water_temp[WATER_NAME_MAX];
  uint8_t  dummy4;

  uint32_t cold_water_kWh;      // 냉수 소비 전력 량..
  uint32_t warm_water_kWh;      // 온수 소비 전력 량..
  uint32_t valve_flow_run_time; // 원수 급수밸드 동작 시간
} user_flash_info_t;

extern user_flash_info_t user_flash_info;


typedef struct {
  char WS_HOST_NAME[32];
  char WS_RESOURCE_NAME[32];
  char WS_SERVER_IP_ADDR[32];
  int  WS_PORT;

} ws_url_t;
extern ws_url_t ws_url;


extern char WS_SERVER_IP[32];
extern char WS_PATH_NAME[32];
extern int  WS_PORT;

void user_device_attach(void);



#endif
