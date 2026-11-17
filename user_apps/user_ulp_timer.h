/***************************************************************************/ /**
* @file ulp_timer_example.h
* @brief ULP-timers example
*******************************************************************************
* # License
* <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
*******************************************************************************
*
* The licenser of this software is Silicon Laboratories Inc. Your use of this
* software is governed by the terms of Silicon Labs Master Software License
* Agreement (MSLA) available at
* www.silabs.com/about-us/legal/master-software-license-agreement. This
* software is distributed to you in Source Code format and is governed by the
* sections of the MSLA applicable to Source Code.
*
******************************************************************************/

#ifndef USER_ULP_TIMER_H_
#define USER_ULP_TIMER_H_


// -----------------------------------------------------------------------------
// Macros
#define ALARM_EXAMPLE   DISABLE ///< To enable alarm trigger
#define SEC_INTR        DISABLE ///< To enable one second trigger
#define MILLI_SEC_INTR  DISABLE ///< To enable one millisecond trigger
#define TIME_CONVERSION DISABLE ///< To enable time conversion

// -----------------------------------------------------------------------------




// Prototypes
/***************************************************************************/ /**
* Ulp timer example initialization function
* Clock and Timer Configurations
* At the configured timeout of timer, on board LED will toggle
*
* @param none
* @return none
******************************************************************************/
void ulp_timer_init(void);

void ulp_timer_event_action_process(void);

void calendar_initialization(void);
void calendar_write_datetime(user_calendar_t *p);
void calendar_read_datetime(user_calendar_t *p);

void user_app_calendar_init(void);

void over_the_pass_day(void);


#endif /* SL_SI91X_ULP_TIMER_EXAMPLE_H_ */
