/***************************************************************************/
/**
 * @file ulp_timer_example.c
 * @brief ULP Timer example
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licenser of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
/*==================================================================================
 * @brief : This file contains example application for ULP-Timer and LED0 toggle
 * @section Description :
 * This example shows how to use ULP timer to toggle LED at 1sec periodic rate,
   configured with default parameters.
 * Timer is configured to generate interrupts upon expiration and toggles LED.
 * After toggling LED for five times, timer stops and configured with new
 * parameters and toggles LED again five times.
 * After that timer is stopped and and switches the power state from High power
 to
 * Ultra low power and configured with new parameters and toggles LED again five
 times.
 * similarly timer is stopped and switches the power state from Ultra Low Power
 to
 * High power and configured with new parameters and toggles LED again five
 times.
 * At the end of this example "Unregistered timer timeout callback, on timer
 * operation completion"
 ===================================================================================*/



#include "sl_si91x_ulp_timer.h"
#include "sl_ulp_timer_instances.h"
#include <sl_si91x_ulp_timer.h>
#include "sl_si91x_calendar.h"

#include "../app.h"


//  SL_ULP_TIMER_CLK_TYPE> Clock Type
//  SL_ULP_TIMER_CLK_TYPE_STATIC=> Static (auto select)
//  SL_ULP_TIMER_CLK_TYPE_DYNAMIC=> Dynamic
//  Selection of the Timer CLK Type.
#define SL_ULP_TIMER0_CLK_TYPE    SL_ULP_TIMER_CLK_TYPE_STATIC

// SL_ULP_TIMER_SYNC_TO_ULPSS_PCLK> Sync to ULPSS PCLK
// Enable or disable sync to ULPSS pclock.
// Default: 0
#define SL_ULP_TIMER0_SYNC_TO_ULPSS_PCLK 0

//  SL_ULP_TIMER_SKIP_SWITCH_TIME> Wait for switching timer clock
//  1 : Enable waiting for switching timer clk & 0 : Skip waiting for switching timer clk.
//  Default: 0
#define SL_ULP_TIMER0_SKIP_SWITCH_TIME 0

//  SL_ULP_TIMER_CLK_INPUT_SOURCE> clock source
//   ULP_TIMER_REF_CLK_SRC=> Ref clock
//   ULP_TIMER_32KHZ_RO_CLK_SRC=> 32KHZ_RO
//   ULP_TIMER_32KHZ_RC_CLK_SRC=> 32KHZ_RC
//   ULP_TIMER_32KHZ_XTAL_CLK_SRC=> 32KHZ_XTAL
//   ULP_TIMER_MHZ_RC_CLK_SRC=> MHZ_RC (auto select)
//   ULP_TIMER_20MHZ_RO_CLK_SRC=> 20MHZ_RO
// Selection of the Clock source
#define SL_ULP_TIMER0_CLK_INPUT_SOURCE \
    ULP_TIMER_MHZ_RC_CLK_SRC //default timer clock input source is ref clock

// <<< end of configuration section >>>
ulp_timer_clk_src_config_t ulp_timer_clk_handle = {
  .ulp_timer_clk_type           = SL_ULP_TIMER0_CLK_TYPE,
  .ulp_timer_sync_to_ulpss_pclk = SL_ULP_TIMER0_SYNC_TO_ULPSS_PCLK,
  .ulp_timer_clk_input_src      = SL_ULP_TIMER0_CLK_INPUT_SOURCE,
  .ulp_timer_skip_switch_time   = SL_ULP_TIMER0_SKIP_SWITCH_TIME,
};




/*******************************************************************************
 ***************************  Defines / Macros  ********************************
 ******************************************************************************/
#define ONE_SEC                       1000000 // 1 second in microseconds
#define HUN_MS                         100000 // 100 millisecond in microseconds
#define FIFTY_MS                        50000 //  50 millisecond in microseconds
#define TEN_MS                          10000 //  10 millisecond in microseconds
#define TIME_INT_ON_SEC   0
#define TIME_INT_hUN_MS   1
#define TIME_INT_FIFTY_MS 2
#define TIME_INT_TEN_MS   3
#define TIMER0_INT_TIME TIME_INT_FIFTY_MS

#if(TIMER0_INT_TIME == TIME_INT_ON_SEC)
#define TIME_IN_MICROSECONDS          ONE_SEC //  1sec timer timeout in seconds
#elif(TIMER0_INT_TIME == TIME_INT_hUN_MS)
#define TIME_IN_MICROSECONDS          HUN_MS //  100ms timer timeout in seconds
#elif(TIMER0_INT_TIME == TIME_INT_FIFTY_MS)
#define TIME_IN_MICROSECONDS          FIFTY_MS //  100ms timer timeout in seconds
#else
#define TIME_IN_MICROSECONDS          TEN_MS //   10ms timer timeout in seconds
#endif

#define CLOCKS_PER_MICROSECONDS_20MHZ 20      // clock cycles per microsecond for 20 mhz clock frequency
#define CLOCKS_PER_MICROSECONDS_32MHZ 32      // clock cycles per microsecond for 32 MHz clock frequency

/*****************************************************************************
 * ULP Timer-match value calculation :
 * For one microsecond type, match-value = number of microseconds
 * For 256 microsecond type, match-value = (time in microseconds)/256
 * For normal up/down counter type, timer =  (number of clock cycles per
 *microseconds * time in microseconds)
 ******************************************************************************/
#define SL_TIMER_MATCH_VALUE \
  ((CLOCKS_PER_MICROSECONDS_20MHZ) * (TIME_IN_MICROSECONDS)) /* Timer match value for down-counter type with 20mhz clock */

#define ONE_SEC_MATCH_VALUE_256US_TYPE 3906 // Timer match value for 1-sec, in 256us type

#define ONE_SEC_MATCH_VALUE_1US_TYPE 1000000 // Timer match value for 1-sec, in 1us type

#define SL_TIMER_MATCH_VALUE_MHZ \
  ((CLOCKS_PER_MICROSECONDS_32MHZ) * (TIME_IN_MICROSECONDS)) /*Timer match value for down-counter type with mhz clock */


#define ONE_SEC_MATCH_VALUE_256US_TYPE_MHZ 3906 // Timer match value for 1-sec, in 256us type for 32 mhz clock frequency

#define ONE_SEC_MATCH_VALUE_1US_TYPE_MHZ \
  1000000 /* Timer match value for 1-sec, in 1us type for 32 mhz clock  frequency */


#define ZERO_INTERRUPT_COUNT 0 // Count for zeroth timeout interrupt

#define FIFTH_INTERRUPT_COUNT 5 // Count for fifth timeout interrupt

#define TENTH_INTERRUPT_COUNT 10 // Count for tenth timeout interrupt

// Macros used to construct ulp-timer instance
#define ULP_TIMER_INSTANCE                   0 // timer insatnce used, pass selected timer instance number in place of '0'
#define _CONCAT_TWO_TOKENS(token_1, token_2) token_1##token_2
#define CONCAT_TWO_TOKENS(token_1, token_2)  _CONCAT_TWO_TOKENS(token_1, token_2)
#define SL_ULP_TIMER_HANDLE                  CONCAT_TWO_TOKENS(sl_timer_handle_timer, ULP_TIMER_INSTANCE)
#define SL_ULP_TIMER_CALLBACK                CONCAT_TWO_TOKENS(on_timeout_callback, ULP_TIMER_INSTANCE)

/*******************************************************************************
 **********************  Local Function prototypes   ***************************
 ******************************************************************************/
static void SL_ULP_TIMER_CALLBACK(void);
/*******************************************************************************
 **********************  Local variables   ***************************
 ******************************************************************************/

//sl_status_t status;
//static uint8_t interrupt_count = 0;

/*******************************************************************************
 ******************************  Data Types  ***********************************
 ******************************************************************************/
// Enum for different transmission scenarios
typedef enum {
  SL_ULP_TIMER_PROCESS_ACTION,         // ulp timer event triggering and operational transforming
  SL_ULP_TIMER_POWER_STATE_TRANSITION, // ulp timer power transition mode
  SL_ULP_TIMER_TRANSMISSION_COMPLETED, // ulp timer transmission completed mode
} ulp_timer_enum_t;


/*******************************************************************************
 ***************************  Defines / Macros  ********************************
 ******************************************************************************/
#define MAX_SECOND          60u        // Total seconds in one minute
#define MAX_MINUTE          60u        // Total minutes in one hour
#define MAX_HOUR            24u        // Total hours in one day
#define SECONDS_IN_HOUR     3600u      // Total seconds in one hour
#define UNIX_TEST_TIMESTAMP 981117600u // Unix Time Stamp for 02/02/2001, 18:10:0
#define MS_DEBUG_DELAY      1000u      // Debug prints after every 1000 counts (callback trigger)

#define DEFAULT_CENTURY      2u
#define DEFAULT_YEAR         26u
#define DEFAULT_MONTH        April
#define DEFAULT_DAY_OF_WEEK  Monday
#define DEFAULT_DAY          13u
#define DEFAULT_HOUR         16u
#define DEFAULT_MINUTE       10u
#define DEFAULT_SECONDS      10u
#define DEFAULT_MILLISECONDS 100u







/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/
void ulp_timer_init(void)
{
  sl_status_t status;

  do {
    // Timer Peripheral input clock source configuration, common for all
    // ulp-timer instances
    status = sl_si91x_ulp_timer_init(&ulp_timer_clk_handle);


    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_si91x_ulp_timer_init : Invalid Parameters, Error Code : %lu \n", status);
      break;
    }
    DEBUGOUT("Successfully Configured ULP-timer clock input source \n");

    // Updating timer match-value with 32 mhz
    SL_ULP_TIMER_HANDLE.timer_match_value = SL_TIMER_MATCH_VALUE_MHZ;
    // Configuring timer instance parameters: mode-periodic, type-1us,
    // match-value: 1second
    status = sl_si91x_ulp_timer_set_configuration(&(SL_ULP_TIMER_HANDLE));
    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_si91x_ulp_timer_set_configuration : Invalid Parameters "
      "Error Code : %lu \n",
      status);
      break;
    }
    DEBUGOUT("Successfully Configured ULP-timer parameters with mhz "
    "parameters \r\n");

    // Registering timeout callback for the selected timer instance, which will
    // also enable its interrupt
    status = sl_si91x_ulp_timer_register_timeout_callback(ULP_TIMER_INSTANCE, &(SL_ULP_TIMER_CALLBACK));
    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_si91x_ulp_timer_timeout_callback_register : Invalid "
      "Parameters Error Code : %lu \n",
      status);
      break;
    }
    DEBUGOUT("Successfully Registered timer instance timeout callback \r\n");
    // Starting Timer instance with default parameters
    status = sl_si91x_ulp_timer_start(ULP_TIMER_INSTANCE);
    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_si91x_ulp_timer_start : Invalid Parameters Error Code : %lu \r\n", status);
      break;
    }
    DEBUGOUT("Successfully started ulp-timer instance with default parameters \r\n");

  } while(false);

  DEBUGOUT("Successfully started ulp-timer instance with default parameters \r\n");
}




/*******************************************************************************
 * Callback function of ulp-timer instance
 *
 * @param none
 * @return none
 ******************************************************************************/
// Note : Remove all debugouts for exact delays
 static void SL_ULP_TIMER_CALLBACK(void)
{
   user_app_set_event(USER_APP_TIMER_CALLBACK);
}


 /*******************************************************************************
  * 1 sec interval time
  ******************************************************************************/
 void periodic_timer_1sec_handler(void)
 {
   if(water_amount_time>0)  water_amount_time--;              // 출수 량 전송은 출수 동작이 30분 동안 없을 경우에만 전송을 한다

   user_app_set_event(USER_APP_Wi_Fi_CALLBACK);                // 1sec Wi-Fi Monitor callback

   if(user_ws_app.need_reconnect==WS_RECONNECT_START) {       // 재 시작 동작 일 경우에만 동작을 한다...
     if(user_ws_app.retry_tmout>0) {                          // 5sec interval time
         user_ws_app.retry_tmout--;
     }
   }
 }

 void periodic_timer_100ms_handler(void)
 {
   user_app_set_event(USER_APP_WS_CALLBACK);                  // 100ms websocket callback

   if(user_ws_app.send_data_tmout>0)
     user_ws_app.send_data_tmout--;

   youngwon_send_water_flow_out_control();
}

 /*******************************************************************************
  * 50ms timer1 event action
  ******************************************************************************/
void ulp_timer_event_action_process(void)
{
  periodic_timer.cb_50ms_count++;
  periodic_timer.cb_50ms_count%=20;

  if(!periodic_timer.cb_50ms_count) {
      periodic_timer_1sec_handler();
  }

  if(periodic_timer.cb_100ms_event) {
      periodic_timer_100ms_handler();
  }
  periodic_timer.cb_100ms_event=periodic_timer.cb_100ms_event==0?1:0;

//------------------------------------------------------------------------------
  if(user_usart_event.read_tmout>0)
    user_usart_event.read_tmout--;

  if(ws_send_waiting_time>0)                                 // 메시지가 충돌 하는 것을 방지 하기 위해 1초 지연을 둔다...
    ws_send_waiting_time--;

#if 1
  youngone_rx_tick_50ms();
#else
  if((user_usart_event.read_map==SET_USART_EVENT) && (user_usart_event.read_tmout==TIME_OVER)) {
      user_usart_event.read_map=CLR_USART_EVENT;

      user_app_set_event(USER_APP_USART_CALLBACK);                // 사용자 USART CALLBACK를 호출 한다..
  }
#endif

//-----------------------------------------------------------------------------
//  BUTTON 선택 여부 확인....
//-----------------------------------------------------------------------------
  if(button_state.action_timeout>0)  button_state.action_timeout--;
  if(button_state.action_timeout == 0) {
      button_state.action_count = 0;
  }

}

/*******************************************************************************
 * Function to print date and time from given structure
 *
 * @param[in] data pointer to the datetime structure
 * @return none
 ******************************************************************************/
static void calendar_print_datetime(sl_calendar_datetime_config_t data)
{
  DEBUGOUT("\n***Calendar time****\n");
  DEBUGOUT("Time Format: hour:%d, min:%d, sec:%d, msec:%d\r\n", data.Hour, data.Minute, data.Second, data.MilliSeconds);
  DEBUGOUT("Date Format: DayOfWeek DD/MM/YY: %.2d %.2d/%.2d/%.2d \r\n", data.DayOfWeek, data.Day, data.Month, data.Year);
  DEBUGOUT(" Century: %d\n", data.Century);
}

/*******************************************************************************
 * Calendar initialization function
 * [FIX-5] 기존에는 부팅할 때마다 DEFAULT 날짜(2026-04-13 16:10)로 무조건 set 했음.
 *         → init 후 현재 값을 읽기만 하고, 유효성 판단/기본값 설정은
 *           user_app_calendar_init()에서 한 곳에서만 한다.
 ******************************************************************************/
void calendar_initialization(void)
{
  sl_calendar_datetime_config_t datetime_config;
  sl_calendar_datetime_config_t get_datetime;

  sl_status_t status;

    // Initialization of calendar
  sl_si91x_calendar_init();

  //Setting datetime for Calendar
  status = sl_si91x_calendar_build_datetime_struct(&datetime_config,
                                                   DEFAULT_CENTURY,
                                                   DEFAULT_YEAR,
                                                   DEFAULT_MONTH,
                                                   DEFAULT_DAY_OF_WEEK,
                                                   DEFAULT_DAY,
                                                   DEFAULT_HOUR,
                                                   DEFAULT_MINUTE,
                                                   DEFAULT_SECONDS,
                                                   DEFAULT_MILLISECONDS);

  if (status != SL_STATUS_OK) {
    DEBUGOUT("sl_si91x_calendar_build_datetime_struct: Invalid Parameters, Error Code : %lu \r\n", status);
  }

  DEBUGOUT("Successfully built datetime structure\r\n");
  status = sl_si91x_calendar_set_date_time(&datetime_config);
  if (status != SL_STATUS_OK) {
    DEBUGOUT("sl_si91x_calendar_set_date_time: Invalid Parameters, Error Code : %lu \r\n", status);
  }

  DEBUGOUT("Successfully set calendar datetime\r\n");
  // Printing datetime for Calendar
  status = sl_si91x_calendar_get_date_time(&get_datetime);
  if (status != SL_STATUS_OK) {
    DEBUGOUT("sl_si91x_calendar_get_date_time: Invalid Parameters, Error Code : %lu \r\n", status);
  }

  DEBUGOUT("Successfully fetched the calendar datetime \r\n");
  calendar_print_datetime(get_datetime);

}

void calendar_write_datetime(user_calendar_t *p)
{
  sl_calendar_datetime_config_t datetime;
  sl_status_t status;

  //Setting datetime for Calendar
  status = sl_si91x_calendar_build_datetime_struct(&datetime,
                                                   DEFAULT_CENTURY,
                                                   p->Year,
                                                   p->Month,
                                                   DEFAULT_DAY_OF_WEEK,
                                                   p->Day,
                                                   p->Hour,
                                                   p->Minute,
                                                   p->Second,
                                                   DEFAULT_MILLISECONDS);


  status = sl_si91x_calendar_set_date_time(&datetime);

  if (status != SL_STATUS_OK) {
    DEBUGOUT("sl_si91x_calendar_set_date_time, Error Code : %lu \r\n", status);
  }

}

void calendar_read_datetime(user_calendar_t *p)
{
  sl_calendar_datetime_config_t datetime;
  sl_status_t status;

  status = sl_si91x_calendar_get_date_time(&datetime);

  p->Year = datetime.Year;
  p->Month = datetime.Month;
  p->Day = datetime.Day;

  p->Hour = datetime.Hour;
  p->Minute = datetime.Minute;
  p->Second = datetime.Second;

  p->update =  user_info.calendar_update;


  if (status != SL_STATUS_OK) {
    DEBUGOUT("sl_si91x_calendar_get_date_time, Error Code : %lu \r\n", status);
  }

}
/*************************************************************************
 *  user calendar setup
 ************************************************************************/
void user_app_calendar_init(void)
{
  calendar_initialization();

  memset(&user_calendar, 0, sizeof(user_calendar_t));
  calendar_read_datetime(&user_calendar);

  if(user_calendar.Year<26 || user_calendar.Year>56) {
      user_calendar.Year = 25;            // sample date & time
      user_calendar.Month = 1;
      user_calendar.Day = 1;
      user_calendar.Hour = 0;
      user_calendar.Minute = 0;
      user_calendar.Second = 0;

      calendar_write_datetime(&user_calendar);

      memset(&user_calendar, 0, sizeof(user_calendar_t));
      calendar_read_datetime(&user_calendar);
  }

  DEBUGOUT("Time Format: hour:%d, min:%d, sec:%d\r\n", user_calendar.Hour, user_calendar.Minute, user_calendar.Second);
  DEBUGOUT("Date Format: DD/MM/YY: %.2d/%.2d/20%.02d \r\n", user_calendar.Day, user_calendar.Month, user_calendar.Year);
}


/*************************************************************************
 *  하루가 지나가는 시점...자정이 되면 수집된 정보를 초기화 하고 누적을 한 뒤 전송 한다.
 ************************************************************************/
void over_the_pass_day(void)
{
  user_calendar_t date_time;

  calendar_read_datetime(&date_time);

  if(user_info.calendar_update == SUCCESS) {
      if(date_time.Hour == 0) {
          if((user_calendar.over_day == 0) && (date_time.Minute >= 10)) {    // 웹 & 앱 명령을 우선으로 하고 10분 동안 데이터를 받지 못하면 초기화 명령을 보낸다.
              user_calendar.over_day = 1;
              youngwon_software_verification_initial(0x07); // control board에 초기화 명령으로 값을 초기화 한다.
          }
      }

      else {
          user_calendar.over_day = 0;
      }
  }
}
