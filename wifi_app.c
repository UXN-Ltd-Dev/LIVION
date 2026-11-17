/*******************************************************************************
* @file  wifi_app.c
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

//! SL Wi-Fi SDK includes
#include "sl_constants.h"
#include "sl_wifi.h"
#include "sl_wifi_callback_framework.h"
#include "sl_net.h"
#include "sl_net_si91x.h"
#include "sl_utility.h"

#include "sl_si91x_hal_soc_soft_reset.h"

#include "cmsis_os2.h"
#include <string.h>

#include "wifi_config.h"
#include "rsi_common_apis.h"

#include "rsi_driver.h"

#include "app.h"

// WLAN include file for configuration

extern osSemaphoreId_t wlan_thread_sem;

#define DHCP_HOST_NAME    NULL
#define TIMEOUT_MS        25000
#define WIFI_SCAN_TIMEOUT 15000//10000

static uint8_t join_retry_count = 0;        // 연결 재시도 횟수
#define MAX_JOIN_RETRY    10                // 최대 재시도 횟수

static uint8_t fail_retry_count = 0;        // 연결 끊어지는 것 횟수
#define LINK_CHECK_FAIL_MAX   5

static uint8_t reconnect_count = 0;
#define MAX_RECONNECT_RETRY   10

static uint8_t wifi_reconnect_pending = 0;   // 1 = 재연결 대기 중
static uint8_t wifi_reconnect_tmout = 0;
#define RECONNECT_PENDING_TIME         5        //

extern void ext_rsi_ble_app_set_event(uint32_t event_num);
extern void change_advertise_device_name(void);


/*
 *********************************************************************************************************
 *                                         LOCAL GLOBAL VARIABLES
 *********************************************************************************************************
 */

sl_wifi_scan_result_t *scan_result          = NULL;
static volatile bool scan_complete          = false;
static volatile sl_status_t callback_status = SL_STATUS_OK;
uint16_t scanbuf_size = (sizeof(sl_wifi_scan_result_t) + (SL_WIFI_MAX_SCANNED_AP * sizeof(scan_result->scan_info[0])));

uint8_t connected = 0, timeout = 0;
uint8_t disconnected = 0, disassosiated = 0;
uint8_t a = 0;

sl_wifi_client_configuration_t access_point = { 0 };
sl_net_ip_configuration_t ip_address        = { 0 };

static uint32_t wlan_app_event_map;

/*
 *********************************************************************************************************
 *                                               DATA TYPES
 *********************************************************************************************************
 */
extern int32_t rsi_ble_start_advertising(void);

extern void wifi_app_send_to_ble(uint16_t msg_type, uint8_t *data, uint16_t data_len);
static sl_status_t show_scan_results();
void wifi_app_set_event(uint32_t event_num);
extern uint8_t coex_ssid[50], pwd[34], sec_type;
uint8_t retry = 1;

uint8_t conn_status;
extern uint8_t magic_word;

/*==============================================*/
/**
 * @fn         wifi_app_set_event
 * @brief      sets the specific event.
 * @param[in]  event_num, specific event number.
 * @return     none.
 * @section description
 * This function is used to set/raise the specific event.
 */
void wifi_app_set_event(uint32_t event_num)
{
  wlan_app_event_map |= BIT(event_num);

  osSemaphoreRelease(wlan_thread_sem);

  return;
}

/*==============================================*/
/**
 * @fn         wifi_app_clear_event
 * @brief      clears the specific event.
 * @param[in]  event_num, specific event number.
 * @return     none.
 * @section description
 * This function is used to clear the specific event.
 */
void wifi_app_clear_event(uint32_t event_num)
{
  wlan_app_event_map &= ~BIT(event_num);
  return;
}

/*==============================================*/
/**
 * @fn         wifi_app_get_event
 * @brief      returns the first set event based on priority
 * @param[in]  none.
 * @return     int32_t
 *             > 0  = event number
 *             -1   = not received any event
 * @section description
 * This function returns the highest priority event among all the set events
 */
int32_t wifi_app_get_event(void)
{
  uint32_t ix;

  for (ix = 0; ix < 32; ix++) {
    if (wlan_app_event_map & (1 << ix)) {
      return ix;
    }
  }

  return (-1);
}

// rejoin failure callback handler in station mode
sl_status_t join_callback_handler(sl_wifi_event_t event, char *result, uint32_t result_length, void *arg)
{
  UNUSED_PARAMETER(event);
  UNUSED_PARAMETER(result);
  UNUSED_PARAMETER(result_length);
  UNUSED_PARAMETER(arg);

  LOG_PRINT(">>> join_callback FIRED, event=0x%lX\r\n", (uint32_t)event);

  // update wlan application state
  disconnected = 1;
  connected    = 0;

  wifi_app_set_event(WIFI_APP_DISCONNECTED_STATE);

  return SL_STATUS_OK;
}

static sl_status_t show_scan_results()
{
  SL_WIFI_ARGS_CHECK_NULL_POINTER(scan_result);
  uint8_t *bssid = NULL;
  int length=0;
  LOG_PRINT("%lu Scan results:\r\n", scan_result->scan_count);
#if 0
  if (scan_result->scan_count) {
    LOG_PRINT("\n   %s %24s %s", "SSID", "SECURITY", "NETWORK");
    LOG_PRINT("%12s %12s %s\n", "BSSID", "CHANNEL", "RSSI");

    for (int a = 0; a < (int)scan_result->scan_count; ++a) {
      bssid = (uint8_t *)&scan_result->scan_info[a].bssid;
      LOG_PRINT("%-24s %4u,  %4u, ",
                scan_result->scan_info[a].ssid,
                scan_result->scan_info[a].security_mode,
                scan_result->scan_info[a].network_type);
      LOG_PRINT("  %02x:%02x:%02x:%02x:%02x:%02x, %4u,  -%u\r\n",
                bssid[0],
                bssid[1],
                bssid[2],
                bssid[3],
                bssid[4],
                bssid[5],
                scan_result->scan_info[a].rf_channel,
                scan_result->scan_info[a].rssi_val);
    }
  }
#else
  if (scan_result->scan_count) {

    user_flash_info.wifi_ap_mode = Wi_Fi_AP_SCANNING;
    length = strlen((char *)user_flash_info.wifi_ssid);

    LOG_PRINT("wifi_ssid : %-24s\r\n",  user_flash_info.wifi_ssid);

    for (int a = 0; a < (int)scan_result->scan_count; ++a) {
      bssid = (uint8_t *)&scan_result->scan_info[a].bssid;

      if (strncmp((const char *)scan_result->scan_info[a].ssid, (const char *)user_flash_info.wifi_ssid, length) == 0) {
          user_flash_info.wifi_ap_mode = Wi_Fi_AP_FOUND;

        LOG_PRINT("%-24s %4u,  %4u, ",
                  scan_result->scan_info[a].ssid,
                  scan_result->scan_info[a].security_mode,
                  scan_result->scan_info[a].network_type);
        LOG_PRINT("  %02x:%02x:%02x:%02x:%02x:%02x, %4u,  -%u --> %d\r\n",
                  bssid[0],
                  bssid[1],
                  bssid[2],
                  bssid[3],
                  bssid[4],
                  bssid[5],
                  scan_result->scan_info[a].rf_channel,
                  scan_result->scan_info[a].rssi_val,
                  length);
      }
    }
  }
#endif

  return SL_STATUS_OK;
}

sl_status_t wlan_app_scan_callback_handler(sl_wifi_event_t event,
                                           sl_wifi_scan_result_t *result,
                                           uint32_t result_length,
                                           void *arg)
{
  UNUSED_PARAMETER(arg);
  UNUSED_PARAMETER(result_length);

  scan_complete = true;

  if (SL_WIFI_CHECK_IF_EVENT_FAILED(event)) {
    callback_status = *(sl_status_t *)result;
    return SL_STATUS_FAIL;
  }
  SL_VERIFY_POINTER_OR_RETURN(scan_result, SL_STATUS_NULL_POINTER);
  memset(scan_result, 0, scanbuf_size);
  memcpy(scan_result, result, scanbuf_size);

  if (result_length != 0) {
    callback_status = show_scan_results();
  }

  // Send wlan scan result to BLE module
  wifi_app_send_to_ble(WIFI_APP_SCAN_RESP, (uint8_t *)scan_result, scanbuf_size);

  return SL_STATUS_OK;
}

void wifi_app_task()
{
  int32_t status   = RSI_SUCCESS;
  int32_t event_id = 0;

  // Allocate memory for scan buffer
  scan_result = (sl_wifi_scan_result_t *)malloc(scanbuf_size);
  if (scan_result == NULL) {
    LOG_PRINT("Failed to allocate memory for scan result\r\n");
    return;
  }

  LOG_PRINT("wifi_app_task\r\n");

#if 0
  strncpy(UserInfo.user_id, "UXN_2G", strlen("UXN_2G"));
  UserFlash_SaveData(&UserInfo);

  UserFlash_LoadData(&UserInfo);
  printf("[TEST] user_storage_save!!!--> %s\r\n", UserInfo.user_id);
#endif

  memset(scan_result, 0, scanbuf_size);
  while (1) {
    // checking for events list
    event_id = wifi_app_get_event();
    if (event_id == -1) {
      osSemaphoreAcquire(wlan_thread_sem, osWaitForever);

      // if events are not received loop will be continued.
      continue;
    }

    switch (event_id) {
      case WIFI_APP_INITIAL_STATE: {
        wifi_app_clear_event(WIFI_APP_INITIAL_STATE);
        LOG_PRINT("WIFI App Initial State\r\n");

        /* AWS S3 HTTPS OTA 대비 CA 인증서 1회 등록 */
//        ota_app_load_certificate();

        //! Initialize join fail call back
        sl_wifi_set_join_callback(join_callback_handler, NULL);

        // update wlan application state
        if (magic_word) {
          // clear the served event
          wifi_app_set_event(WIFI_APP_FLASH_STATE);
        } else {
          wifi_app_set_event(WIFI_APP_SCAN_STATE);
        }
      } break;

      case WIFI_APP_UNCONNECTED_STATE: {
        wifi_app_clear_event(WIFI_APP_UNCONNECTED_STATE);
        LOG_PRINT("WIFI App Unconnected State\r\n");

        // Any additional code if required

        osSemaphoreRelease(wlan_thread_sem);
      } break;

      case WIFI_APP_SCAN_STATE: {
        wifi_app_clear_event(WIFI_APP_SCAN_STATE);

        sl_wifi_scan_configuration_t wifi_scan_configuration = { 0 };
        wifi_scan_configuration                              = default_wifi_scan_configuration;

        sl_wifi_set_scan_callback(wlan_app_scan_callback_handler, NULL);

        status = sl_wifi_start_scan(SL_WIFI_CLIENT_2_4GHZ_INTERFACE, NULL, &wifi_scan_configuration);
        if (SL_STATUS_IN_PROGRESS == status) {
          LOG_PRINT("Scanning...\r\n");
          const uint32_t start = osKernelGetTickCount();

          while (!scan_complete && (osKernelGetTickCount() - start) <= WIFI_SCAN_TIMEOUT) {
            osThreadYield();
          }
          status = scan_complete ? callback_status : SL_STATUS_TIMEOUT;
        }

        if (status != SL_STATUS_OK) {
          LOG_PRINT("WLAN Scan Wait Failed, Error Code : 0x%lX\r\n", status);
          wifi_app_set_event(WIFI_APP_SCAN_STATE);


              // 2. SiWG917(Cortex-M4) 시스템 리셋 호출
              // 이 함수는 'core_cm4.h'에 정의되어 있으며, 대부분의 SDK에 기본 포함됩니다.
          __disable_irq();
          NVIC_SystemReset();

          osDelay(1000);
        }

/*******************************************************************************
 * 저장된 SSID와 PASSWORD가 있을 경우...add sodykim
 *******************************************************************************/
        if(user_flash_info.wifi_mode == Wi_Fi_LOAD_AP_SUCCESS) {
          if(user_flash_info.wifi_ap_mode == Wi_Fi_AP_CONNECT) {
              wifi_app_set_event(WIFI_APP_JOIN_STATE);
          } else {
              user_flash_info.wifi_mode = Wi_Fi_LOAD_AP_FAIL;
              user_flash_info.wifi_ap_mode = Wi_Fi_AP_SCANNING;
              rsi_ble_start_advertising();
              LOG_PRINT("\r\nBLE Advertising Started...\r\n");
          }
        }

        osSemaphoreRelease(wlan_thread_sem);
      } break;

      case WIFI_APP_JOIN_STATE: {

        sl_wifi_credential_t cred  = { 0 };
        sl_wifi_credential_id_t id = SL_NET_DEFAULT_WIFI_CLIENT_CREDENTIAL_ID;
        memset(&access_point, 0, sizeof(sl_wifi_client_configuration_t));


        cred.type = SL_WIFI_PSK_CREDENTIAL;
        memcpy(cred.psk.value, pwd, strlen((char *)pwd));

        if (sec_type != SL_WIFI_OPEN) {
          status = sl_net_set_credential(id, SL_NET_WIFI_PSK, pwd, strlen((char *)pwd));
          if (SL_STATUS_OK == status) {
            LOG_PRINT("Credentials set, id : %lu\r\n", id);
          }
          if (status != SL_STATUS_OK) {
            printf("Failed to set client credentials: 0x%lx\r\n", status);
            continue;
          }
        }

        access_point.ssid.length = strlen((char *)coex_ssid);
        memcpy(access_point.ssid.value, coex_ssid, access_point.ssid.length);

        /* sec_type = 7 (WPA3 단독) → 8 (WPA3_TRANSITION) 으로 교체 후 시도 */
        sl_wifi_security_t try_sec = (sl_wifi_security_t)sec_type;

        if (try_sec == SL_WIFI_WPA3) {
            try_sec = SL_WIFI_WPA3_TRANSITION;    /* 8 */
        }

        access_point.security      = sec_type;
        access_point.encryption    = SL_WIFI_DEFAULT_ENCRYPTION;
        access_point.credential_id = id;


        status = sl_wifi_connect(SL_WIFI_CLIENT_2_4GHZ_INTERFACE, &access_point, TIMEOUT_MS);
        LOG_PRINT("SSID=%s PWD=%s sec_type : %d\r\n", access_point.ssid.value, pwd, sec_type);

        if (status != RSI_SUCCESS) {
          timeout = 1;
          wifi_app_clear_event(WIFI_APP_JOIN_STATE);

          join_retry_count++;
          LOG_PRINT("WLAN Connect Failed [%d/%d], Error Code : 0x%lX\r\n",
                    join_retry_count, MAX_JOIN_RETRY, status);

#if 0
          wifi_app_set_event(WIFI_APP_SCAN_STATE);              // 재 스캔을 한 뒤 연결 상태 확인 한다...10회 이상 안될 경우 BLE 동작을 한다.
          osDelay(pdMS_TO_TICKS(100));

          LOG_PRINT("WLAN Connect Failed, Error Code : 0x%lX\r\n", status);
#else
          if (join_retry_count < MAX_JOIN_RETRY) {
              /*------------------------------------------------------
               * 10회 미만 : 재스캔 후 재연결 시도
               *----------------------------------------------------*/
              LOG_PRINT("Retry scan and connect...\r\n");
              osDelay(pdMS_TO_TICKS(1000));          /* 1초 대기 후 재시도 */
              wifi_app_set_event(WIFI_APP_SCAN_STATE);

          } else {
              /*------------------------------------------------------
               * 10회 이상 실패 : BLE provisioning 으로 전환
               * 저장된 자격증명 삭제 후 새로운 SSID/PWD 입력 요청
               *----------------------------------------------------*/
              LOG_PRINT("Max retry reached. Switching to BLE provisioning.\r\n");
              join_retry_count = 0;                  /* 카운터 리셋 */

              /* 저장된 자격증명 초기화 (잘못된 SSID/PWD 가능성) */
              M4_erase_user_common_flash();

              /* BLE provisioning 시작 */
              wifi_app_set_event(WIFI_APP_UNCONNECTED_STATE);
              ext_rsi_ble_app_set_event(0x0F); // RSI_BLE_WLAN_DISCONN_NOTIFY
          }
#endif

          // update wlan application state
          disconnected = 1;
          connected    = 0;
        }
        else {
          LOG_PRINT("WLAN connection is successful\r\n");
          // update wlan application state
          wifi_app_clear_event(WIFI_APP_JOIN_STATE);
          wifi_app_set_event(WIFI_APP_CONNECTED_STATE);
        }

        osSemaphoreRelease(wlan_thread_sem);
        LOG_PRINT("WIFI App Join State\r\n");
      }
      break;

      case WIFI_APP_FLASH_STATE: {
        wifi_app_clear_event(WIFI_APP_FLASH_STATE);

#if 0
        if (retry) {
          status = sl_wifi_connect(SL_WIFI_CLIENT_2_4GHZ_INTERFACE, &access_point, TIMEOUT_MS);
          if (status != RSI_SUCCESS) {
            LOG_PRINT("WLAN Connect Failed, Error Code : 0x%lX\r\n", status);
            break;
          } else {
            wifi_app_set_event(WIFI_APP_CONNECTED_STATE);
          }
        }

        osSemaphoreRelease(wlan_thread_sem);
#else
        if (!retry) {
            osSemaphoreRelease(wlan_thread_sem);
            break;
        }

        /*------------------------------------------------------------------
         * 자격증명 재설정 — access_point 가 비어 있을 수 있으므로 항상 재구성
         *-----------------------------------------------------------------*/
        sl_wifi_credential_id_t id = SL_NET_DEFAULT_WIFI_CLIENT_CREDENTIAL_ID;
        memset(&access_point, 0, sizeof(access_point));

        if (sec_type != SL_WIFI_OPEN) {
            sl_net_set_credential(id, SL_NET_WIFI_PSK, pwd, strlen((char *)pwd));
        }

        access_point.ssid.length = strlen((char *)coex_ssid);
        memcpy(access_point.ssid.value, coex_ssid, access_point.ssid.length);

        sl_wifi_security_t try_sec = (sl_wifi_security_t)sec_type;
        if (try_sec == SL_WIFI_WPA3) {
            try_sec = SL_WIFI_WPA3_TRANSITION;
        }
        access_point.security      = try_sec;
        access_point.encryption    = SL_WIFI_DEFAULT_ENCRYPTION;
        access_point.credential_id = id;

        //  재연결 시도
        LOG_PRINT("Reconnecting to %s (attempt %u)...\r\n", coex_ssid, reconnect_count + 1);

        status = sl_wifi_connect(SL_WIFI_CLIENT_2_4GHZ_INTERFACE,
                                 &access_point, TIMEOUT_MS);

        if (status != RSI_SUCCESS) {

            connected    = 0;
            disconnected = 1;

#if 1
            wifi_reconnect_pending = API_TRUE;
            wifi_reconnect_tmout = RECONNECT_PENDING_TIME;

#else
            reconnect_count++;
            LOG_PRINT("Reconnect FAIL [%u/%u]: 0x%lX\r\n",
                      reconnect_count, MAX_RECONNECT_RETRY, (uint32_t)status);

            if (reconnect_count < MAX_RECONNECT_RETRY) {
                wifi_reconnect_pending = 1;
                wifi_reconnect_tmout = RECONNECT_PENDING_TIME;

                LOG_PRINT("Waiting for next 10s retry tick...\r\n");
            } else {
                LOG_PRINT("Reconnect limit reached. Rescanning.\r\n");
                reconnect_count = 0;
                wifi_reconnect_pending = 0;
                wifi_app_set_event(WIFI_APP_SCAN_STATE);
            }
#endif
        }
        else {        // 재연결 성공 → IP 설정으로
            reconnect_count = 0;                    /* 카운터 리셋 */
            wifi_reconnect_pending = 0;
            LOG_PRINT("Reconnect success\r\n");
            wifi_app_set_event(WIFI_APP_CONNECTED_STATE);
        }

        osSemaphoreRelease(wlan_thread_sem);
        LOG_PRINT("WIFI App Flash State\r\n");
#endif

      } break;

      case WIFI_APP_CONNECTED_STATE: {
        wifi_app_clear_event(WIFI_APP_CONNECTED_STATE);

        ip_address.type      = SL_IPV4;
        ip_address.mode      = SL_IP_MANAGEMENT_DHCP;
        ip_address.host_name = DHCP_HOST_NAME;

        // Configure IP
        status = sl_si91x_configure_ip_address(&ip_address, SL_SI91X_WIFI_CLIENT_VAP_ID);
        if (status != RSI_SUCCESS) {
          a++;
          if (a == 3) {
            a       = 0;
            timeout = 1;
            status  = sl_wifi_disconnect(SL_WIFI_CLIENT_INTERFACE);
            if (status == RSI_SUCCESS) {
              connected     = 0;
              disassosiated = 1;
              wifi_app_send_to_ble(WIFI_APP_TIMEOUT_NOTIFY, (uint8_t *)&timeout, 1);
              wifi_app_set_event(WIFI_APP_ERROR_STATE);
            }
          }
          LOG_PRINT("IP Config Failed, Error Code : 0x%lX\r\n", status);
          break;
        } else {
          a             = 0;
          connected     = 1;
          conn_status   = 1;
          disconnected  = 0;
          disassosiated = 0;

#if defined(SL_SI91X_PRINT_DBG_LOG)
          sl_ip_address_t ip = { 0 };
          ip.type            = ip_address.type;
          ip.ip.v4.value     = ip_address.ip.v4.ip_address.value;
          print_sl_ip_address(&ip);
#endif

          // update wlan application state
          wifi_app_set_event(WIFI_APP_IPCONFIG_DONE_STATE);
          wifi_app_send_to_ble(WIFI_APP_CONNECTION_STATUS, (uint8_t *)&connected, 1);
        }

        osSemaphoreRelease(wlan_thread_sem);
        LOG_PRINT("WIFI App Connected State\r\n");
      } break;

      case WIFI_APP_IPCONFIG_DONE_STATE: {
        wifi_app_clear_event(WIFI_APP_IPCONFIG_DONE_STATE);

        change_advertise_device_name();

//------------------------------------------------------------------------------
//  USER WebSocket
//------------------------------------------------------------------------------
        ws_app_set_event(WS_APP_INIT_STATE);                // WebSocket Initial 을 한다
        WLAN_WIFI_CONNECT_STATE() = 1;                      // Wi-Fi Connect State

        if(user_flash_info.wifi_mode == Wi_Fi_LOAD_AP_FAIL) {
          user_flash_info.wifi_mode = Wi_Fi_LOAD_AP_SUCCESS;  // Wi-Fi ssid & password save
          user_flash_info.wifi_ap_mode = Wi_Fi_AP_CONNECT;

          memset((char *)user_flash_info.wifi_ssid, 0, sizeof(user_flash_info.wifi_ssid));
          memset((char *)user_flash_info.wifi_password, 0, sizeof(user_flash_info.wifi_password));

          memcpy(user_flash_info.wifi_ssid, coex_ssid, strlen((char *)coex_ssid));
          memcpy(user_flash_info.wifi_password, pwd, strlen((char *)pwd));
          user_flash_info.wifi_sec_type = sec_type;

          printf("WIFI_APP_IPCONFIG_DONE_STATE : [%s][%s][%d]\r\n",
                 user_flash_info.wifi_ssid, user_flash_info.wifi_password, user_flash_info.wifi_sec_type);

          user_app_set_event(USER_APP_NVM_CALLBACK);        // 메모리에 저장을 한다.
        }

//------------------------------------------------------------------------------
        osSemaphoreRelease(wlan_thread_sem);
        LOG_PRINT("WIFI App IPCONFIG Done State\r\n");
      } break;

      case WIFI_APP_ERROR_STATE: {

      } break;

      case WIFI_APP_DISCONNECTED_STATE: {
        wifi_app_clear_event(WIFI_APP_DISCONNECTED_STATE);
        LOG_PRINT("Wi-Fi disconnected. Start reconnect sequence.\r\n");

//  BLE 로 끊김 알림
        wifi_app_send_to_ble(WIFI_APP_DISCONNECTION_STATUS, (uint8_t *)&disconnected, 1);

//  이전 연결 세션 정리 — 재연결 전 명시적 disconnect
        sl_wifi_disconnect(SL_WIFI_CLIENT_INTERFACE);

//  WebSocket 정리 (Wi-Fi 죽었으니 소켓도 종료)
        ws_app_set_event(WS_APP_UNCONNECT_STATE);        // Wi-Fi Disconnect. websocket close
        WLAN_WIFI_CONNECT_STATE() = 0;

//  재연결 시도로 진입
        retry = 1;
        wifi_app_set_event(WIFI_APP_FLASH_STATE);

        osSemaphoreRelease(wlan_thread_sem);
        LOG_PRINT("WIFI App Disconnected State\r\n");

      } break;

      case WIFI_APP_DISCONN_NOTIFY_STATE: {
        wifi_app_clear_event(WIFI_APP_DISCONN_NOTIFY_STATE);

        status = sl_wifi_disconnect(SL_WIFI_CLIENT_INTERFACE);
        if (status == RSI_SUCCESS) {
#if RSI_WISE_MCU_ENABLE
          rsi_flash_erase((uint32_t)FLASH_ADDR_TO_STORE_AP_DETAILS);
#endif
          LOG_PRINT("\r\nWLAN Disconnected\r\n");
          disassosiated = 1;
          connected     = 0;
          wifi_app_send_to_ble(WIFI_APP_DISCONNECTION_NOTIFY, (uint8_t *)&disassosiated, 1);
          wifi_app_set_event(WIFI_APP_UNCONNECTED_STATE);
        } else {
          LOG_PRINT("\r\nWIFI Disconnect Failed, Error Code : 0x%lX\r\n", status);
        }

        osSemaphoreRelease(wlan_thread_sem);
        LOG_PRINT("WIFI App Disconnect Notify State\r\n");
      } break;
      case WIFI_APP_SOCKET_RECEIVE_STATE:
        break;
      case WIFI_APP_MQTT_INIT_DONE_STATE:
        break;
      case WIFI_APP_MQTT_SUBSCRIBE_DONE_STATE:
        break;
      case BLE_APP_GATT_WRITE_EVENT:
        break;
      case WIFI_APP_DATA_RECEIVE_STATE:
        break;
      case WIFI_APP_SD_WRITE_STATE:
        break;
      case WIFI_APP_DEMO_COMPLETE_STATE:
        break;

      case WIFI_APP_FOTA_STATE: {
        wifi_app_clear_event(WIFI_APP_FOTA_STATE);
        LOG_PRINT("\r\n=== OTA Start ===\r\n");

        /* 1) WebSocket 종료만 깔끔히 */
        ws_app_set_event(WS_APP_UNCONNECT_STATE);
        LOG_PRINT("[OTA] Waiting 3s for WS cleanup...\r\n");
        osDelay(3000);

        /* ⭐️ 2) OTA 전용 인증서 강제 재로드 (다른 통신과 꼬이는 것 방지) */
        LOG_PRINT("[OTA] Loading S3 Certificate...\r\n");
        ota_app_load_certificate();
        osDelay(500); // 넉넉히 대기

        /* 3) OTA 시작 */
        sl_status_t s = app_start_http_ota();
        if (s != SL_STATUS_OK) {
            LOG_PRINT("[OTA] start failed: 0x%lX\r\n", s);
            osSemaphoreRelease(wlan_thread_sem);
            break;
        }

        /* M4 펌웨어 업데이트 → SoC 리셋 */
        LOG_PRINT("[OTA] success, rebooting...\r\n");
        osDelay(500);
        sl_si91x_soc_nvic_reset();
        /* 여기서 돌아오지 않음 */
      } break;

      default:
        break;
    }
  }

  if (scan_result != NULL) {
    free(scan_result);
  }
}


/*******************************************************************************
 *  Wi-Fi Disconnect 확인 횟수...1초 간격으로 확인
 *******************************************************************************/
void wifi_link_monitor(void)
{
    int32_t rssi = 0;
    sl_status_t status;

    if (!connected) {
        return;                         /* 이미 끊김 처리 중 */
    }

    status = sl_wifi_get_signal_strength(SL_WIFI_CLIENT_INTERFACE, &rssi);

    if (status != SL_STATUS_OK) {
        fail_retry_count++;
        LOG_PRINT("link check fail [%u/%u]: 0x%lX\r\n",
                  fail_retry_count, LINK_CHECK_FAIL_MAX, (uint32_t)status);

        if (fail_retry_count >= LINK_CHECK_FAIL_MAX) {
            fail_retry_count     = 0;
            connected    = 0;
            disconnected = 1;
            LOG_PRINT("link lost -> DISCONNECTED_STATE\r\n");
            wifi_app_set_event(WIFI_APP_DISCONNECTED_STATE);
        }
    } else {
        fail_retry_count = 0;                   /* 정상 */
    }
}

/*******************************************************************************
 *  Wi-Fi reconnect try ...5초 간격으로 확인
 *******************************************************************************/
void wifi_reconnect_event_handler(void)
{
  if(wifi_reconnect_tmout>0)  wifi_reconnect_tmout--;

  if((wifi_reconnect_pending) && (!wifi_reconnect_tmout)) {
    wifi_reconnect_pending = 0;
    wifi_app_set_event(WIFI_APP_FLASH_STATE);   /* 재연결 재시도 */
  }
}
