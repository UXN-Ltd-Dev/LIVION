/***************************************************************************/ /**
 * @file  usart_async.h
 * @brief Control Board <-> Communication Board UART 통신 (youngone protocol)
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef USART_ASYNC_H_
#define USART_ASYNC_H_

/*******************************************************************************
 *************************** Macros   *******************************
 ******************************************************************************/

/*******************************************************************************
 ********************************   ENUMS   ************************************
 ******************************************************************************/
typedef enum {
  BASIC_INFORMATION=1,
  STATE_INFORMATION,

  SENSOR_INFORMATION=5,
  SOFTWARE_VERIFICATION=6,
  WATER_DISCHARGE_AMOUNT=7,
  COMMAND_ID_MAX
} youngone_cmd_id_e;

typedef enum {
  NO_ERROR=0,
  CRC_FAILED,
  UNKNOWN_CMD,
  BAD_OARAM,
  DATA_LENGTH_ERROR,
  STX_ERROR,
  PRODUCT_CODE_ERROR,
  ERROR_CODE_MAX
} youngone_data_error_e;

#define YOUNGONE_HEAD_1  0xFF
#define YOUNGONE_HEAD_2  0x81


#define PRODUCT_NAME_POS  2
#define COMMAND_POS       8
#define DATA_LENGTH_POS   10
#define CHECKSUM_POS      12
#define PAYLOAD_START_POS       14

extern uint8_t water_flow_out_flag;
extern uint8_t water_flow_out_step;


// -----------------------------------------------------------------------------
// Prototypes
// -----------------------------------------------------------------------------
// init / driver
void usart_async_init(void);
void set_usart_receive_data_callback(void);

// RX (user_app_thread context)
void   user_usart_receive_data_action(void);    // USER_APP_USART_CALLBACK 처리
void   youngone_rx_tick_50ms(void);             // 50ms 주기 - 미완성 프레임 타임아웃
int8_t youngone_receive_data_parser(uint8_t *receive_buff, const uint16_t receive_size);

// utilities
uint16_t youngone_checksum_calculation(uint8_t *payload, uint16_t length);
uint16_t make_Word(uint8_t *acBuff, uint8_t i);
uint32_t make_DWord(uint8_t *acBuff, uint8_t i);

// TX (thread context 전용 - 송신 완료까지 대기함)
void youngwon_information_request(uint8_t cmd_ID);
void youngwon_receive_response(uint8_t cmd_ID, uint8_t err_code);
void youngwon_water_out_control_setup(uint8_t water_mode);
void youngwon_recipe_control_setup(void);
void youngwon_software_verification_initial(uint8_t init_cmd);
void youngwon_send_water_flow_out_control(void);

// 다른 모듈에서 정의/사용 (이 파일에는 구현 없음)
uint8_t YT_T1C_data_parser(void);
void    YT_T1C_data_response(uint8_t err_status);
void    usart_async_process_action(void);

#endif /* USART_ASYNC_H_ */
