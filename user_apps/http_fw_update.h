/***************************************************************************/ /**
 * @file app.h
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef HTTP_FW_UPDATE_H
#define HTTP_FW_UPDATE_H

#include "sl_status.h"
#include <stdbool.h>

/*------------------------------------------------------------------------------
 * 컴파일 옵션 — 기존 app.c 와 동일한 분기 구조 유지
 *----------------------------------------------------------------------------*/
#define AWS_ENABLE          1     // 1: AWS S3 HTTPS, (정의 없음): 로컬 HTTP

#define M4_FW_UPDATE        0
#define TA_FW_UPDATE        1
#define COMBINED_FW_UPDATE  2
#define FW_UPDATE_TYPE      M4_FW_UPDATE

#define LOAD_CERTIFICATE    1     // 부팅 후 1회만 호출 권장

/*------------------------------------------------------------------------------
 * 외부 공개 함수
 *----------------------------------------------------------------------------*/

/** CA 인증서를 플래시에 등록 (HTTPS 사용 시 1회만 호출) */
sl_status_t ota_app_load_certificate(void);

/**
 * @brief OTA 다운로드 시작 (블로킹).
 *        호출 전 Wi-Fi 연결 + IP 할당이 끝나 있어야 함.
 * @return SL_STATUS_OK: 다운로드 요청 성공, NWP가 적용 중
 */
sl_status_t app_start_http_ota(void);

/** OTA 콜백 결과 (sl_wifi_set_callback 등록 핸들러에서 갱신) */
extern volatile bool ota_response;

#endif // HTTP_FW_UPDATE_H
