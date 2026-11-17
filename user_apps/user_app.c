/*******************************************************************************
 * @file  user_app.c
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
/*************************************************************************
 *
 */

/*================================================================================
 * @brief : This file contains example application for Wlan Station BLE
 * Provisioning
 * @section Description :
 * This application explains how to get the WLAN connection functionality using
 * BLE provisioning.
 * Silicon Labs Module starts advertising and with BLE Provisioning the Access Point
 * details are fetched.
 * Silicon Labs device is configured as a WiFi station and connects to an Access Point.
 =================================================================================*/



#if 0
#if defined(SLI_SI917) || defined(SLI_SI917B0)
#include "RTE_Device_917.h"
#else
#include "RTE_Device_915.h"
#endif

#include "sl_status.h"
#include "em_assert.h"

#include <rsi_ble_apis.h>
#include <rsi_common_apis.h>
#else
#include "sl_board_configuration.h"
#include "sl_wifi.h"
#include "sl_wifi_callback_framework.h"
#include "cmsis_os2.h"
#include "sl_utility.h"
#include "sl_si91x_hal_soc_soft_reset.h"

//BLE Specific inclusions
#include <rsi_ble_apis.h>
#include "ble_config.h"
#include "rsi_ble_common_config.h"
#include <rsi_common_apis.h>

#include "sl_si91x_driver.h"
#include "sl_status.h"

#endif

#include "../app.h"



/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

// [FIX-2] 이벤트 맵 + 세마포어 → CMSIS-RTOS2 Thread Flags 로 교체
//   - osThreadFlagsSet()은 ISR에서 호출 가능하고 atomic → set/clear race 제거
//   - osThreadFlagsWait()가 받는 순간 flag를 clear → 처리 중 들어온 이벤트가 지워지지 않음
//   - USER_APP_* 값(0x01,0x02,0x04...)을 비트마스크 그대로 사용 (BIT() 이중 적용 제거)
#define USER_APP_EVENT_ALL  (USER_APP_TIMER_CALLBACK | USER_APP_USART_CALLBACK | \
                             USER_APP_WS_CALLBACK    | USER_APP_NVM_CALLBACK   | \
                             USER_APP_BLE_CALLBACK   | USER_APP_Wi_Fi_CALLBACK)

// 타이머 틱 유실 방지: ISR은 카운터만 증가, 스레드가 밀린 틱 수만큼 처리
#define USER_APP_TIMER_CATCHUP_MAX  20u      // 최대 1초(50ms x 20)까지만 따라잡기

static osThreadId_t      user_thread_id = NULL;
static volatile uint32_t user_timer_tick_isr = 0;   // ISR에서만 증가 (single writer)
static uint32_t          user_timer_tick_done = 0;  // 스레드에서만 증가 (single writer)

/*******************************************************************************
 **************************   LOCAL VARIABLES   ********************************
 ******************************************************************************/

#define M4_FLASH_ADDRESS  0x8300000
#define NWP_FLASH_ADDRESS 0

#define LENGTH_OF_BUFFER           (1024*4)
#define FLASH_SECTOR_ERASE_ENABLE  1
#define FLASH_SECTOR_ERASE_DISABLE 0

unsigned char user_data_buffer[LENGTH_OF_BUFFER];

/*******************************************************************************
 *  sets the specific event. (ISR / 다른 스레드에서 호출 가능)
 *  event_num : USER_APP_xxx_CALLBACK 비트마스크 값
 *******************************************************************************/
void user_app_set_event(uint32_t event_num)
{
  if (event_num == USER_APP_TIMER_CALLBACK) {
    user_timer_tick_isr++;                    // 틱 수를 세어 두고 flag는 "깨우기" 용도로만 사용
  }

  if (user_thread_id != NULL) {
    (void)osThreadFlagsSet(user_thread_id, event_num & USER_APP_EVENT_ALL);
  }
}

/*******************************************************************************
 *  clear the specific event.
 *  Thread flags는 osThreadFlagsWait()에서 자동 clear 되므로 보통 호출할 필요 없음.
 *  (기존 호출부 호환용으로 남겨둠 - user_app_thread 안에서만 의미가 있음)
 *******************************************************************************/
void user_app_clear_event(uint32_t event_num)
{
  if (osThreadGetId() == user_thread_id) {
    (void)osThreadFlagsClear(event_num & USER_APP_EVENT_ALL);
  }
}

/*************************************************************************
 *  밀린 50ms 타이머 틱 처리
 ************************************************************************/
static void user_app_timer_process(void)
{
  uint32_t pending = user_timer_tick_isr - user_timer_tick_done;   // wrap-around 안전

  if (pending > USER_APP_TIMER_CATCHUP_MAX) {
    DEBUGOUT("timer tick overrun : %lu\r\n", pending);
    user_timer_tick_done += (pending - USER_APP_TIMER_CATCHUP_MAX);
    pending = USER_APP_TIMER_CATCHUP_MAX;
  }

  while (pending--) {
    ulp_timer_event_action_process();
    button_event_action_process();
    user_timer_tick_done++;
  }
}

/*************************************************************************
 *  user app thread...usart & timer event
 ************************************************************************/
void user_app_thread(void *argument)
{
  UNUSED_PARAMETER(argument);
  uint32_t flags;

  DEBUGOUT("user_app_thread initialization is successful\r\n");

  // 주변장치 콜백 등록 전에 thread id를 먼저 확보한다.
  user_thread_id = osThreadGetId();

  user_device_attach();

  user_app_calendar_init();                               // data & time 초기화
  ulp_timer_init();                                       // timer0 초기화
  usart_async_init();                                     // usart sync 초기화...
  button_gpio_init();

  user_timer_tick_done = user_timer_tick_isr;             // 초기화 중 쌓인 틱은 버린다

  while (1)  {

    flags = osThreadFlagsWait(USER_APP_EVENT_ALL, osFlagsWaitAny, osWaitForever);
    if (flags & osFlagsError) {
      continue;
    }

    // 1. 50ms timer (내부에서 USART / WS 이벤트를 추가로 set 할 수 있음)
    if (flags & USER_APP_TIMER_CALLBACK) {
      user_app_timer_process();
    }

    // 2. USART frame 수신 완료
    if (flags & USER_APP_USART_CALLBACK) {
      user_usart_receive_data_action();
    }

    // 3. 100ms websocket / 자정 체크
    if (flags & USER_APP_WS_CALLBACK) {
      youngwon_ws_message_callback();
      over_the_pass_day();
    }

    // 4. NVM 저장
    if (flags & USER_APP_NVM_CALLBACK) {
      M4_erase_user_common_flash();
      M4_write_user_common_flash(&user_flash_info);
    }

    // 5. Wi-Fi link Monitor
    if (flags & USER_APP_Wi_Fi_CALLBACK) {
        wifi_link_monitor();
        wifi_reconnect_event_handler();
    }

    // 5. BLE 요청: Wi-Fi 정보 초기화 후 리셋 (항상 마지막에 처리)
    //    [FIX] 기존에는 여기서 USER_APP_NVM_CALLBACK을 잘못 clear 했음 → flags 방식에서는 불필요
    if (flags & USER_APP_BLE_CALLBACK) {
      user_flash_info.wifi_mode    = Wi_Fi_LOAD_AP_FAIL;   // Wi-Fi ssid & password clear
      user_flash_info.wifi_ap_mode = Wi_Fi_AP_SCANNING;
      M4_erase_user_common_flash();
      M4_write_user_common_flash(&user_flash_info);

      osDelay(500);
      sl_si91x_soc_nvic_reset();
    }

    // [FIX-2] 기존의 이벤트마다 osDelay(25ms) 는 제거 (이벤트 처리 지연/적체 원인)
  }
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
//

void M4_init_user_common_flash(void)
{
  M4_read_user_common_flash(&user_flash_info);

  if(user_flash_info.register_mode == FLASH_REGISTERED_SUCCESS) { // 설정 등록 있음

  } else {                                                        // 설정 등록 없음
      user_flash_info.register_mode = FLASH_REGISTERED_SUCCESS;
      user_flash_info.boot_mode = BOOT_POWER_ON;                  // POWER ON MODE
      user_flash_info.wifi_mode = Wi_Fi_LOAD_AP_FAIL;             // Wi-Fi AP Mode 없음...

      user_flash_info.water_flow[WATER_NAME_COLD] = 120;
      user_flash_info.water_temp[WATER_NAME_COLD] = 1;            // 에코 모드

      user_flash_info.water_flow[WATER_NAME_HOT] = 250;
      user_flash_info.water_temp[WATER_NAME_HOT] = 75;

      user_flash_info.water_flow[WATER_SPARKLING] = 550;
      user_flash_info.water_temp[WATER_SPARKLING] = 0;

      user_flash_info.cold_water_kWh = 0;                         // 냉수 소비 전력 량..
      user_flash_info.warm_water_kWh = 0;                         // 온수 소비 전력 량..
      user_flash_info.valve_flow_run_time = 0;                    // 원수 급수밸드 동작 시간

      M4_erase_user_common_flash();
      M4_write_user_common_flash(&user_flash_info);
  }
}


/*******************************************************************************
 *  USER AREA FLASH READ & WRITE & ERASE
 ******************************************************************************/
void M4_read_user_common_flash(user_flash_info_t *out_info)
{
  // clear data_read_buffer with 0x00
  memset(user_data_buffer, 0x00, LENGTH_OF_BUFFER * sizeof(unsigned char));

  // Read from M4 flash memory
  memcpy((uint8_t *)user_data_buffer, (uint8_t *)M4_FLASH_ADDRESS, LENGTH_OF_BUFFER);
  memcpy(out_info, user_data_buffer, sizeof(user_flash_info_t));


  printf("user_common : [%d][%d][%d]\r\n",
         user_flash_info.water_temp[WATER_NAME_COLD],
         user_flash_info.water_temp[WATER_NAME_HOT],
         user_flash_info.water_temp[WATER_SPARKLING]);

}

void M4_erase_user_common_flash(void)
{
  sl_status_t status;

  // M4 flash memory
  status = (int)sl_si91x_command_to_write_common_flash(M4_FLASH_ADDRESS,
                                                       user_data_buffer,
                                                       FLASH_SECTOR_SIZE,
                                                       FLASH_SECTOR_ERASE_ENABLE);

  if (status == SL_STATUS_OK) {
    printf("\r\nSuccessfully erased the MP4 flash memory.\r\n");
  }
  else {
    printf("\r\nError erasing MP4 flash memory.\r\n");
    return;
  }
}

void M4_write_user_common_flash(user_flash_info_t *in_info)
{
  sl_status_t status;

  memset(user_data_buffer, 0, sizeof(user_data_buffer));
  memcpy(user_data_buffer, in_info, sizeof(user_flash_info_t));


  status = (int)sl_si91x_command_to_write_common_flash(M4_FLASH_ADDRESS,
                                                       user_data_buffer,
                                                       LENGTH_OF_BUFFER,
                                                       FLASH_SECTOR_ERASE_DISABLE);

  if (status == SL_STATUS_OK) {
    printf("\r\nData successfully written to M4 flash memory.\r\n");
  }
  else {
    printf("\r\nError writing data to M4 flash memory.\r\n");
    return;
  }
}

/******************************************************************************
 * WI-FI만 초기화 한다...
 ******************************************************************************/
void M4_wi_fi_user_common_flash(void)
{
  user_flash_info.register_mode = FLASH_REGISTERED_SUCCESS;
  user_flash_info.boot_mode = BOOT_POWER_ON;                  // POWER ON MODE
  user_flash_info.wifi_mode = Wi_Fi_LOAD_AP_FAIL;             // Wi-Fi AP Mode 없음...

  M4_erase_user_common_flash();
  M4_write_user_common_flash(&user_flash_info);

}
