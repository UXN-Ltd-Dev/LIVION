/***************************************************************************/ /**
 * @file    usart_async.c
 * @brief   Control Board <-> Communication Board UART 통신 (youngone protocol)
 *
 *  - RX : USART ISR → 링버퍼 → user_app_thread 에서 스트림 파싱(STX/Length/CRC)
 *  - TX : 공통 프레임 빌더 youngone_send_frame() 로 송신 (mutex + 송신 완료 대기)
 *
 *  Frame : FF 81 | Product Code(6) | CMD | Status | Length | Reserved | CRC16(LE) | Payload
 *          (CRC 계산 시 CRC 필드는 0x0000 으로 간주)
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

#include "sl_si91x_driver.h"
#include "sl_si91x_usart.h"
#include "cmsis_os2.h"

#include "rsi_common_apis.h"
#include "rsi_bt_common_apis.h"

#include "../app.h"


/*******************************************************************************
 ******************************  CONFIGURATION  ********************************
 ******************************************************************************/
#define USART_BAUDRATE              9600u       // Baud rate <9600-7372800>

// Length 필드 의미
//  1 : 헤더 포함 전체 프레임 길이 (현재 제어보드 동작 기준)
//  0 : payload 길이 (프로토콜 문서 2-1 기준)
#ifndef YOUNGONE_LEN_IS_TOTAL
#define YOUNGONE_LEN_IS_TOTAL       1
#endif

#define YOUNGONE_RX_TIMEOUT_TICKS   4u          // 50ms x 4 = 200ms 동안 진전 없으면 재동기
#define RX_RING_SIZE                512u        // 2의 거듭제곱

#define YOUNGONE_TX_BUF_SIZE        64u
#define YOUNGONE_TX_RETRY           5u          // 최초 1회 + 재시도 5회
#define YOUNGONE_TX_RETRY_DELAY_MS  100u
#define YOUNGONE_TX_DONE_TIMEOUT_MS 100u        // 9600bps 64byte ≒ 67ms
#define YOUNGONE_TX_LOCK_TIMEOUT_MS 500u


/*******************************************************************************
 ****************************  PROTOCOL DEFINES  *******************************
 ******************************************************************************/
// PRODUCT_NAME_POS / COMMAND_POS / DATA_LENGTH_POS / CHECKSUM_POS / PAYLOAD_START_POS
// YOUNGONE_HEAD_1 / YOUNGONE_HEAD_2 는 usart_async.h 에 정의
#define STATUS_POS                  9
#define RESERVED_POS                11
#define YOUNGONE_HEADER_LEN         PAYLOAD_START_POS     // 14

#if YOUNGONE_LEN_IS_TOTAL
#define YOUNGONE_FRAME_LEN(len_field)   ((uint16_t)(len_field))
#define YOUNGONE_LEN_FIELD(frame_len)   ((uint8_t)(frame_len))
#define YOUNGONE_FRAME_MAX              255u
#else
#define YOUNGONE_FRAME_LEN(len_field)   ((uint16_t)(len_field) + YOUNGONE_HEADER_LEN)
#define YOUNGONE_LEN_FIELD(frame_len)   ((uint8_t)((frame_len) - YOUNGONE_HEADER_LEN))
#define YOUNGONE_FRAME_MAX              (255u + YOUNGONE_HEADER_LEN)
#endif

// Communication Board → Control Board command
#define TX_CMD_INFO_REQ_OFFSET      0x10        // 정보 요청 : 수신 CMD + 0x10
#define TX_CMD_WATER_CONTROL        0x21
#define TX_CMD_RECIPE_CONTROL       0x22
#define TX_CMD_SW_VERIFY_INIT       0x25

#define RX_RING_MASK                (RX_RING_SIZE - 1u)

// ISR ↔ 스레드 간 메모리 순서 보장
#if defined(__arm__) || defined(__thumb__)
#define RX_MEM_BARRIER()            __asm volatile ("dmb 0xF" ::: "memory")
#else
#define RX_MEM_BARRIER()            __sync_synchronize()
#endif

#define TEMP_THRESHOLD              2           // 센서 온도 변경 판단 기준(±2도 이내는 무시)


/*******************************************************************************
 ********************************   TYPES   ************************************
 ******************************************************************************/
typedef struct {
  uint32_t frame_ok;
  uint32_t crc_err;
  uint32_t len_err;
  uint32_t pcode_err;
  uint32_t dispatch_err;
  uint32_t ring_overflow;
  uint32_t timeout_resync;
  uint32_t drop_bytes;
} youngone_rx_stat_t;

typedef struct {
  uint32_t prev;
  bool     valid;
} counter_acc_t;


/*******************************************************************************
 ***************************   GLOBAL VARIABLES   ******************************
 ******************************************************************************/
sl_usart_handle_t usart_handle;

uint8_t water_flow_out_flag = 0;
uint8_t water_flow_out_step = 0;

youngone_rx_stat_t youngone_rx_stat;            // 디버거에서 통신 품질 확인용


/*******************************************************************************
 ****************************   LOCAL VARIABLES   ******************************
 ******************************************************************************/
// ---- USART driver ----
static uint8_t           usart_rx_byte;
static volatile bool     usart_send_complete = false;

// ---- RX ring buffer (ISR: head 만 씀 / 스레드: tail 만 씀) ----
static uint8_t           rx_ring[RX_RING_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

// ---- RX frame accumulator ----
static uint8_t           rx_acc[YOUNGONE_FRAME_MAX + 1];
static uint16_t          rx_acc_len   = 0;
static uint8_t           rx_idle_tick = 0;

// ---- TX ----
static uint8_t           usart_data_out[YOUNGONE_TX_BUF_SIZE];
static osMutexId_t       usart_tx_mutex = NULL;

// ---- 누적 카운터 (SOFTWARE_VERIFICATION) ----
static counter_acc_t     cold_kWh_acc;
static counter_acc_t     warm_kWh_acc;
static counter_acc_t     run_time_acc;


/*******************************************************************************
 ***********************   LOCAL FUNCTION PROTOTYPES   *************************
 ******************************************************************************/
void usart_callback_event(uint32_t event);
static void youngone_framer_run(void);
static sl_status_t youngone_send_frame(uint8_t cmd, const uint8_t *payload, uint8_t payload_len);


/*******************************************************************************
 *******************************   UTILITIES   *********************************
 ******************************************************************************/
/*******************************************************************************
 * 2 byte (Little Endian) -> uint16_t
 ******************************************************************************/
uint16_t make_Word(uint8_t *acBuff, uint8_t i)
{
  return (uint16_t)acBuff[i] | ((uint16_t)acBuff[i + 1] << 8);
}

/*******************************************************************************
 * 4 byte (Little Endian) -> uint32_t
 ******************************************************************************/
uint32_t make_DWord(uint8_t *acBuff, uint8_t i)
{
  return  (uint32_t)acBuff[i]
       | ((uint32_t)acBuff[i + 1] << 8)
       | ((uint32_t)acBuff[i + 2] << 16)
       | ((uint32_t)acBuff[i + 3] << 24);
}

void make_itoa(uint16_t word, uint8_t *acBuff, uint8_t i)
{
  acBuff[i] = (uint8_t)(word & 0xFF);            // 하위 8비트 (Low byte)
  acBuff[i + 1] = (uint8_t)((word >> 8) & 0xFF); // 상위 8비트 (High byte)
}

/*******************************************************************************
 * youngone CRC16
 *  - 실제 알고리즘 : CRC-16/MODBUS (poly 0xA001 reflected, init 0xFFFF) + 결과 byte swap
 *  - 프로토콜 문서에는 "CRC-16 CCITT" 로 표기되어 있으나 현재 제어보드는 이 방식으로 동작
 ******************************************************************************/
uint16_t youngone_checksum_calculation(uint8_t *payload, uint16_t length)
{
  uint16_t crc = 0xFFFF;
  uint16_t i;
  uint8_t  j;

  for (i = 0; i < length; i++) {
    crc ^= payload[i];
    for (j = 0; j < 8; j++) {
      crc = (crc & 0x0001) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
    }
  }

  return (uint16_t)((crc << 8) | (crc >> 8));
}

/*******************************************************************************
 * 센서 정보 비교
 ******************************************************************************/
char sensor_info_equal_mem(const sensor_info_t *a, const sensor_info_t *b)
{
  return (memcmp(a, b, sizeof(sensor_info_t)) == 0);
}

static inline char is_temp_changed(uint8_t a, uint8_t b)
{
  return (abs((int)a - (int)b) > TEMP_THRESHOLD);
}

char sensor_info_changed(const sensor_info_t *a, const sensor_info_t *b)
{
  if (is_temp_changed(a->ambient_1_Temp,  b->ambient_1_Temp))  return 1;
  if (is_temp_changed(a->ambient_2_Temp,  b->ambient_2_Temp))  return 1;
  if (is_temp_changed(a->cold_water_Temp, b->cold_water_Temp)) return 1;
  if (is_temp_changed(a->warm_water_Temp, b->warm_water_Temp)) return 1;

  if (a->AF_NTU        != b->AF_NTU)        return 1;
  if (a->BF_NTU        != b->BF_NTU)        return 1;
  if (a->water_Quality != b->water_Quality) return 1;
//  if (a->flow_value    != b->flow_value)    return 1;     // 초단위로 변경 되기 때문에 이 부분은 삭제..

  return 0;
}

/*******************************************************************************
 * 제어보드 누적 카운터 → 통신보드 누계 반영 (증분 방식)
 *  - 부팅 후 첫 수신값은 기준값으로만 사용 (누계에 더하지 않음)
 *  - 수신값이 이전값보다 작으면 제어보드 카운터 리셋으로 보고 수신값을 증분으로 더함
 *    (자정 0x07 초기화 명령 이후 이 경로)
 ******************************************************************************/
static void accumulate_counter(counter_acc_t *acc, uint32_t *total, uint32_t rx)
{
  if (acc->valid) {
    *total += (rx >= acc->prev) ? (rx - acc->prev) : rx;
  }
  acc->prev  = rx;
  acc->valid = true;
}


/*******************************************************************************
 ****************************   INITIALIZATION   *******************************
 ******************************************************************************/
void usart_async_init(void)
{
  sl_status_t status;
  sl_si91x_usart_control_config_t usart_config;
  sl_si91x_usart_control_config_t get_config;

  memset(&usart_config, 0, sizeof(usart_config));
  usart_config.baudrate      = USART_BAUDRATE;
  usart_config.mode          = SL_USART_MODE_ASYNCHRONOUS;
  usart_config.parity        = SL_USART_NO_PARITY;
  usart_config.stopbits      = SL_USART_STOP_BITS_1;
  usart_config.hwflowcontrol = SL_USART_FLOW_CONTROL_NONE;
  usart_config.databits      = SL_USART_DATA_BITS_8;
  usart_config.misc_control  = SL_USART_MISC_CONTROL_NONE;
  usart_config.usart_module  = USART_0;
  usart_config.config_enable = ENABLE;
  usart_config.synch_mode    = DISABLE;

  if (usart_tx_mutex == NULL) {
    usart_tx_mutex = osMutexNew(NULL);
  }

  status = sl_si91x_usart_init(USART_0, &usart_handle);
  if (status != SL_STATUS_OK) {
    DEBUGOUT("sl_si91x_usart_init: Error Code : %lu \r\n", status);
    return;
  }

  status = sl_si91x_usart_set_configuration(usart_handle, &usart_config);
  if (status != SL_STATUS_OK) {
    DEBUGOUT("sl_si91x_usart_set_configuration: Error Code : %lu \r\n", status);
    return;
  }

  status = sl_si91x_usart_multiple_instance_register_event_callback(USART_0, usart_callback_event);
  if (status != SL_STATUS_OK) {
    DEBUGOUT("sl_si91x_usart_register_event_callback: Error Code : %lu \r\n", status);
    return;
  }

  sl_si91x_usart_get_configurations(USART_0, &get_config);
  DEBUGOUT("USART init OK, Baud Rate = %ld \r\n", get_config.baudrate);

  set_usart_receive_data_callback();
}

/*******************************************************************************
 * 1 byte 비동기 수신 등록 (수신 완료 콜백에서 매번 재등록)
 ******************************************************************************/
void set_usart_receive_data_callback(void)
{
  (void)sl_si91x_usart_async_receive_data(usart_handle, &usart_rx_byte, 1);
}


/*******************************************************************************
 ******************************   USART ISR   **********************************
 ******************************************************************************/
static inline void rx_ring_push_isr(uint8_t c)
{
  uint16_t next = (uint16_t)((rx_head + 1u) & RX_RING_MASK);

 // LOG_PRINT("%02X ", c);

  if (next == rx_tail) {                    // full → 새 바이트 버림
    youngone_rx_stat.ring_overflow++;
    return;
  }
  rx_ring[rx_head] = c;
  RX_MEM_BARRIER();                         // 데이터 기록 후 head 갱신
  rx_head = next;
}

/*******************************************************************************
 * USART event callback (ISR context)
 *  - 여기서는 링버퍼 push / 플래그 set 만 한다. printf 금지.
 ******************************************************************************/
void usart_callback_event(uint32_t event)
{
  switch (event) {
    case SL_USART_EVENT_SEND_COMPLETE:
      usart_send_complete = true;
      break;

    case SL_USART_EVENT_RECEIVE_COMPLETE:
      rx_ring_push_isr(usart_rx_byte);
      set_usart_receive_data_callback();
      user_app_set_event(USER_APP_USART_CALLBACK);
      break;

    default:
      break;
  }
}


/*******************************************************************************
 ***************************   RX STREAM PARSER   ******************************
 *
 *  1) STX(FF 81) 탐색, 앞쪽 쓰레기 제거
 *  2) 헤더(14B) 확보 → Product Code 'Y' / Length 범위 확인
 *  3) Length 만큼 모이면 CRC 확인
 *       OK   : 디스패치 후 frame_len 제거 → 뒤에 붙은 프레임 계속 처리
 *       FAIL : 1byte 만 버리고 재탐색 (가짜 STX 재동기)
 *  4) 미완성 프레임이 YOUNGONE_RX_TIMEOUT_TICKS 동안 정체되면 1byte 버리고 재동기
 ******************************************************************************/
static inline bool rx_ring_pop(uint8_t *c)
{
  uint16_t tail = rx_tail;

  if (tail == rx_head) {
    return false;
  }
  *c = rx_ring[tail];
  RX_MEM_BARRIER();
  rx_tail = (uint16_t)((tail + 1u) & RX_RING_MASK);
  return true;
}

static void rx_acc_drop(uint16_t n)
{
  if (n >= rx_acc_len) {
    rx_acc_len = 0;
    return;
  }
  memmove(rx_acc, &rx_acc[n], rx_acc_len - n);
  rx_acc_len -= n;
}

static void rx_acc_discard(uint16_t n)
{
  youngone_rx_stat.drop_bytes += n;
  rx_acc_drop(n);
}

// CRC 필드를 0x0000 으로 두고 계산한 뒤 원본 복원
static bool youngone_crc_ok(uint8_t *f, uint16_t frame_len)
{
  uint8_t  c0 = f[CHECKSUM_POS];
  uint8_t  c1 = f[CHECKSUM_POS + 1];
  uint16_t calc;

  f[CHECKSUM_POS]     = 0;
  f[CHECKSUM_POS + 1] = 0;
  calc = youngone_checksum_calculation(f, frame_len);
  f[CHECKSUM_POS]     = c0;
  f[CHECKSUM_POS + 1] = c1;

  return (((uint16_t)c0 | ((uint16_t)c1 << 8)) == calc);
}

static void youngone_framer_run(void)
{
  //uint8_t exception = 0;
  uint16_t i, frame_len;

  for (;;) {        // 1) STX 탐색

// STEP1 : STX 탐색
    for (i = 0; (i + 1u) < rx_acc_len; i++) {
      if (rx_acc[i] == YOUNGONE_HEAD_1 && rx_acc[i + 1u] == YOUNGONE_HEAD_2) {
          break;
      }
    }

// STEP2 : STX 없음 : 마지막 바이트가 0xFF 면 다음 바이트와 이어질 수 있으므로 남김
    if ((i + 1u) >= rx_acc_len) {
      i = (rx_acc_len > 0 && rx_acc[rx_acc_len - 1u] == YOUNGONE_HEAD_1)
          ? (uint16_t)(rx_acc_len - 1u) : rx_acc_len;
    }
    if (i > 0) {
      rx_acc_discard(i);
    }

// STEP3 : 헤더 확보 및 검사
    if (rx_acc_len < YOUNGONE_HEADER_LEN) {
      return;
    }

    if (rx_acc[PRODUCT_NAME_POS] != 'Y') {
      youngone_rx_stat.pcode_err++;
      rx_acc_discard(1);
      continue;
    }

    frame_len = YOUNGONE_FRAME_LEN(rx_acc[DATA_LENGTH_POS]);

    if (frame_len < YOUNGONE_HEADER_LEN || frame_len > YOUNGONE_FRAME_MAX) {

      youngone_rx_stat.len_err++;
      rx_acc_discard(1);
      continue;
    }

#if 0
    if(frame_len == 0x13) {
        frame_len = 0x12;
        exception = 1;
    }
#endif

// STEP4 : 프레임 전체 확보 및 CRC
    if (rx_acc_len < frame_len) {
      return;
    }

//    if(exception == 0) {
      if (!youngone_crc_ok(rx_acc, frame_len)) {
        LOG_PRINT("youngone_crc_error\r\n");
        youngone_rx_stat.crc_err++;
        rx_acc_discard(1);
        continue;
      }
//    }

// STEP5 : 정상 프레임 처리
    if (youngone_receive_data_parser(rx_acc, frame_len) == NO_ERROR) {
      youngone_rx_stat.frame_ok++;
    }
    else {
      youngone_rx_stat.dispatch_err++;
    }

    rx_acc_drop(frame_len);
  }
}

/*******************************************************************************
 * USER_APP_USART_CALLBACK 처리 (user_app_thread)
 ******************************************************************************/
void user_usart_receive_data_action(void)
{
  uint8_t c;
  bool    got = false;

  while (rx_ring_pop(&c)) {
    got = true;

    if (rx_acc_len >= sizeof(rx_acc)) {     // 이론상 발생하지 않음
      rx_acc_discard(1);
    }
    rx_acc[rx_acc_len++] = c;

    if (rx_acc_len >= YOUNGONE_HEADER_LEN) {
      youngone_framer_run();
    }
  }

  if (got) {
    rx_idle_tick = 0;
    youngone_framer_run();
  }
}

/*******************************************************************************
 * 50ms 주기 호출 (ulp_timer_event_action_process)
 ******************************************************************************/
void youngone_rx_tick_50ms(void)
{
  if (rx_acc_len == 0) {
    rx_idle_tick = 0;
    return;
  }

  if (++rx_idle_tick >= YOUNGONE_RX_TIMEOUT_TICKS) {
    rx_idle_tick = 0;
    youngone_rx_stat.timeout_resync++;
    rx_acc_discard(1);
    youngone_framer_run();
  }
}


/*******************************************************************************
 ***************************   RX COMMAND HANDLER   ****************************
 ******************************************************************************/
static uint8_t youngone_cmd_min_payload(uint8_t cmd)
{
  switch (cmd) {
    case BASIC_INFORMATION:      return 8;
    case STATE_INFORMATION:      return 4;
    case SENSOR_INFORMATION:     return 7;
    case SOFTWARE_VERIFICATION:  return 13;
    case WATER_DISCHARGE_AMOUNT: return 6;
    default:                     return 0;
  }
}

static void rx_basic_information(uint8_t *p)
{
  pYoungWon->basic_info.ver_main  = p[0];
  pYoungWon->basic_info.ver_sub1  = p[1];
  pYoungWon->basic_info.ver_sub2  = p[2];
  pYoungWon->basic_info.device_id = make_Word(p, 3);
  pYoungWon->basic_info.year      = p[5];
  pYoungWon->basic_info.month     = p[6];
  pYoungWon->basic_info.day       = p[7];

#if DEBUG_ASYNC_MSG
  LOG_PRINT("version : [%d].[%d].[%d]\r\n", pYoungWon->basic_info.ver_main,
            pYoungWon->basic_info.ver_sub1, pYoungWon->basic_info.ver_sub2);
  LOG_PRINT("device ID : [%ld]\r\n", pYoungWon->basic_info.device_id);
  LOG_PRINT("date : [%d]-[%d]-[%d]\r\n", pYoungWon->basic_info.year,
            pYoungWon->basic_info.month, pYoungWon->basic_info.day);
#endif
}

static void update_state_byte(tyBITS *dst, uint8_t value)
{
  if (dst->bAll != value) {
    dst->bAll = value;
    SET_WS_STATUS_INFO(1);
  }
}

static void rx_state_information(uint8_t *p)
{
  update_state_byte(&device_bit_state, p[0]);
  update_state_byte(&module_bit_state, p[1]);
  update_state_byte(&module_bit_error, p[2]);
  update_state_byte(&active_bit_state, p[3]);

#if DEBUG_ASYNC_MSG
  LOG_PRINT("State Info : [%02X] / [%02X] / [%02X] / [%02X]\r\n",
            device_bit_state.bAll, module_bit_state.bAll,
            module_bit_error.bAll, active_bit_state.bAll);
  LOG_PRINT("=> [%d] [%d] [%d] [%d] [%d] [%d] [%d] [%d] \r\n", GET_HOT_WATER_SWITCH(),
            GET_CARBON_GAS_STATE(), GET_CARBON_GAS_LEAK(), GET_CARBON_GAS_LEVEL(),
            GET_COMPRESSOR_MOTOR(), GET_COLD_WATER_TEMP(), GET_COLD_WATER_LEVEL(), GET_COOLING_STATE());
#endif
}

static void rx_sensor_information(uint8_t *p)
{
  sensor_info_t s_info;

  memset(&s_info, 0, sizeof(s_info));
  s_info.cold_water_Temp = p[0];
  s_info.ambient_1_Temp  = p[1];
  s_info.ambient_2_Temp  = p[2];
  s_info.warm_water_Temp = p[3];
  s_info.BF_NTU          = p[4];
  s_info.AF_NTU          = p[5];
  s_info.water_Quality   = p[6];
  s_info.flow_value      = p[7];

  if (sensor_info_changed(&pYoungWon->sensor_info, &s_info)) {   // 온도 ±2도 이내는 전송 안 함
    pYoungWon->sensor_info = s_info;
    SET_WS_SENSOR_INFO(1);
  }

#if DEBUG_ASYNC_MSG
  LOG_PRINT("SENSOR : [%d][%d][%d][%d] / [%d]\r\n", pYoungWon->sensor_info.cold_water_Temp,
            pYoungWon->sensor_info.ambient_1_Temp, pYoungWon->sensor_info.ambient_2_Temp,
            pYoungWon->sensor_info.warm_water_Temp, s_info.flow_value);
#endif
}

static void rx_software_verification(uint8_t *p)
{
  uint32_t cold_kWh = make_DWord(p, 0);
  uint32_t warm_kWh = make_DWord(p, 4);
  uint32_t run_time = make_DWord(p, 8);

  valve_bit_state.bAll = p[12];
  pYoungWon->valve_info.cold_Watt_hr = cold_kWh;
  pYoungWon->valve_info.warm_Watt_hr = warm_kWh;
  pYoungWon->valve_info.life_filter = run_time;

  SET_WS_VERIFICATION(1);



#if DEBUG_ASYNC_MSG
  LOG_PRINT("SOFTWARE : [%ld][%ld][%ld][%02X]\r\n", pYoungWon->valve_info.cold_Watt_hr,
            pYoungWon->valve_info.warm_Watt_hr, pYoungWon->valve_info.life_filter, valve_bit_state.bAll);
#endif
}

static void rx_water_discharge_amount(uint8_t *p)
{
  pYoungWon->water_amount.cold_water = make_Word(p, 0);
  pYoungWon->water_amount.warm_water = make_Word(p, 2);
  pYoungWon->water_amount.sparkling  = make_Word(p, 4);

  SET_WS_OUTPUT_CAPACITY(1);                  // 현재 출수량을 전송 한다.
}

/*******************************************************************************
 * youngone protocol parser
 *  - youngone_framer_run() 에서 STX / Product Code / Length / CRC 검증이 끝난
 *    단일 프레임만 들어온다.
 ******************************************************************************/
int8_t youngone_receive_data_parser(uint8_t *receive_buff, const uint16_t receive_size)
{
  uint8_t  cmd     = receive_buff[COMMAND_POS];
  uint8_t *payload = &receive_buff[PAYLOAD_START_POS];

  if (receive_size < (uint16_t)(PAYLOAD_START_POS + youngone_cmd_min_payload(cmd))) {
    return DATA_LENGTH_ERROR;
  }

  memset(pYoungWon->basic_info.p_name, 0, sizeof(pYoungWon->basic_info.p_name));
  memcpy(pYoungWon->basic_info.p_name, &receive_buff[PRODUCT_NAME_POS], 6);

#if DEBUG_ASYNC_MSG
  LOG_PRINT(" command : [%d] len : [%d]\r\n", cmd, receive_size);
#endif

  switch (cmd) {
    case BASIC_INFORMATION:      rx_basic_information(payload);      break;
    case STATE_INFORMATION:      rx_state_information(payload);      break;
    case SENSOR_INFORMATION:     rx_sensor_information(payload);     break;
    case SOFTWARE_VERIFICATION:  rx_software_verification(payload);  break;
    case WATER_DISCHARGE_AMOUNT: rx_water_discharge_amount(payload); break;
    default:
      return UNKNOWN_CMD;
  }

  return NO_ERROR;
}


/*******************************************************************************
 ********************************   TX   ***************************************
 ******************************************************************************/
static uint32_t ms_to_ticks(uint32_t ms)
{
  uint32_t t = (ms * osKernelGetTickFreq()) / 1000u;
  return (t == 0u) ? 1u : t;
}

static bool usart_wait_send_complete(uint32_t timeout_ms)
{
  uint32_t start   = osKernelGetTickCount();
  uint32_t timeout = ms_to_ticks(timeout_ms);

  while (!usart_send_complete) {
    if ((osKernelGetTickCount() - start) >= timeout) {
      return false;
    }
    osDelay(1);
  }
  return true;
}

/*******************************************************************************
 * 공통 프레임 송신 (스레드 context 전용)
 *  - 헤더/Length/CRC 를 채워서 송신하고 송신 완료까지 대기
 *  - mutex 로 송신 버퍼 보호 (여러 스레드에서 호출되어도 안전)
 ******************************************************************************/
static sl_status_t youngone_send_frame(uint8_t cmd, const uint8_t *payload, uint8_t payload_len)
{
  uint16_t    frame_len = (uint16_t)(YOUNGONE_HEADER_LEN + payload_len);
  uint16_t    crc;
  uint8_t     attempt;
  sl_status_t status = SL_STATUS_FAIL;

  if (frame_len > sizeof(usart_data_out)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (usart_tx_mutex != NULL &&
      osMutexAcquire(usart_tx_mutex, ms_to_ticks(YOUNGONE_TX_LOCK_TIMEOUT_MS)) != osOK) {
    return SL_STATUS_BUSY;
  }

  // ---- frame build ----
  memset(usart_data_out, 0, YOUNGONE_HEADER_LEN);
  usart_data_out[0] = YOUNGONE_HEAD_1;
  usart_data_out[1] = YOUNGONE_HEAD_2;
  strncpy((char *)&usart_data_out[PRODUCT_NAME_POS], (char *)pYoungWon->basic_info.p_name, 6);
  usart_data_out[COMMAND_POS]     = cmd;
  usart_data_out[STATUS_POS]      = 0x00;     // No Reply
  usart_data_out[DATA_LENGTH_POS] = YOUNGONE_LEN_FIELD(frame_len);
  usart_data_out[RESERVED_POS]    = 0x00;
  if (payload_len > 0) {
    memcpy(&usart_data_out[PAYLOAD_START_POS], payload, payload_len);
  }

  crc = youngone_checksum_calculation(usart_data_out, frame_len);
  usart_data_out[CHECKSUM_POS]     = make_Byte(crc, 0);
  usart_data_out[CHECKSUM_POS + 1] = make_Byte(crc, 1);

  // ---- send ----
  for (attempt = 0; attempt <= YOUNGONE_TX_RETRY; attempt++) {
    usart_send_complete = false;
    status = sl_si91x_usart_async_send_data(usart_handle, usart_data_out, frame_len);
    if (status == SL_STATUS_OK) {
      if (!usart_wait_send_complete(YOUNGONE_TX_DONE_TIMEOUT_MS)) {
        status = SL_STATUS_TIMEOUT;
      }
      break;
    }
    osDelay(ms_to_ticks(YOUNGONE_TX_RETRY_DELAY_MS));
  }

#if DEBUG_ASYNC_PACKET
  printf("TX[%d] =>", frame_len);
  for (uint16_t i = 0; i < frame_len; i++) {
    printf(" %02x", usart_data_out[i]);
  }
  printf(" (%lu)\r\n", status);
#endif

  if (usart_tx_mutex != NULL) {
    osMutexRelease(usart_tx_mutex);
  }
  return status;
}

/*******************************************************************************
 * 정보 요청 (Communication Board -> Control Board)
 ******************************************************************************/
void youngwon_information_request(uint8_t cmd_ID)
{
  (void)youngone_send_frame((uint8_t)(cmd_ID + TX_CMD_INFO_REQ_OFFSET), NULL, 0);
}

/*******************************************************************************
 * 수신 응답 (Communication Board -> Control Board)
 ******************************************************************************/
void youngwon_receive_response(uint8_t cmd_ID, uint8_t err_code)
{
  uint8_t payload[5];

  payload[0] = err_code;
  payload[1] = pYoungWon->basic_info.ver_main;
  payload[2] = pYoungWon->basic_info.ver_sub1;
  payload[3] = pYoungWon->basic_info.ver_sub2;
  payload[4] = wlan_bit_state.bAll;

  (void)youngone_send_frame(cmd_ID, payload, sizeof(payload));
}

/*******************************************************************************
 * 출수량 설정값 → 프로토콜 레벨
 *  120ml:0 / 250ml:1 / 550ml:2 / 1000ml:3(allow_1l 인 경우만), 그 외는 0 (120ml)
 ******************************************************************************/
static uint8_t water_flow_to_level(uint16_t flow, bool allow_1l, bool *invalid)
{
  *invalid = false;

  if (flow == 250)               return 1;
  if (flow == 550)               return 2;
  if (flow == 1000 && allow_1l)  return 3;
  if (flow != 120)               *invalid = true;
  return 0;
}

/*******************************************************************************
 * 출수 설정 (Communication Board -> Control Board)
 ******************************************************************************/
void youngwon_water_out_control_setup(uint8_t water_mode)
{
  uint8_t payload[3];
  uint8_t idx;
  bool    allow_1l;
  bool    invalid;

  if (water_mode == COLD_WATER_MODE) {
    idx = WATER_NAME_COLD;  allow_1l = false;
  } else if (water_mode == HOT_WATER_MODE) {
    idx = WATER_NAME_HOT;   allow_1l = true;
  } else {
    water_mode = SPARKLING_MODE;
    idx = WATER_SPARKLING;  allow_1l = true;
  }

  payload[0] = water_mode;
  payload[1] = water_flow_to_level(user_flash_info.water_flow[idx], allow_1l, &invalid);
  payload[2] = user_flash_info.water_temp[idx];

  if (invalid) {
    user_flash_info.water_flow[idx] = 120;   // 지원하지 않는 값 → 기본 120ml (저장은 안 함)
  }

  (void)youngone_send_frame(TX_CMD_WATER_CONTROL, payload, sizeof(payload));
}

/*******************************************************************************
 * 레시피 설정 (Communication Board -> Control Board)
 *  - 레시피는 저장하지 않고 웹에서 변경한 값을 그대로 전달 (단위 10ml)
 ******************************************************************************/
void youngwon_recipe_control_setup(void)
{
  uint8_t payload[4];

  payload[0] = (uint8_t)(user_info.water_recipe.cold_flow);
  payload[1] = (uint8_t)(user_info.water_recipe.warm_flow);
  make_itoa(user_info.water_recipe.sparkling, payload, 2);

  (void)youngone_send_frame(TX_CMD_RECIPE_CONTROL, payload, sizeof(payload));
}

/*******************************************************************************
 * 모든 누적 정보 초기화 (Communication Board -> Control Board)
 ******************************************************************************/
void youngwon_data_initialize(void)
{
  uint8_t payload[4];

  payload[0] = (uint8_t)(user_info.water_recipe.cold_flow);
  payload[1] = (uint8_t)(user_info.water_recipe.warm_flow);
  make_itoa(user_info.water_recipe.sparkling, payload, 2);

  (void)youngone_send_frame(TX_CMD_RECIPE_CONTROL, payload, sizeof(payload));
}



/*******************************************************************************
 * Software verification 초기화 (Communication Board -> Control Board)
 *  - 현재 날짜/시간과 초기화 명령 전달
 ******************************************************************************/
void youngwon_software_verification_initial(uint8_t init_cmd)
{
  uint8_t payload[6];

  payload[0] = user_calendar.Year;
  payload[1] = user_calendar.Month;
  payload[2] = user_calendar.Day;
  payload[3] = user_calendar.Hour;
  payload[4] = user_calendar.Minute;
  payload[5] = init_cmd;

  (void)youngone_send_frame(TX_CMD_SW_VERIFY_INIT, payload, sizeof(payload));
}

/*******************************************************************************
 * 설정된 출수(냉/온/탄산)를 100ms 간격으로 Control Board 로 순차 전송
 ******************************************************************************/
void youngwon_send_water_flow_out_control(void)
{
  static const uint8_t mode_seq[] = { COLD_WATER_MODE, HOT_WATER_MODE, SPARKLING_MODE };

  if (water_flow_out_flag == 0) {
    return;
  }

  if (water_flow_out_step >= sizeof(mode_seq)) {
    water_flow_out_step = 0;
  }

  youngwon_water_out_control_setup(mode_seq[water_flow_out_step]);

  if (++water_flow_out_step >= sizeof(mode_seq)) {
    water_flow_out_step = 0;
    water_flow_out_flag = 0;
  }
}
