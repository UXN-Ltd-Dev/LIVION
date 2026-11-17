/*******************************************************************************
* @file  websocket_app.h
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
 * @file         websocket_app.h
 * @version      0.1
 *
 *  @brief : This file contains user configurable details to configure the device
 *
 *  @section Description  This file contains user configurable details to configure the device
 *
 *
 */
#ifndef WEBSOCKET_APP_H
#define WEBSOCKET_APP_H

#include "sl_websocket_client_types.h"


void ws_app_set_event(uint32_t event_num);
void websocket_app_thread(void *argument);

void make_cJSON_payload(void);
//void parser_cJSON_payload(const uint8_t *buffer);
void parser_cJSON_payload(const uint8_t *buffer, uint16_t length);

sl_websocket_error_t send_JSON_websocket_data(void);

void make_api_key_generation(void);
int8_t make_cJSON_AcceptedStatus(uint8_t err_code);

int8_t make_cJSON_idTag(void);
int8_t make_cJSON_Heartbeat(void);
int8_t make_cJSON_Epoch(void);
int8_t make_cJSON_Basic_Information(void);
int8_t make_cJSON_Status_Information(void);
int8_t make_cJSON_Sensor_Information(void);
int8_t make_cJSON_Software_Verification(void);

sl_websocket_error_t youngwon_ws_message_handler(void);
void youngwon_ws_message_callback(void);


#endif
