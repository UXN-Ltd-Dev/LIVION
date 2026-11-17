/*******************************************************************************
 * @file  ota_app.c
 * @brief HTTP/HTTPS OTAF 모듈 — 검증된 app.c 의 설정값 그대로 이식
 ******************************************************************************/
#include "http_fw_update.h"
#include "sl_wifi.h"
#include "sl_net.h"
#include "sl_net_dns.h"
#include "sl_net_si91x.h"
#include "sl_wifi_callback_framework.h"
#include "sl_wifi_types.h"
#include "sl_net_wifi_types.h"
#include "firmware_upgradation.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>

/* 인증서 — 기존과 동일 */
#include "aws_starfield_ca.pem.h"
#include "azure_baltimore_ca.pem.h"
#include "cacert.pem.h"

#ifdef SLI_SI91X_MCU_INTERFACE
#include "sl_si91x_hal_soc_soft_reset.h"
#endif

/*------------------------------------------------------------------------------
 * 매크로 — 기존 app.c와 1:1 동일
 *----------------------------------------------------------------------------*/
#ifndef BIT
#define BIT(a)                 ((uint32_t)1U << (a))
#endif

#define HTTPV6                 BIT(3)
#define HTTPS_SUPPORT          BIT(0)
#define HTTP_POST_DATA         BIT(5)
#define HTTP_V_1_1             BIT(6)
#define HTTP_USER_DEFINED_CT   BIT(7)
#define HTTP_OTAF              2

#define CERTIFICATE_INDEX      0          // OTA 전용
#define DNS_TIMEOUT            20000
#define MAX_DNS_RETRY_COUNT    5
#define OTAF_TIMEOUT_MS        600000

/*------------------------------------------------------------------------------
 * 서버 설정 — 기존 app.c 의 #ifdef AWS_ENABLE 블록 그대로
 *----------------------------------------------------------------------------*/
#ifdef AWS_ENABLE
  #define FLAGS                 HTTPS_SUPPORT
  #define HTTP_PORT             443
  #if (FW_UPDATE_TYPE == TA_FW_UPDATE)
    #define HTTP_URL            "rps/firmware.rps"
  #else
    #define HTTP_URL            "youngone_puri_IoT_isp.bin"
  #endif

  char *hostname = "mybucket-978796688028-ap-northeast-2-an.s3.ap-northeast-2.amazonaws.com";

  #define HTTP_EXTENDED_HEADER  NULL
  #define USERNAME              ""
  #define PASSWORD              ""
  #define SERVER_NAME           "AWS Server"

#elif defined(AZURE_ENABLE)
  #define FLAGS                 HTTPS_SUPPORT
  #define HTTP_PORT             443
  #define HTTP_URL              "rps/firmware.rps"
  static char *hostname        = "si917updates.blob.core.windows.net";
  #define HTTP_EXTENDED_HEADER  NULL
  #define USERNAME              ""
  #define PASSWORD              ""
  #define SERVER_NAME           "AZURE Server"

#else
  #define FLAGS                 0
  #define HTTP_PORT             3022
  #define HTTP_SERVER_IP_ADDR   "118.131.46.69"
  #if (FW_UPDATE_TYPE == TA_FW_UPDATE)
    #define HTTP_URL            "rps/firmware.rps"
  #else
    #define HTTP_URL            "~/firmware/wifi_http_otaf_soc_isp.bin"
  #endif
  static char *hostname        = HTTP_SERVER_IP_ADDR;
  #define HTTP_EXTENDED_HEADER  NULL
  #define USERNAME              "uxn"
  #define PASSWORD              "Uxn1234!"
  #define SERVER_NAME           "Local UXN Server"
#endif

/*------------------------------------------------------------------------------
 * 콜백 결과 플래그
 *----------------------------------------------------------------------------*/
volatile bool ota_response = false;

/*------------------------------------------------------------------------------
 * OTA 이벤트 콜백 — 기존 http_fw_update_response_handler 그대로
 *----------------------------------------------------------------------------*/
static sl_status_t ota_event_handler(sl_wifi_event_t event,
                                     uint16_t *data,
                                     uint32_t data_length,
                                     void *arg)
{
    (void)data; (void)data_length; (void)arg;

    if (SL_WIFI_CHECK_IF_EVENT_FAILED(event)) {
        ota_response = false;
        printf("[OTA] callback FAIL (event=0x%lX)\r\n", (uint32_t)event);
        return SL_STATUS_FAIL;
    }
    ota_response = true;
    printf("[OTA] callback SUCCESS\r\n");
    return SL_STATUS_OK;
}

/*------------------------------------------------------------------------------
 * CA 인증서 등록 — 기존 clear_and_load_certificates_in_flash() 그대로
 *----------------------------------------------------------------------------*/
sl_status_t ota_app_load_certificate(void)
{
#if LOAD_CERTIFICATE
    sl_status_t  status;
    void        *cert        = NULL;
    uint32_t     cert_length = 0;

  #ifdef AWS_ENABLE
    cert        = (void *)aws_starfield_ca;
    cert_length = sizeof(aws_starfield_ca) - 1;
  #elif defined(AZURE_ENABLE)
    cert        = (void *)azure_baltimore_ca;
    cert_length = sizeof(azure_baltimore_ca) - 1;
  #else
    cert        = (void *)cacert;
    cert_length = sizeof(cacert) - 1;
  #endif

    status = sl_net_set_credential(SL_NET_TLS_SERVER_CREDENTIAL_ID(CERTIFICATE_INDEX),
                                   SL_NET_SIGNING_CERTIFICATE,
                                   cert,
                                   cert_length);
    if (status != SL_STATUS_OK) {
        printf("[OTA] CA load FAIL: 0x%lX\r\n", status);
    } else {
        printf("[OTA] CA loaded at index %d\r\n", CERTIFICATE_INDEX);
    }
    return status;
#else
    return SL_STATUS_OK;
#endif
}

/*------------------------------------------------------------------------------
 * OTA 본체 — 기존 http_fw_update_application() 의 WLAN_FIRMWARE_UPDATE 케이스를
 *           함수로 추출, 설정값은 100% 동일
 *----------------------------------------------------------------------------*/
sl_status_t app_start_http_ota(void)
{
    sl_status_t status;
    uint16_t    flags = FLAGS;
    char        server_ip[16] = {0};

    /* CERTIFICATE_INDEX 비트 OR — 기존 코드와 동일 */
    if (CERTIFICATE_INDEX == 1) {
        flags |= SL_SI91X_HTTPS_CERTIFICATE_INDEX_1;
    } else if (CERTIFICATE_INDEX == 2) {
        flags |= SL_SI91X_HTTPS_CERTIFICATE_INDEX_2;
    }

    /* 1) 콜백 등록 + 결과 플래그 초기화 */
    ota_response = false;
    sl_wifi_set_callback(SL_WIFI_HTTP_OTA_FW_UPDATE_EVENTS,
                         (sl_wifi_callback_function_t)&ota_event_handler,
                         NULL);

    /* 2) 서버 IP 확보 */
#if defined(AWS_ENABLE) || defined(AZURE_ENABLE)
    sl_ip_address_t dns_query_rsp  = {0};
    int32_t         dns_retry_count = MAX_DNS_RETRY_COUNT;
    uint32_t        server_address;

    do {
        status = sl_net_dns_resolve_hostname((const char *)hostname,
                                             DNS_TIMEOUT,
                                             SL_NET_DNS_TYPE_IPV4,
                                             &dns_query_rsp);
        dns_retry_count--;
    } while ((dns_retry_count != 0) && (status != SL_STATUS_OK));

    if (status != SL_STATUS_OK) {
        printf("[OTA] DNS resolve FAIL: 0x%lX\r\n", status);
        return status;
    }
    printf("[OTA] DNS resolve OK\r\n");

    server_address = dns_query_rsp.ip.v4.value;
    sprintf(server_ip, "%ld.%ld.%ld.%ld",
            server_address & 0x000000ff,
            (server_address & 0x0000ff00) >> 8,
            (server_address & 0x00ff0000) >> 16,
            (server_address & 0xff000000) >> 24);
  #ifdef AWS_ENABLE
    printf("[OTA] Resolved AWS S3 IP = %s\r\n", server_ip);
  #else
    printf("[OTA] Resolved AZURE Blob IP = %s\r\n", server_ip);
  #endif
#else
    strcpy(server_ip, HTTP_SERVER_IP_ADDR);
    printf("[OTA] %s IP : %s\r\n", SERVER_NAME, server_ip);
#endif

    /* 3) 파라미터 구성 — 기존 http_params 와 1:1 동일 */
    sl_si91x_http_otaf_params_t http_params = {0};
    http_params.flags           = flags;
    http_params.ip_address      = (uint8_t *)server_ip;
    http_params.port            = (uint16_t)HTTP_PORT;
    http_params.resource        = (uint8_t *)HTTP_URL;
    http_params.host_name       = (uint8_t *)hostname;
    http_params.extended_header = (uint8_t *)HTTP_EXTENDED_HEADER;
    http_params.user_name       = (uint8_t *)USERNAME;
    http_params.password        = (uint8_t *)PASSWORD;

    /* 4) OTA 호출 */
    printf("[OTA] Firmware download from %s in progress...\r\n", SERVER_NAME);
    status = sl_si91x_http_otaf_v2(&http_params);

    if (status != SL_STATUS_OK) {
        printf("[OTA] sl_si91x_http_otaf_v2 FAIL: 0x%lX\r\n", status);
        return status;
    }
    printf("[OTA] download request OK, applying firmware...\r\n");
    return SL_STATUS_OK;
}
