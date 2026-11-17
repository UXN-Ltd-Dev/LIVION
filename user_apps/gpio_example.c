/*******************************************************************************
 * @file  gpio_example.c
 * @brief GPIO example
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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
/**============================================================================
 * @brief : This file contains example application for GPIO example
 * @section Description :
 * This application demonstrates the HP pin interrupt and toggling of HP GPIO pin.
 ============================================================================**/
#include "gpio_example.h"
#include "sl_si91x_driver_gpio.h"
#include "sl_gpio_board.h"
#include "rsi_debug.h"

#include "../app.h"

extern void wifi_app_set_event(uint32_t event_num);

/*******************************************************************************
 ***************************  Defines / Macros  ********************************
 ******************************************************************************/
#define AVL_INTR_NO 0 // available interrupt number
#define INT_CH      0 // GPIO Pin interrupt 0


#define BUTTON_PRESSED      1
#define BUTTON_LONG_TIME    80
/*******************************************************************************
 ********************************   ENUMS   ************************************
 ******************************************************************************/

/*******************************************************************************
 *************************** LOCAL VARIABLES   *********************************
 ******************************************************************************/
// Define GPIO port and pin for HP instance. Here SL_SI91X_GPIO_6_PORT corresponds to GPIO pin 6's port number : 0.
// This is defined to '0' which is PORT_A and GPIO_PIN_NUMBER6 refers to GPIO pin number 6.
// Same representation followed for other port and pins.
static sl_si91x_gpio_pin_config_t sl_gpio_pin_config = { { SL_SI91X_GPIO_6_PORT, GPIO_PIN_NUMBER6 }, GPIO_OUTPUT };
// Define GPIO port and pin for HP instance. Here SL_SI91X_GPIO_11_PORT corresponds to GPIO pin 11's port number : 0.
// This is defined to '0' which is PORT_A and GPIO_PIN_NUMBER11 refers to GPIO pin number 11.
// Same representation followed for other port and pins.
//static sl_si91x_gpio_pin_config_t sl_gpio_pin_config1 = { { SL_SI91X_GPIO_11_PORT, GPIO_PIN_NUMBER11 }, GPIO_INPUT };
static sl_si91x_gpio_pin_config_t sl_gpio_pin_config1 = { { SL_SI91X_GPIO_57_PORT, SL_SI91X_GPIO_57_PIN }, GPIO_INPUT };
/*******************************************************************************
 **********************  Local Function prototypes   ***************************
 ******************************************************************************/
static void gpio_pin_interrupt0_callback(uint32_t pin_intr);
/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/
/*******************************************************************************
 * GPIO example initialization function. It initializes clock, pin mode,
 * direction and configures pin interrupt
 ******************************************************************************/
button_state_t  button_state;
uint8_t previous_pin_state = 0xFF;

int8_t send_sample_count = 0;

void button_gpio_init(void)
{
  sl_status_t status;

  do {
    // Initialize the GPIOs by clearing all interrupts initially
    status = sl_gpio_driver_init();
    if (status != SL_STATUS_OK) {
      // Prints GPIO initialization fails
      DEBUGOUT("sl_gpio_driver_init, Error code: %lu\r\n", status);
      break; // breaks if error occurs
    }
    DEBUGOUT("GPIO driver initialization is successful \r\n");

#if 0
    // Configure GPIO pin 6 using pin configuration API.
    // Using this API by default GPIO mode is set as MODE 0. If any other mode is selected for any GPIO use
    // corresponding API sl_gpio_driver_set_pin_mode() is for mode setting.
    status = sl_gpio_set_configuration(sl_gpio_pin_config);
    if (status != SL_STATUS_OK) {
      // Prints if pin configuration fails
      DEBUGOUT("sl_gpio_set_configuration, Error code: %lu\r\n", status);
      break; // breaks if error occurs
    }
    DEBUGOUT("GPIO driver set pin configuration is successful \r\n");
    // Configure GPIO pin 11 using pin configuration API.
    // Using this API by default GPIO mode is set as MODE 0. If any other mode is selected for any GPIO use
    // corresponding API sl_gpio_driver_set_pin_mode() is for mode setting.
#endif

    status = sl_gpio_set_configuration(sl_gpio_pin_config1);
    if (status != SL_STATUS_OK) {
      // Prints if pin configuration fails
      DEBUGOUT("sl_gpio_set_configuration, Error code: %lu\r\n", status);
      break; // breaks if error occurs
    }
    DEBUGOUT("GPIO driver set pin configuration is successful \r\n");

    sl_si91x_gpio_enable_pad_receiver(SL_SI91X_GPIO_57_PIN);

#if 0
    // Configure pin interrupt for GPIO pin. The pin interrupt in this application is performed using external triggering from button.
    // Press button1 for triggering HP GPIO instance pin interrupt
    status = sl_gpio_driver_configure_interrupt(&sl_gpio_pin_config1.port_pin,
                                                INT_CH,
                                                (sl_gpio_interrupt_flag_t)SL_GPIO_INTERRUPT_FALL_EDGE,
                                                (sl_gpio_irq_callback_t)&gpio_pin_interrupt0_callback,
                                                AVL_INTR_NO);
    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_gpio_configure_pin_interrupt, Error code: %lu\r\n", status);
      break;
    }
#endif

    DEBUGOUT("GPIO driver interrupt configure is successful \r\n");
  } while (false);
}


#if 0
/*******************************************************************************
 *   This API handles GPIO pin interrupt 0 request
 ******************************************************************************/
static void gpio_pin_interrupt0_callback(uint32_t pin_intr)
{
  if (pin_intr == PIN_INTR_0) {
    // This is with respect to ISR context. Debugout might cause issues sometimes.
    DEBUGOUT("gpio pin interrupt0\r\n");
  }
}
#endif

/*******************************************************************************
 *   This API handles GPIO pin button event...25ms
 ******************************************************************************/
void button_event_action_process(void)
{
  uint8_t current_pin_state = 0;

  current_pin_state = sl_gpio_get_pin_input(SL_SI91X_GPIO_57_PORT, SL_SI91X_GPIO_57_PIN);

  if(current_pin_state == 0) {
      if(previous_pin_state == current_pin_state) {
          button_state.pressed_status = BUTTON_PRESSED;
          button_state.pressed_tmout++;
          if(button_state.pressed_tmout>BUTTON_LONG_TIME) {         // 80*25 = 2000ms
              previous_pin_state = 0x01;
              button_state.pressed_name = BUTTON_LONG_KEY;          // LONG BUTTON
              button_state.action_process = BUTTON_ACTION_START;
          }
      } else {
          if(previous_pin_state == 0xFF)
            previous_pin_state = current_pin_state;
      }

  }  else {
      if(button_state.pressed_status == BUTTON_PRESSED) {
          if(button_state.pressed_tmout<BUTTON_LONG_TIME) {

#if 1
              if(button_state.pressed_name != BUTTON_UPDATE_KEY) {
                button_state.action_count++;
                if(button_state.action_count>=10) {
                    button_state.pressed_name = BUTTON_UPDATE_KEY;          // SHORT BUTTON...
                    button_state.action_process = BUTTON_ACTION_START;

                    DEBUGOUT(" Button status : Short Key : %d\r\n", button_state.action_count);

                }

                button_state.action_timeout = 10;
              }
#else
              button_state.pressed_name = BUTTON_SHORT_KEY;          // SHORT BUTTON...
              button_state.action_process = BUTTON_ACTION_START;
#endif
         }
      }

      button_state.pressed_status = 0;
      button_state.pressed_tmout  = 0;
      previous_pin_state = 0xFF;
  }

//  눌린 버튼 처리
  if(button_state.action_process == BUTTON_ACTION_START) {
      button_state.action_process = 0;

      switch(button_state.pressed_name) {
        case BUTTON_SHORT_KEY:
#if 0

          DEBUGOUT(" Button status : Short Key\r\n");

          if(send_sample_count==0) {
              water_flow_out_flag = API_TRUE;
              water_flow_out_step = 0;
          }
          else if(send_sample_count==1) {
            youngwon_recipe_control_setup();
          }
          else {
              youngwon_software_verification_initial(0x07); // control board에 초기화 명령으로 값을 초기화 한다.
          }

          send_sample_count++;
          send_sample_count%=3;
#endif
          break;

        case BUTTON_LONG_KEY:
          DEBUGOUT(" Button status : Long Key\r\n");
          user_app_set_event(USER_APP_BLE_CALLBACK);                 // USER_APP_BLE_CALLBACK
          button_state.pressed_name = 0;
          break;

        case BUTTON_UPDATE_KEY:

          if(WLAN_WIFI_CONNECT_STATE()) {                // Wi-Fi 가 연결 된 경우에만 업데이트 한다.
            wifi_app_set_event(WIFI_APP_FOTA_STATE);
          }
          DEBUGOUT(" Button status : update Key\r\n");
          button_state.pressed_name = 0;
          break;
        default:
          break;
      }
  }

}
