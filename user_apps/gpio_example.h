/***************************************************************************/ /**
 * @file gpio_example.h
 * @brief GPIO example
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

#ifndef GPIO_EXAMPLE_H_
#define GPIO_EXAMPLE_H_

#include <stdint.h>

// -----------------------------------------------------------------------------
// Prototypes
/*****************************************************************************
 * GPIO example initialization function. It initializes pin mode,
 * direction and configures the HP pin interrupt
 * @param none
 * @return none
 ******************************************************************************/
#define BUTTON_ACTION_START   1
#define BUTTON_ACTION_STOP    0


#define BUTTON_SHORT_KEY    1
#define BUTTON_LONG_KEY     2
#define BUTTON_UPDATE_KEY   3

typedef struct {
  int8_t  pressed_status;
  int8_t  pressed_name;
  int8_t  action_process;
  int8_t  action_count;

  int8_t  action_timeout;
  int16_t pressed_tmout;
}button_state_t;
extern button_state_t button_state;

void button_gpio_init(void);

void button_event_action_process(void);

/***************************************************************************/ /**
 * Function will run continuously in while loop and led toggles for every 1sec
 * @param none
 * @return none
 ******************************************************************************/
void gpio_example_process_action(void);

#endif /* GPIO_EXAMPLE_H_ */
