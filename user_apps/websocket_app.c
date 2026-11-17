/*******************************************************************************
 * @file  websocket_app.c
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

#include "sl_websocket_client_types.h"
#include "sl_websocket_client.h"

#include "sl_sleeptimer.h"

//BLE Specific inclusions
#include <rsi_ble_apis.h>
#include "ble_config.h"
#include "rsi_ble_common_config.h"

#include <rsi_common_apis.h>
#endif

#include "../app.h"
#include "../wifi_config.h"

#include "cJSON/cJSON.h"
#include "cJSON/cJSON_Utils.h"


#ifdef SLI_SI91X_ENABLE_IPV6
#define SERVER_IP_ADDR "fd7f:c929:e748::3df"
#else
//#define SERVER_IP_ADDR "121.137.124.156"
#define SERVER_IP_ADDR "118.131.46.69"
#endif

#if 1
#define HOST_NAME     "118.131.46.69"              // owj
#define RESOURCE_NAME "/ws"
#else
#define HOST_NAME     "118.131.46.69:8081"//8509"  //8081 //sib
#define RESOURCE_NAME "/user/websocket"
#endif

static sl_websocket_client_t ws_handle = { 0 };

#define OBJECT_ITEM_NUM     7
const char *OBJECT_1ST_ITEM_TABLE[OBJECT_ITEM_NUM] = {
    "Current Time",
    "Accepted Status",
    "Water Flow Config",
    "Request Config",
    "Sparkling Recipe Config",
    "FOTA download Start",
    "Calendar Update"
};

int ws_data_send_timeout = 0;


/*********************************************************************************************************
 *                                               DATA TYPES
 *********************************************************************************************************/
static volatile uint32_t ws_app_event_map = 0;
osSemaphoreId_t ws_app_thread_sem;

char *json_payload = NULL;

sl_websocket_error_t create_websocket_data(void);
sl_websocket_error_t send_websocket_data(const char *data);
sl_websocket_error_t send_websocket_pong(const uint8_t *buffer, uint32_t length);
sl_websocket_error_t websocket_close(void);


/*******************************************************************************
 *  sets the specific event.
 *******************************************************************************/
void ws_app_set_event(uint32_t event_num)
{
  ws_app_event_map |= BIT(event_num);
  osSemaphoreRelease(ws_app_thread_sem);
  return;
}

/*******************************************************************************
 *  clear the specific event.
 *******************************************************************************/
void ws_app_clear_event(uint32_t event_num)
{
  ws_app_event_map &= ~BIT(event_num);
  return;
}
/*******************************************************************************
 *  get the specific event.
 *******************************************************************************/
int32_t ws_app_get_event(void)
{
  uint32_t ix;

  for (ix = 0; ix < 32; ix++) {
    if (ws_app_event_map & (1 << ix)) {
      return ix;
    }
  }

  return NO_EVENT_ID;
}


/*************************************************************************
 *  websocket app thread...
 ************************************************************************/
void websocket_app_thread(void *argument)
{
  UNUSED_PARAMETER(argument);

  int32_t event_id = 0;
  sl_websocket_error_t ws_error;

  printf("websocket_app_thread initialization is successful\r\n");

  ws_app_thread_sem = osSemaphoreNew(1, 0, NULL);
  if (ws_app_thread_sem == NULL) {
    LOG_PRINT("Failed to create ws_app_thread_sem\n");
    return;
  }

  make_api_key_generation();                            // api key

  while (1)
  {
      event_id = ws_app_get_event();
      if (event_id == NO_EVENT_ID) {
        osSemaphoreAcquire(ws_app_thread_sem, osWaitForever);
        continue;         // if events are not received loop will be continued.
      }

      switch(event_id) {
        case WS_APP_IDLE_STATE:
          break;

        case WS_APP_INIT_STATE:
          ws_app_clear_event(WS_APP_INIT_STATE);

          ws_error = create_websocket_data();
          if (ws_error != SL_WEBSOCKET_SUCCESS) {
              printf("create_and_send_websocket_data failed with error:%d\r\n", ws_error);

              ws_app_set_event(WS_APP_INIT_FAIL);
              user_ws_app.connect = CONNECT_FAIL;

              user_ws_app.need_reconnect = WS_RECONNECT_START;
              user_ws_app.retry_tmout = 5;
          }
          else {
              printf("create_and_send_websocket_data success\r\n");

              user_ws_app.connect = CONNECT_SUCCESS;
              user_ws_app.need_reconnect = WS_RECONNECT_STOP;
              user_ws_app.retry_count = 0;
              user_ws_app.send_data_mode = WS_SUCCESS;                // 데이터를 전송 할 수 있다...
              ws_send_elapsed_time = 0;

              WLAN_WIFI_CONNECT_STATE() = 1;
          }
          break;

        case WS_APP_INIT_FAIL:                                // 실패 이후 5초 간격으로 재 연결을 한다.
          if((user_ws_app.need_reconnect==WS_RECONNECT_START) && (user_ws_app.retry_tmout==0)) {
            user_ws_app.retry_count++;
            if(user_ws_app.retry_count>=10) {                   // 10회 이상 연결이 되지 않으면...
                user_ws_app.retry_tmout = 60;                   // 60초 뒤 다시 시작을 한다...
                user_ws_app.retry_count = 0;
            }
            else {
                ws_app_clear_event(WS_APP_INIT_FAIL);
                ws_app_set_event(WS_APP_INIT_STATE);            // 초기화를 다시 해 본다..
            }
            printf("websocket_init_reconnect : [%d]\r\n", user_ws_app.retry_count);
          }
          break;

        case WS_APP_DATA_SEND_MODE:                            // 연결이 된 상태에서 일정한 간격으로 데이터 전송한다.

          ws_app_clear_event(WS_APP_DATA_SEND_MODE);            // 1번만
          printf("WS_APP_DATA_SEND_MODE\r\n");

          if(user_ws_app.connect == CONNECT_SUCCESS) {
              ws_error = youngwon_ws_message_handler();
              if(ws_error != SL_WEBSOCKET_SUCCESS) {
                  LOG_PRINT("WS send FAIL: %ld → disconnected\r\n", ws_error);
                  ws_app_set_event(WS_APP_UNCONNECT_STATE);
              }
          }

          osSemaphoreRelease(ws_app_thread_sem);
          break;

        case WS_APP_UNCONNECT_STATE:
          ws_app_clear_event(WS_APP_UNCONNECT_STATE);            // web socket close
          printf("WS_APP_UNCONNECT_STATE\r\n");

          websocket_close();
          user_ws_app.connect = CONNECT_FAIL;

          if(WLAN_WIFI_CONNECT_STATE()) {                           // Wi-Fi Connect 상태 라면...
              ws_app_set_event(WS_APP_INIT_STATE);                  // 재연결 상태를 확인 한다
              user_ws_app.retry_count = 0;
          }
          break;

        default:
          break;
      }

//-------------------------------------------------------------------
//  5. waiting for OS time.
//-------------------------------------------------------------------
      osDelay(pdMS_TO_TICKS(25));
  }
}




/********************************************************************************************
 * web socket data callback event.
 ********************************************************************************************/
void data_callback(uint32_t sock_no,
                   uint8_t *buffer,
                   uint32_t length,
                   const sl_si91x_socket_metadata_t *firmware_socket_response)
{
  UNUSED_PARAMETER(sock_no);
  sl_websocket_opcode_t opcode = sl_si91x_get_opcode_from_socket_id(firmware_socket_response->socket_id);

  switch(opcode) {
    case SL_WEBSOCKET_OPCODE_TEXT:
      parser_cJSON_payload((uint8_t *)buffer, length);
      break;

    case SL_WEBSOCKET_OPCODE_BINARY:
      break;

    case SL_WEBSOCKET_OPCODE_PING:                  // PING 요청에 대한 답변...
      send_websocket_pong((uint8_t *)buffer, length);
      break;

    default:
      break;
  }
}

void remote_terminate_callback(int socket_id, uint16_t port_number, uint32_t bytes_sent)
{
  printf("Remote client terminated on socket %d, port %d , bytes_sent %ld\r\n", socket_id, port_number, bytes_sent);
}

sl_websocket_error_t create_websocket_data(void)
{
  sl_websocket_config_t ws_config = {
    .host                = HOST_NAME,
    .resource            = RESOURCE_NAME,
    .server_port         = 8509,  //8081,  //8002,
    .client_port         = 5001,
    .ip_address          = SERVER_IP_ADDR,
    .data_cb             = data_callback,
    .remote_terminate_cb = remote_terminate_callback,
    .enable_ssl          = false,
  };

  sl_websocket_error_t ws_error = sl_websocket_init(&ws_handle, &ws_config);
  if (ws_error != SL_WEBSOCKET_SUCCESS) {
    printf("Error in sl_websocket_init:%d \r\n", ws_error);
    return ws_error;
  }
  printf("WebSocket Init done\r\n");

  ws_error = sl_websocket_connect(&ws_handle);
  if (ws_error != SL_WEBSOCKET_SUCCESS) {
    printf("Error while websocket connect:%d\r\n", ws_error);

    sl_websocket_close(&ws_handle);      /* 실패해도 무방 */
    sl_websocket_deinit(&ws_handle);     /* init 리소스 해제 (핵심) */

    return ws_error;
  }
  printf("WebSocket connect Success\r\n");

  return SL_WEBSOCKET_SUCCESS;
}


/********************************************************************************************
 * web socket data send message..JSON 형식 정보 포맷.
 ********************************************************************************************/
sl_websocket_error_t send_websocket_data(const char *data)
{
  sl_websocket_error_t ws_error;

  // Create and populate the send_request structure
  sl_websocket_send_request_t send_request;

  send_request.opcode = SL_WEBSOCKET_OPCODE_TEXT | SL_WEBSOCKET_FIN_BIT;
  send_request.buffer = (const uint8_t *)data;
  send_request.length = strlen(data);

  ws_error = sl_websocket_send_frame(&ws_handle, &send_request);
  if (ws_error != SL_WEBSOCKET_SUCCESS) {
    printf("Error while websocket send frame:%d\r\n", ws_error);
    return ws_error;
  }
  printf("WebSocket send success\r\n");

  return SL_WEBSOCKET_SUCCESS;
}

/********************************************************************************************
 * web socket data send message..JSON 형식 정보 포맷.
 ********************************************************************************************/
sl_websocket_error_t websocket_close(void)
{
  sl_websocket_error_t ws_error;

  ws_error = sl_websocket_close(&ws_handle);
  if (ws_error != SL_WEBSOCKET_SUCCESS) {
    printf("\r\nError while websocket close:%d", ws_error);
    return ws_error;
  }
  printf("\r\nWebSocket close success");

  ws_error = sl_websocket_deinit(&ws_handle);
  if (ws_error != SL_WEBSOCKET_SUCCESS) {
    printf("\r\nError while websocket deinit:%d", ws_error);
    return ws_error;
  }
  printf("\r\nWebSocket deinit success");

  return SL_WEBSOCKET_SUCCESS;
}


/********************************************************************************************
 * WebSocket data pong send message..
 ********************************************************************************************/
sl_websocket_error_t send_websocket_pong(const uint8_t *buffer, uint32_t length)
{
  // Send the pong response
  sl_websocket_send_request_t send_request;
  send_request.opcode           = SL_WEBSOCKET_OPCODE_PONG | SL_WEBSOCKET_FIN_BIT;
  send_request.buffer           = buffer;
  send_request.length           = length;

  sl_websocket_error_t ws_error = sl_websocket_send_frame(&ws_handle, &send_request);
  if (ws_error != SL_WEBSOCKET_SUCCESS) {
    printf("Error while sending pong frame:%d\r\n", ws_error);
    return SL_WEBSOCKET_ERR_SOCKET_CREATION;
  }
  printf("Pong Send success\r\n");

//After sending the pong response we are closing the socket in this application
  return SL_WEBSOCKET_SUCCESS;
}



/*************************************************************************
 * json payload memory release
 ************************************************************************/
void json_payload_point_free(void)
{
  if (json_payload != NULL) {
    free(json_payload);
    json_payload = NULL;
  }
}


/*******************************************************************************
 *  수신 메시지에 대한 ID 구분 하기
 *******************************************************************************/
int8_t youngone_cJSON_Object_Item(const char *object_item)
{
  int8_t item;

  for(item=0;item<OBJECT_ITEM_NUM;item++) {
    if(strncmp(object_item,OBJECT_1ST_ITEM_TABLE[item], strlen(OBJECT_1ST_ITEM_TABLE[item]))==0) {
        printf("cJSON_object :  %d\r\n", item+1);
        return (item+1);
    }
  }

  return 0;
}


/*******************************************************************************
 *  JSON MESSAGE ANALYSIS
 *******************************************************************************/
void youngone_cJSON_idTag(const cJSON *root)
{
  const cJSON *idTagName = cJSON_GetObjectItemCaseSensitive(root, "idTag Name");
  const cJSON *idTagStatus=cJSON_GetObjectItemCaseSensitive(root, "idTag Status");

  // 타입 확인 후 값 출력 (안전한 코드를 위해 타입 확인은 필수입니다)
  if (cJSON_IsString(idTagName) && idTagName->valuestring != NULL) {
      printf("idTag Name: %s\r\n", idTagName->valuestring);
  }

  if (cJSON_IsString(idTagStatus) && idTagStatus->valuestring != NULL) {
      printf("idTag Status: %s\r\n", idTagStatus->valuestring);
  }

  SET_WS_idTAG_STATUS(1);
  user_ws_app.idTag_status = SUCCESS;

}

/*******************************************************************************
 * idTag 수신...Current Data & Time
 *******************************************************************************/
void youngone_cJSON_CurrentTime(const cJSON *root)
{
  cJSON *currentTime = cJSON_GetObjectItemCaseSensitive(root, "Current Time");
  cJSON *idTagInfoArray = cJSON_GetObjectItemCaseSensitive(root, "idTagInfo");

  // 1. "Current Time" 읽기
  if (cJSON_IsString(currentTime) && (currentTime->valuestring != NULL)) {
    const char *time_str = currentTime->valuestring;

    int year, month, day, hour, min, sec;

      // 2. sscanf를 이용하여 포맷에 맞춰 숫자 추출
      // 포맷: YYYY-MM-DD D HH:mm:ss T
    int items = sscanf(time_str, "%d-%d-%dD%d:%d:%dT",
                     &year, &month, &day, &hour, &min, &sec);

    if (items == 6) {       // 성공적으로 6개의 값을 읽었을 경우
      DEBUGOUT("Time Format: hour:%d, min:%d, sec:%d\r\n", hour, min, sec);
      DEBUGOUT("Date Format: DD/MM/YY: %.2d/%.2d/20%.02d \r\n", day, month, year);

      if(year>=2000)
        year-=2000;
      user_calendar.Year = year;            // sample date & time
      user_calendar.Month = month;
      user_calendar.Day = day;
      user_calendar.Hour = hour;
      user_calendar.Minute = min;
      user_calendar.Second = sec;

      calendar_write_datetime(&user_calendar);
      calendar_read_datetime(&user_calendar);                                 // 현재 시간 날짜를 읽어 온다..

      user_info.calendar_update = SUCCESS;

      DEBUGOUT("Time Format: hour:%d, min:%d, sec:%d\r\n", user_calendar.Hour, user_calendar.Minute, user_calendar.Second);
      DEBUGOUT("Date Format: DD/MM/YY: %.2d/%.2d/20%.02d \r\n", user_calendar.Day, user_calendar.Month, user_calendar.Year);
    }
    else {
      printf("시간 포맷이 올바르지 않습니다: %s\r\n", time_str);
    }
  }

  // 2. "idTagInfo" 배열 접근
  if (cJSON_IsArray(idTagInfoArray)) {              // 배열의 첫 번째 요소(객체) 가져오기
    cJSON *firstItem = cJSON_GetArrayItem(idTagInfoArray, 0);

  // 3. "Status" 값 추출
    if (cJSON_IsObject(firstItem)) {
      cJSON *status = cJSON_GetObjectItemCaseSensitive(firstItem, "Status");
      if (cJSON_IsString(status) && (status->valuestring != NULL)) {
        printf("status value: %s\r\n", status->valuestring);
        if (strcmp(status->valuestring, "Accepted") == 0) {
            printf("Accepted Success.\r\n");

            user_ws_app.idTag_status = SUCCESS;

            SET_WS_idTAG_STATUS(0);
            SET_WS_BASIC_INFO(1);                             // FLOW CONFIG
        }
        else {
            printf("Accepted Fail.\r\n");
        }
      }
    }
  }
}




/*******************************************************************************
 * idTag 수신...Current Data & Time
 *******************************************************************************/
int8_t youngone_cJSON_AcceptedStatus(const cJSON *root)
{
  int8_t result = 0;
  cJSON *status_item = NULL;
  cJSON *code_item   = NULL;

  status_item = cJSON_GetObjectItemCaseSensitive(root, "Accepted Status");
  code_item = cJSON_GetObjectItemCaseSensitive(root, "Accepted Code");

  if(strncmp(status_item->valuestring, "Success", strlen("Success")) == 0) {
      if(code_item->valueint == ERROR_NO_ERROR) {
          result = 1;
      }
  }

  printf("%s, %d --> %d\r\n", status_item->valuestring, code_item->valueint, result);

  return result;

}

#define WATER_CONFIG_MAX        16      /* 최대 채널 수 (필요에 맞게 조정) */
#define WATER_NAME_INVALID      (-1)
int8_t youngone_cJSON_WaterFlowConfig(const cJSON *root)
{
  cJSON *array      = NULL;
  cJSON *item       = NULL;
  cJSON *name_node  = NULL;
  cJSON *flow_node  = NULL;
  cJSON *temp_node  = NULL;

  int array_size = 0;
  int idx        = 0;

  array = cJSON_GetObjectItemCaseSensitive(root, "Water Flow Config");
  if (!cJSON_IsArray(array))  {
      printf("[JSON] 'Water Flow Config' is not an array\r\n");
      return -1;
  }

  array_size = cJSON_GetArraySize(array);
  if (array_size <= 0)  {
      printf("[JSON] Empty array\r\n");
      return -1;
  }

  if (array_size > WATER_CONFIG_MAX)  {
      printf("[JSON] Array size %d exceeds max %d, truncating\r\n",
             array_size, WATER_CONFIG_MAX);
      array_size = WATER_CONFIG_MAX;
  }

/******************************************************************************
 * 각 항목 순회하며 파싱
 ******************************************************************************/
  cJSON_ArrayForEach(item, array) {
    if (idx >= WATER_CONFIG_MAX)  {
        break;
    }

    if (!cJSON_IsObject(item))  {
        printf("[JSON] entry[%d] is not an object, skip\r\n", idx);
        continue;
    }

// Water Name : 1: 냉수      2:온수      3: 탄산수
    name_node = cJSON_GetObjectItemCaseSensitive(item, "Water Name");
    if (!cJSON_IsNumber(name_node)) {
        printf("[JSON] entry[%d] 'Water Name' invalid, skip\r\n", idx);
        continue;
    }

// Water Flow : 100, 120, 250, 550, 1000, 0(연속 출수)
    flow_node = cJSON_GetObjectItemCaseSensitive(item, "Water Flow");
    if (!cJSON_IsNumber(flow_node))      {
          printf("[JSON] entry[%d] 'Water Flow' invalid, skip\r\n", idx);
        continue;
    }

    if(name_node->valueint>=1 && name_node->valueint<=3) {
        user_flash_info.water_flow[name_node->valueint-1]= flow_node->valueint;
    }

// Water Temperature : 40, 75, 85, 90, 0(저온 냉수), 1(에코 모드)
    temp_node = cJSON_GetObjectItemCaseSensitive(item, "Temperature");
    if (!cJSON_IsNumber(temp_node)) {
        printf("[JSON] entry[%d] 'Temperature' invalid, skip\r\n", idx);
        continue;
    }

    if(name_node->valueint>=1 && name_node->valueint<=3) {
        user_flash_info.water_temp[name_node->valueint-1]= temp_node->valueint;
    }

    printf("[JSON] entry[%d] Name:%d, Flow:%d, Temp:%d\r\n",
           idx,
           name_node->valueint,
           flow_node->valueint,
           temp_node->valueint);

      idx++;
  }


  return 1;
}



/*****************************************************************************
 *  필요에 따라 정보를 요청 할 경우 정보를 전송 한다.
 *****************************************************************************/
int8_t youngone_cJSON_RequestConfig(const cJSON *root)
{
  cJSON *config_array = cJSON_GetObjectItemCaseSensitive(root, "Request Config");

  if (!cJSON_IsArray(config_array)) {
      return -1;
  }

  cJSON *request_item = cJSON_GetArrayItem(config_array, 0);
  if (request_item == NULL) {
      return -1;
  }

  cJSON *item_name =cJSON_GetObjectItemCaseSensitive(request_item, "Request Name");
  cJSON *item_time = cJSON_GetObjectItemCaseSensitive(request_item, "Request Time");

  if (cJSON_IsNumber(item_name)) {

    printf("RequestConfig : [%d] \r\n",item_name->valueint);

    switch(item_name->valueint) {
      case 1:         // Basic information
        SET_WS_BASIC_INFO(1);
        break;
      case 2:         // Water Output Capacity
        SET_WS_OUTPUT_CAPACITY(1);
        break;
      case 3:         // Status Information
        SET_WS_STATUS_INFO(1);
        break;
      case 4:         // Sensor Information
        SET_WS_SENSOR_INFO(1);
        break;
      case 5:         // Software Verification
        SET_WS_VERIFICATION(1);
        break;
      default:
        break;

    }
  }

  if (cJSON_IsNumber(item_time)) {
      ws_send_waiting_time = item_time->valueint*10;
  }
  return 1;
}

/*******************************************************************************
 *  SPARKLING RECIPE 설정 --> 내부 플래쉬에 저장 하지 않는다.
 ******************************************************************************/
int8_t youngone_cJSON_SparklingRecipeConfig(const cJSON *root)
{
  cJSON *config_array = cJSON_GetObjectItemCaseSensitive(root, "Sparkling Recipe Config");

  if (!cJSON_IsArray(config_array)) {
      return -1;
  }

  cJSON *recipe_item = cJSON_GetArrayItem(config_array, 0);
  if (recipe_item == NULL) {
      return -1;
  }

  cJSON *cold_water =cJSON_GetObjectItemCaseSensitive(recipe_item, "Cold Water");
  cJSON *hot_water = cJSON_GetObjectItemCaseSensitive(recipe_item, "Hot Water");
  cJSON *sparkling = cJSON_GetObjectItemCaseSensitive(recipe_item, "Sparkling");

  if (cJSON_IsNumber(cold_water)) {
    printf("냉수(Cold Water): %d\r\n", cold_water->valueint);
    user_info.water_recipe.cold_flow = cold_water->valueint;
  }

  if (cJSON_IsNumber(hot_water)) {
      printf("온수(Hot Water): %d\r\n", hot_water->valueint);
      user_info.water_recipe.warm_flow = hot_water->valueint;
  }

  if (cJSON_IsNumber(sparkling)) {
      printf("탄산(Sparking): %d\r\n", sparkling->valueint);
      user_info.water_recipe.sparkling = sparkling->valueint;
  }

  return 1;
}

/*******************************************************************************
 *  펌웨어 업데이트 상태를 기억 한다...ifTag를 올릴 때 Updated 라고 변경 해서 전달 한다...
 ******************************************************************************/
int8_t youngone_cJSON_FOTA_Start(const cJSON *root)
{
  cJSON *fota_array = cJSON_GetObjectItemCaseSensitive(root, "FOTA download Start");

  if (!cJSON_IsArray(fota_array)) {
      return -1;
  }

  cJSON *fota_item = cJSON_GetArrayItem(fota_array, 0);
  if (fota_item == NULL) {
      return -1;
  }

  cJSON *fw_ver = cJSON_GetObjectItem(fota_item, "Target FW Ver");
  cJSON *fsize  = cJSON_GetObjectItem(fota_item, "File Size");
  cJSON *offset = cJSON_GetObjectItem(fota_item, "Start Offset");

  printf("version : %s\r\n", fw_ver->valuestring);
  printf("fsize : %d byte\r\n", fsize->valueint);
  printf("offset : %d \r\n", offset->valueint);


  if(strncmp(fw_ver->valuestring, AWS_FW_VERSION, strlen(AWS_FW_VERSION))!=0) {
    if(offset->valueint == 1) {
        wifi_app_set_event(WIFI_APP_FOTA_STATE);
        return 1;
    }
  }

  return 0;
}


/*******************************************************************************
 * idTag 수신...Current Data & Time
 *******************************************************************************/
int8_t youngone_cJSON_calendar_update(const cJSON *root)
{
  cJSON *calendar_array = cJSON_GetObjectItemCaseSensitive(root, "Calendar Update");

  if (!cJSON_IsArray(calendar_array)) {
      return -1;
  }

  cJSON *calendar_item = cJSON_GetArrayItem(calendar_array, 0);
  if (calendar_item == NULL) {
      return -1;
  }

  cJSON *currentTime = cJSON_GetObjectItem(calendar_item, "Current Time");

  if (currentTime->valuestring != NULL) {
      const char *time_str = currentTime->valuestring;

      int year, month, day, hour, min, sec;

        // 2. sscanf를 이용하여 포맷에 맞춰 숫자 추출
        // 포맷: YYYY-MM-DD D HH:mm:ss T
      int items = sscanf(time_str, "%d-%d-%dD%d:%d:%dT",
                       &year, &month, &day, &hour, &min, &sec);

      if (items == 6) {       // 성공적으로 6개의 값을 읽었을 경우
          printf("Time Format: hour:%d, min:%d, sec:%d\r\n", hour, min, sec);
          printf("Date Format: DD/MM/YY: %.2d/%.2d/20%.02d \r\n", day, month, year);

        if(year>=2000)
          year-=2000;
        user_calendar.Year = year;            // sample date & time
        user_calendar.Month = month;
        user_calendar.Day = day;
        user_calendar.Hour = hour;
        user_calendar.Minute = min;
        user_calendar.Second = sec;

        calendar_write_datetime(&user_calendar);
        calendar_read_datetime(&user_calendar);                                 // 현재 시간 날짜를 읽어 온다..

        user_info.calendar_update = SUCCESS;

        printf("Time Format: hour:%d, min:%d, sec:%d\r\n", user_calendar.Hour, user_calendar.Minute, user_calendar.Second);
        printf("Date Format: DD/MM/YY: %.2d/%.2d/20%.02d \r\n", user_calendar.Day, user_calendar.Month, user_calendar.Year);

  // 초기화 명령을 제어 보드 쪽으로 전송을 한다.
        youngwon_software_verification_initial(0x07); // control board에 초기화 명령으로 값을 초기화 한다.
        user_calendar.over_day = 1;

        return 1;

      }
      else {
        printf("시간 포맷이 올바르지 않습니다: %s\r\n", time_str);

      }
  }

  return 0;
}
/*******************************************************************************
 *  JSON MESSAGE ANALYSIS
 *******************************************************************************/
void youngone_cJSON_analysis(const cJSON *root)
{
  int8_t result = 0;
  int8_t obj_item = 0;
  cJSON *object_item = root->child;

  if(object_item != NULL) {
    if(object_item->string != NULL) {
      obj_item = youngone_cJSON_Object_Item(object_item->string);

      switch(obj_item) {
        case 1:
          youngone_cJSON_CurrentTime(root);
          break;

        case 2:
          if(youngone_cJSON_AcceptedStatus(root) == 1) {
              switch(user_ws_app.now_step) {
                case WS_JSON_MSG_BASIC_INFO:
                  break;

                case WS_JSON_MSG_STATUS_INFO:
                  break;

                case WS_JSON_MSG_SENSOR_INFO:
                  break;

                case WS_JSON_MSG_VERIFICATION:
                  break;

                case WS_JSON_MSG_OUTPUT_CAPACITY:
                  SET_RCV_WATER_AMOUNT(0);
                  break;

                default:
                  break;
              }
          }
          break;

        case 3:             // Water Flow Config
          if(youngone_cJSON_WaterFlowConfig(root) == 1) {       // 정상 수신이 되면
              SET_WS_BASIC_INFO(1);

              user_app_set_event(USER_APP_NVM_CALLBACK);        // 메모리에 저장을 한다.
              water_flow_out_flag = API_TRUE;
              water_flow_out_step = 0;
          }
          else {
              make_cJSON_AcceptedStatus(ERROR_FORMAT);
          }
          send_JSON_websocket_data();
          break;

        case 4:             // Request Config
          if(youngone_cJSON_RequestConfig(root) != 1) {
//              make_cJSON_AcceptedStatus(ERROR_FORMAT);
              send_JSON_websocket_data();
          }
          break;

        case 5:             // Sparkling Recipe Config
          if(youngone_cJSON_SparklingRecipeConfig(root) == 1) {
              make_cJSON_AcceptedStatus(ERROR_NO_ERROR);
              youngwon_recipe_control_setup();
          }
          else {
              make_cJSON_AcceptedStatus(ERROR_FORMAT);
          }
          send_JSON_websocket_data();
          break;

        case 6:             // FOTA download Start
          result = youngone_cJSON_FOTA_Start(root);
          if(result==1) {           // FOTA UPDATE START
              make_cJSON_AcceptedStatus(ERROR_NO_ERROR);
          }
          else if(result == 0) {    // FOTA UPDATE CANCEL
              make_cJSON_AcceptedStatus(ERROR_COMMAND_CANCEL);
          }
          else {                    // FOTA UPDATE ERROR
              make_cJSON_AcceptedStatus(ERROR_FORMAT);
          }
          send_JSON_websocket_data();
          break;

        case 7:               // 00:00:00 초기화
          if(youngone_cJSON_calendar_update(root)) {
              make_cJSON_AcceptedStatus(ERROR_NO_ERROR);
          }
          else {
              make_cJSON_AcceptedStatus(ERROR_FORMAT);
          }

          send_JSON_websocket_data();
          break;

        default:
          break;
      }
    }
  }

  else {
      printf("JSON is Empty\r\n");
  }
}



/*************************************************************************
 *  web socket parser receive cJSON payload message is JSON
 ************************************************************************/
void parser_cJSON_payload(const uint8_t *buffer, uint16_t length)
{
#if 1
  cJSON      *root      = NULL;
  const char *error_ptr = NULL;

  if ((buffer == NULL) || (length == 0))
  {
      printf("[JSON] empty buffer\r\n");
      return;
  }

  /* 길이를 알면 ParseWithLength 가 안전 */
  root = cJSON_ParseWithLength((const char *)buffer, length);
  if (root == NULL)
  {
      error_ptr = cJSON_GetErrorPtr();
      printf("[JSON] parse error before: %s\r\n",
             (error_ptr != NULL) ? error_ptr : "(unknown)");
      return;
  }

  youngone_cJSON_analysis(root);
  cJSON_Delete(root);     /* 만든 쪽에서 해제 → 일관성 유지 */


#else
  cJSON *root = cJSON_Parse((char *)buffer);

  if (root == NULL) {
      const char *error_ptr = cJSON_GetErrorPtr();
      if (error_ptr != NULL) {
          fprintf(stderr, "JSON 파싱 오류: %s\n", error_ptr);
      }
      return ;
  }

  youngone_cJSON_analysis(root);

  cJSON_Delete(root);
#endif
}

/*************************************************************************
 *  payload data send message
 ************************************************************************/
sl_websocket_error_t send_JSON_websocket_data(void)
{
  sl_websocket_error_t ws_error;

  if (json_payload != NULL) {
    ws_error = send_websocket_data((char *)json_payload);
    if (ws_error != SL_WEBSOCKET_SUCCESS) {
        printf("create_and_send_websocket_data failed with error:%d\r\n", ws_error);
        user_ws_app.send_data_state = WS_SEND_DATA_FAIL;
    }
    else {
        user_ws_app.send_data_state = WS_SEND_DATA_SUCCESS;
    }

    free(json_payload);
    json_payload = NULL;  // 다시 NULL로 만들어 중복 해제 방지
  }

  return ws_error;
}


/*************************************************************************
 *  idTag를 위한 api key값을 생성 한다.
 ************************************************************************/
void make_api_key_generation(void)
{
  memset(authorize_api_key, 0, sizeof(authorize_api_key));
  SIWG917_generate_api_key(authorize_api_key);
  printf("api_key: %s\r\n", authorize_api_key);
}


/*************************************************************************
 *  web socket make send cJSON idTag
 ************************************************************************/
int8_t make_cJSON_AcceptedStatus(uint8_t err_code)
{
  cJSON *root = cJSON_CreateObject();

  if (root == NULL)
    return -1;

  cJSON_AddStringToObject(root, "Accepted Status", "Success");
  cJSON_AddNumberToObject(root, "Accepted Code", err_code);

  json_payload_point_free();
  json_payload = cJSON_Print(root);
  if (json_payload == NULL) {
      free(json_payload);
      json_payload = NULL; // 다시 NULL로 만들어 중복 해제 방지

      cJSON_Delete(root);
      return -1;
  }

  printf("Generated JSON:\n%s\n", json_payload);

  cJSON_Delete(root);
  return 1;
}

/*************************************************************************
 *  web socket make send cJSON idTag
 ************************************************************************/
int8_t make_cJSON_idTag(void)
{
  cJSON *root = cJSON_CreateObject();

  if (root == NULL)
    return -1;

  cJSON_AddStringToObject(root, "idTag Name", authorize_idTag);
  cJSON_AddStringToObject(root, "idTag Status", "Authorize");
  cJSON_AddStringToObject(root, "FW Version", AWS_FW_VERSION);

  json_payload_point_free();
  json_payload = cJSON_Print(root);
  if (json_payload == NULL) {
      free(json_payload);
      json_payload = NULL; // 다시 NULL로 만들어 중복 해제 방지

      cJSON_Delete(root);
      return -1;
  }

  printf("Generated JSON:\n%s\n", json_payload);

  cJSON_Delete(root);
  return 1;
}

/*************************************************************************
 *  web socket make send cJSON Heartbeat
 ************************************************************************/
int8_t make_cJSON_Heartbeat(void)
{
  cJSON *root = cJSON_CreateObject();

  if (root == NULL)
    return -1;

  cJSON_AddStringToObject(root, "idTag Name", authorize_idTag);
  cJSON_AddStringToObject(root, "idTag Status", "Heartbeat");
  cJSON_AddStringToObject(root, "FW Version", AWS_FW_VERSION);
//  cJSON_AddStringToObject(root, "api key", authorize_api_key);

  json_payload_point_free();
  json_payload = cJSON_Print(root);
  if (json_payload == NULL) {
      free(json_payload);
      json_payload = NULL;

      cJSON_Delete(root);
      return -1;
  }

  printf("Generated JSON:\n%s\n", json_payload);

  cJSON_Delete(root);
  return 1;
}

/*************************************************************************
 *  web socket make send cJSON current data & time..just only test...
 ************************************************************************/
int8_t make_cJSON_Epoch(void)
{
  cJSON *root = cJSON_CreateObject();

  if (root == NULL)
    return -1;

  cJSON_AddStringToObject(root, "Current Time", "2026-03-23D16:32:48T");

  // 3. "idTagInfo" 배열 생성 및 추가
  cJSON *idTagInfoArray = cJSON_AddArrayToObject(root, "idTagInfo");
  if (idTagInfoArray == NULL) {
      cJSON_Delete(root);
      return 0;
  }

  // 4. 배열 안에 들어갈 객체 생성 {"Status": "Accepted"}
  cJSON *statusObj = cJSON_CreateObject();
  cJSON_AddStringToObject(statusObj, "Status", "Accepted");

  // 5. 생성한 객체를 배열에 추가
  cJSON_AddItemToArray(idTagInfoArray, statusObj);


  // 6. JSON을 문자열로 변환하여 출력
  json_payload_point_free();
  json_payload = cJSON_Print(root);
  if (json_payload == NULL) {
      free(json_payload);
      json_payload = NULL; // 다시 NULL로 만들어 중복 해제 방지

      cJSON_Delete(root);
      return -1;
  }

  printf("Generated JSON:\n%s\n", json_payload);
  cJSON_Delete(root);

  return 1;
}


/*************************************************************************
 *  web socket make send cJSON Water Output Capacity
 ************************************************************************/
int8_t make_cJSON_Output_Capacity(void)
{
  cJSON *root = NULL;
  cJSON *waterFlowConfig = NULL;
  cJSON *waterItem = NULL;

  root = cJSON_CreateObject();
  if (root == NULL) {
    cJSON_Delete(root);
    return -1;
  }

  waterFlowConfig = cJSON_AddArrayToObject(root, "Water Output Config");
  if (waterFlowConfig == NULL)  {
      printf("[JSON] water array add fail\r\n");
      cJSON_Delete(root);
      return -1;
  }

  waterItem = cJSON_CreateObject();
  if(waterItem == NULL) {
    cJSON_Delete(root);
    return -1;
  }

  cJSON_AddNumberToObject(waterItem, "Cold Water Output",(int)user_info.water_output.cold_flow);
  cJSON_AddNumberToObject(waterItem, "Hot Water Output", (int)user_info.water_output.warm_flow);
  cJSON_AddNumberToObject(waterItem, "Sparkling Output", (int)user_info.water_output.sparkling);

  cJSON_AddItemToArray(waterFlowConfig, waterItem);

  json_payload_point_free();
  json_payload = cJSON_Print(root);
  if (json_payload == NULL) {
      cJSON_Delete(root);
      return -1;
  }

  printf("Generated JSON:\r\n%s\r\n", json_payload);

  cJSON_Delete(root);
  return 1;

}


/*************************************************************************
 *  web socket make send cJSON basic information
 ************************************************************************/
int8_t make_cJSON_Basic_Information(void)
{
  cJSON *root = NULL;
  cJSON *waterFlowConfig = NULL;
  cJSON *waterItem = NULL;
  uint8_t itemCount = 0;

  root = cJSON_CreateObject();
  if (root == NULL) {
    cJSON_Delete(root);
    return -1;
  }

  // 1. "Water Flow Config" 배열 생성 및 추가
  waterFlowConfig = cJSON_AddArrayToObject(root, "Water Flow Config");
  if (waterFlowConfig == NULL)  {
      printf("[JSON] water array add fail\r\n");
      cJSON_Delete(root);
      return -1;
  }

  // 2. 배열 안에 들어갈 첫 번째 객체 생성
  for(itemCount=0;itemCount<3;itemCount++) {
      waterItem = cJSON_CreateObject();
      if(waterItem == NULL) {
          cJSON_Delete(root);
          break;
      }

      cJSON_AddNumberToObject(waterItem, "Water Name", (double)itemCount+1);
      cJSON_AddNumberToObject(waterItem, "Water Flow", (double)user_flash_info.water_flow[itemCount]);
      cJSON_AddNumberToObject(waterItem, "Temperature",(double)user_flash_info.water_temp[itemCount]);

      // 3. 배열에 객체 추가
      cJSON_AddItemToArray(waterFlowConfig, waterItem);
  }


  json_payload_point_free();
  json_payload = cJSON_Print(root);
  if (json_payload == NULL) {
      cJSON_Delete(root);
      return -1;
  }

  printf("Generated JSON:\r\n%s\r\n", json_payload);

  // 7. 루트 객체 메모리 해제 (하위 모든 노드 포함)
  cJSON_Delete(root);

  return 1;
}

/*************************************************************************
 *  web socket make send cJSON status information
 ************************************************************************/
int8_t make_cJSON_Status_Information(void)
{
  char au8buff[16];

  cJSON *root = cJSON_CreateObject();

  if (root == NULL)
    return -1;

  memset(au8buff,0, sizeof(au8buff));
  au8buff[0] = GET_HOT_WATER_SWITCH()==0?'0':'1';                // 온수 스위치
  au8buff[1] = GET_CARBON_GAS_STATE()==0?'0':'1';                // 탄소 가스 상태
  au8buff[2] = GET_CARBON_GAS_LEAK()==0?'0':'1';                 // 탄소 외부 수위
  au8buff[3] = GET_CARBON_GAS_LEVEL()==0?'0':'1';                // 탄소 모듈 급수
  au8buff[4] = GET_COMPRESSOR_MOTOR()==0?'0':'1';                // 콤프레샤 가동
  au8buff[5] = GET_COLD_WATER_TEMP()==0?'0':'1';                 // 냉수 온도 센서
  au8buff[6] = GET_COLD_WATER_LEVEL()==0?'0':'1';                // 냉수 습수 상태
  au8buff[7] = GET_COOLING_STATE()==0?'0':'1';                   // 냉각 불량 상태
  cJSON_AddStringToObject(root, "Water Service Status", au8buff);

  memset(au8buff,0, sizeof(au8buff));
  au8buff[0] = GET_CALIBRATION_STATE()==0?'0':'1';                // 캘리브레이션 상태
  au8buff[1] = GET_HOT_WATER_SOL_STATE()==0?'0':'1';              // 순간 온수 솔 상태
  if(GET_MODULE_INIT()==1)              au8buff[2] = '1';         // 모듈 상태 초기화
  else if(GET_MODULE_STOP()==1)         au8buff[2] = '2';         // 모듈 상태 정지
  else if(GET_MODULE_RUNNING()==1)      au8buff[2] = '3';         // 모듈 상태 동작 중
  else if(GET_MODULE_STOP_FAULT()==1)   au8buff[2] = '4';         // 모듈 상태 정지 중 재 동작 불가
  else if(GET_MODULE_STOP_AND_GO()==1)  au8buff[2] = '5';         // 모듈 상태 정지 중 재 동작 가능
  else                                  au8buff[2] = '0';         // 상관 없음
  cJSON_AddStringToObject(root, "Module Status Info", au8buff);

  memset(au8buff,0, sizeof(au8buff));
  au8buff[0] = GET_COMM_SENSOR_ERROR()==0?'0':'1';                // 통신 센서 에러
  au8buff[1] = GET_FLUX_SENSOR_ERROR()==0?'0':'1';                // 유량 센서 에러
  au8buff[2] = GET_WATER_OUTLET_ERROR()==0?'0':'1';               // 출수 센서 에러
  au8buff[3] = GET_INNER_SENSOR_ERROR()==0?'0':'1';               // 내부 센서 에러
  au8buff[4] = GET_WATER_INLET_ERROR()==0?'0':'1';                // 입수 센서 에러
  au8buff[5] = GET_ZERO_CROSS_ERROR()==0?'0':'1';                 // 제로 크로스 에러
  cJSON_AddStringToObject(root, "Module Error Status", au8buff);

  memset(au8buff,0, sizeof(au8buff));
  au8buff[0] = GET_ALL_SENSOR_STATE()==0?'0':'1';                 // 제품 정상 작동 유무
  au8buff[1] = GET_UNUSED_LONG_TIME()==0?'0':'1';                 // 무 출력 상태 지속
  au8buff[2] = GET_UVC_LIFETIME_OVER()==0?'0':'1';                // UVC 수명 상태
  au8buff[3] = GET_WATER_VALVE_ERROR()==0?'0':'1';                // 단수 밸브 불랭 상태
  au8buff[4] = GET_WATER_LEAKAGE()==0?'0':'1';                    // 누수 발생
  au8buff[5] = GET_CONTINUOUS_HOT_WATER()==0?'0':'1';             // 온수 모듈 연속 가동 상태
  cJSON_AddStringToObject(root, "Run Status Info", au8buff);

  json_payload_point_free();

  json_payload = cJSON_Print(root);
  if (json_payload == NULL) {
      free(json_payload);
      json_payload = NULL; // 다시 NULL로 만들어 중복 해제 방지

      cJSON_Delete(root);
      return -1;

  }

  printf("Generated JSON:\n%s\n", json_payload);
  // 7. 루트 객체 메모리 해제 (하위 모든 노드 포함)
  cJSON_Delete(root);

  return 1;
}

/*************************************************************************
 *  web socket make send cJSON sensor information
 ************************************************************************/
int8_t make_cJSON_Sensor_Information(void)
{
  char au8buff[4];
  cJSON *root = cJSON_CreateObject();

  if (root == NULL)
    return -1;

  // 배열을 생성하고 그 안에 변수 값을 가진 객체를 넣습니다.
  cJSON *tempArray = cJSON_AddArrayToObject(root, "Temperature Sensor");
  cJSON *tempObj = cJSON_CreateObject();
  cJSON_AddNumberToObject(tempObj, "Cold Water Temp.", (double)pYoungWon->sensor_info.cold_water_Temp);
  cJSON_AddNumberToObject(tempObj, "Ambient Temp1.", (double)pYoungWon->sensor_info.ambient_1_Temp);
  cJSON_AddNumberToObject(tempObj, "Ambient Temp2.", (double)pYoungWon->sensor_info.ambient_2_Temp);
  cJSON_AddNumberToObject(tempObj, "Hot Water Temp.", (double)pYoungWon->sensor_info.warm_water_Temp);
  cJSON_AddItemToArray(tempArray, tempObj);


  // --- Water Quality Sensor 섹션 ---
  cJSON *qualityArray = cJSON_AddArrayToObject(root, "Water Quality Sensor");
  cJSON *qualityObj = cJSON_CreateObject();
  cJSON_AddNumberToObject(qualityObj, "After Filter Quality", (double)pYoungWon->sensor_info.AF_NTU);
  cJSON_AddNumberToObject(qualityObj, "Before Filter Quality",(double)pYoungWon->sensor_info.BF_NTU);


  // 상태 값은 문자열 변수로 처리
  memset(au8buff, 0, sizeof(au8buff));
  au8buff[0] = pYoungWon->sensor_info.water_Quality==0?'0':'1';
  cJSON_AddStringToObject(qualityObj, "Water Quality Status", au8buff);
  cJSON_AddItemToArray(qualityArray, qualityObj);

  // --- Flow Sensor 섹션 ---
  cJSON *flowArray = cJSON_AddArrayToObject(root, "Flow Sensor");
  cJSON *flowObj = cJSON_CreateObject();
  cJSON_AddNumberToObject(flowObj, "Flow Running", (double)pYoungWon->sensor_info.flow_value);
  cJSON_AddItemToArray(flowArray, flowObj);

  json_payload_point_free();

  json_payload = cJSON_Print(root);
  if (json_payload == NULL) {
      free(json_payload);
      json_payload = NULL; // 다시 NULL로 만들어 중복 해제 방지

      cJSON_Delete(root);
      return -1;
  }

  printf("Generated JSON:%s\r\n", json_payload);
  // 7. 루트 객체 메모리 해제 (하위 모든 노드 포함)
  cJSON_Delete(root);

  return 1;
}


/*************************************************************************
 *  web socket make send cJSON Software Verification
 ************************************************************************/
int8_t make_cJSON_Software_Verification(void)
{
  char au8buff[8];
  double Watt_hr = 0;

  cJSON *root = cJSON_CreateObject();

  if (root == NULL)
    return -1;

  // 배열을 생성하고 그 안에 변수 값을 가진 객체를 넣습니다.
  cJSON *tempArray = cJSON_AddArrayToObject(root, "Software Verification");
  cJSON *tempObj = cJSON_CreateObject();

  Watt_hr = (double)pYoungWon->valve_info.cold_Watt_hr;     // /100
  cJSON_AddNumberToObject(tempObj, "Cold Water Power", (double)Watt_hr);

  Watt_hr = (double)pYoungWon->valve_info.warm_Watt_hr;     // /100
  cJSON_AddNumberToObject(tempObj, "Hot Water Power", (double)Watt_hr);
  cJSON_AddNumberToObject(tempObj, "Filter Life Time", (double)pYoungWon->valve_info.life_filter);

  memset(au8buff, 0, sizeof(au8buff));
  au8buff[0] = valve_bit_state.b.bit3==0?'0':'1';
  au8buff[1] = valve_bit_state.b.bit2==0?'0':'1';
  au8buff[2] = valve_bit_state.b.bit1==0?'0':'1';
  au8buff[3] = valve_bit_state.b.bit0==0?'0':'1';
  cJSON_AddStringToObject(tempObj, "Valve Status", au8buff);
  cJSON_AddItemToArray(tempArray, tempObj);

  json_payload_point_free();
  json_payload = cJSON_Print(root);
  if (json_payload == NULL) {
      free(json_payload);
      json_payload = NULL; // 다시 NULL로 만들어 중복 해제 방지

      cJSON_Delete(root);
      return -1;
  }

  printf("Generated JSON:\n%s\n", json_payload);
  // 7. 루트 객체 메모리 해제 (하위 모든 노드 포함)
  cJSON_Delete(root);

  return 1;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
///
/// JSON DATA SEND & ANALYSIS
///

sl_websocket_error_t youngwon_ws_message_handler(void)
{
  sl_websocket_error_t ws_error = SL_WEBSOCKET_SUCCESS;

  switch(user_ws_app.now_step) {
    case WS_JSON_MSG_IDLE:
      break;

    case WS_JSON_MSG_INIT:
      break;

    case WS_JSON_MSG_idTAG:
      if(make_cJSON_idTag()) {
          ws_error = send_JSON_websocket_data();
      }
      break;

    case WS_JSON_MSG_BASIC_INFO:
      if(make_cJSON_Basic_Information()) {
          ws_error = send_JSON_websocket_data();
      }
      break;

    case WS_JSON_MSG_OUTPUT_CAPACITY:
      if(make_cJSON_Output_Capacity()) {
          ws_error = send_JSON_websocket_data();
      }
      break;

    case WS_JSON_MSG_STATUS_INFO:
      if(make_cJSON_Status_Information()) {
          ws_error = send_JSON_websocket_data();
      }
      break;

    case WS_JSON_MSG_SENSOR_INFO:
      if(make_cJSON_Sensor_Information()) {
          ws_error = send_JSON_websocket_data();
      }
      break;

    case WS_JSON_MSG_VERIFICATION:
      if(make_cJSON_Software_Verification()) {
          ws_error = send_JSON_websocket_data();
      }
      break;

    case WS_JSON_MSG_HEARTBEAT:
      if(make_cJSON_Heartbeat()) {
          ws_error = send_JSON_websocket_data();
      }
      break;

    default:
      break;
  }

  return ws_error;
}

/*******************************************************************************
 *  일정 시간 간격으로 메시지를 보내 도록 한다...1sec timer event
 *  모든 데이터는 기본 설정 시간 + 상태 변경 될 경우에만 데이터 전송을 한다.
 *  데이터 충돌을 피하기 위해 각자의 시간을 다르게 한다...
 ******************************************************************************/
void youngwon_ws_message_callback(void)
{
  if(user_ws_app.connect != CONNECT_SUCCESS)        // Wi-Fi connect & web server connect
    return;

  ws_send_elapsed_time++;

  if(user_ws_app.idTag_status == 0) {
    if(ws_send_elapsed_time%WS_INTERVAL_idTAG==0) {         // 10초 간격으로 idTag --> 수신 완료 되어야 그 다음으로 변경...
      ws_send_elapsed_time = 0;
      ws_send_waiting_time = 0;

      ws_app_set_event(WS_APP_DATA_SEND_MODE);             // 데이터 전송 준비를 한다...
      user_ws_app.now_step = WS_JSON_MSG_idTAG;
    }
  }

  else {                                                    // idTag 정보 수신 이후 현재 시간을 수신 받은 뒤 전송을 한다...

    if(ws_send_elapsed_time%WS_INTERVAL_HEAERBEAT == 0) {                       // 60초 간격으로
        ws_send_elapsed_time = 0;
        SET_WS_HEARTBEAT(1);
    }

//  출수 량은 출수 버튼 이후 최소 30분 동안 동작이 없을 경우에만 출수 량을 서버에 전송 한다
    if((water_amount_time==0)&&(GET_RCV_WATER_AMOUNT()==1)) {
        water_amount_time=60*30;
        SET_WS_OUTPUT_CAPACITY(1);
    }

/*******************************************************************************
 * DATA와 DATA 사이는 최소 2초 지연을 갖도록 한다...
 *******************************************************************************/
    if(((ws_send_bit_state.bAll&0xFE)!=0x00) && (ws_send_waiting_time==0)) {
      printf("ws_send_bit_state:%02x, %d\r\n", ws_send_bit_state.bAll, ws_send_waiting_time);

      ws_send_waiting_time = 10;                           // 100ms * 10 = 1000ms
      ws_app_set_event(WS_APP_DATA_SEND_MODE);             // 데이터 전송 준비를 한다...

      if(GET_WS_BASIC_INFO()==1) {
          SET_WS_BASIC_INFO(0);
          user_ws_app.now_step = WS_JSON_MSG_BASIC_INFO;
      }

      else if(GET_WS_OUTPUT_CAPACITY()==1) {
          SET_WS_OUTPUT_CAPACITY(0);
          user_ws_app.now_step = WS_JSON_MSG_OUTPUT_CAPACITY;
      }

      else if(GET_WS_STATUS_INFO()==1) {
          SET_WS_STATUS_INFO(0);
          user_ws_app.now_step = WS_JSON_MSG_STATUS_INFO;
      }

      else if(GET_WS_SENSOR_INFO()==1) {
          SET_WS_SENSOR_INFO(0);
          user_ws_app.now_step = WS_JSON_MSG_SENSOR_INFO;
      }

      else if(GET_WS_VERIFICATION()==1) {
          SET_WS_VERIFICATION(0);
          user_ws_app.now_step = WS_JSON_MSG_VERIFICATION;
      }

      else if(GET_WS_HEARTBEAT()==1) {
          SET_WS_HEARTBEAT(0);
          user_ws_app.now_step = WS_JSON_MSG_HEARTBEAT;
      }
    }
  }
}

