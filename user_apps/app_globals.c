/*******************************************************************************
 * @file  app_globals.c
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

/**
 * Include files
 * */


#include "../app.h"

tyBITS  wlan_bit_state;
tyBITS  device_bit_state;
tyBITS  module_bit_state;
tyBITS  module_bit_error;
tyBITS  active_bit_state;
tyBITS  valve_bit_state;
tyBITS  ws_send_bit_state;
tyBITS  user_info_bit_state;

uint32_t ws_send_elapsed_time = 0;
uint16_t ws_send_waiting_time = 0;         // 전송을 보내도 대략 1초 뒤 다음 데이터를 보낸다.

uint16_t water_amount_time = 0;


periodic_timer_t    periodic_timer;
user_calendar_t     user_calendar;
user_usart_event_t  user_usart_event;
user_ws_app_t       user_ws_app;

user_flash_info_t   user_flash_info;
user_info_t         user_info;         // 사용자 제어 정보
ws_url_t            ws_url;



youngone_device_t *pYoungWon;

char authorize_idTag[32];
char authorize_api_key[32];

char WS_SERVER_IP[32];
char WS_PATH_NAME[32];
int  WS_PORT;



void user_device_attach(void)
{
  memset(&periodic_timer, 0, sizeof(periodic_timer_t));
  memset(&user_calendar, 0, sizeof(user_calendar_t));
  memset(&user_usart_event, 0, sizeof(user_usart_event_t));

  pYoungWon = (youngone_device_t *)malloc(sizeof(youngone_device_t));
  memset(pYoungWon, 0, sizeof(youngone_device_t));


  user_info.water_recipe.cold_flow = 120;
  user_info.water_recipe.warm_flow = 250;
  user_info.water_recipe.sparkling = 550;
}

