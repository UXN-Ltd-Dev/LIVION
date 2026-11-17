/*******************************************************************************
* @file  user_config.h
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
 * @file         user_app.h
 * @version      0.1
 *
 *  @brief : This file contains user configurable details to configure the device
 *
 *  @section Description  This file contains user configurable details to configure the device
 *
 *
 */
#ifndef USER_APP_H
#define USER_APP_H

#include "sl_status.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif
/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/


#define USER_APP_IDLE_STATE       0x0000
#define USER_APP_TIMER_CALLBACK   0x0001
#define USER_APP_USART_CALLBACK   0x0002
#define USER_APP_WS_CALLBACK      0x0004
#define USER_APP_NVM_CALLBACK     0x0008
#define USER_APP_BLE_CALLBACK     0x0010
#define USER_APP_Wi_Fi_CALLBACK   0x0020

void user_app_set_event(uint32_t event_num);
void user_app_clear_event(uint32_t event_num);

void user_app_thread(void *argument);


// ============================================================================
// SiWG917(Si91x) Flash 메모리 주소 정의
// ============================================================================
void M4_init_user_common_flash(void);

void M4_erase_user_common_flash(void);
void M4_read_user_common_flash(user_flash_info_t *out_info);
void M4_write_user_common_flash(user_flash_info_t *in_info);
void M4_wi_fi_user_common_flash(void);

#ifdef __cplusplus
}
#endif

#endif
