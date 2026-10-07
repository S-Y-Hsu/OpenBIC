/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <zephyr.h>
#include <stdio.h>
#include <stdlib.h>
#include <logging/log.h>

#include "plat_gpio.h"
#include "plat_cpld.h"
#include "plat_kernel_obj.h"
#include "plat_event.h"
#include "plat_log.h"
#include "plat_work.h"
#include "plat_power_seq.h"

LOG_MODULE_REGISTER(plat_isr);

void ISR_GPIO_ALL_VR_PM_ALERT_R_N()
{
	if (gpio_get(ALL_VR_PM_ALERT_R_N) == GPIO_LOW) {
		plat_trigger_cpld_polling();
	}
}

void ISR_SGPIO_PWR_EN()
{
	LOG_INF("PWR_EN = %d", sgpio_get(PWR_EN));

	if (sgpio_get(PWR_EN) == GPIO_HIGH) {
		plat_record_pwr_en_rise_time();
		plat_set_dc_on_log(LOG_ASSERT);
	} else {
		plat_set_dc_on_log(LOG_DEASSERT);
		plat_clear_power_seq_fault();
	}
}

// Shared by every VR SMBALERT# SGPIO input and PWRGD_P3V3_R (the alert enable) - the scan work
// finds which one(s) changed.
void ISR_SGPIO_VR_SMBALERT()
{
	plat_vr_smbalert_trigger_scan();
}

void ISR_SGPIO_MODULE_PWRGD()
{
	LOG_INF("MODULE_PWRGD = %d", sgpio_get(MODULE_PWRGD));

	if (sgpio_get(MODULE_PWRGD) == GPIO_HIGH) {
		// when dc on clear cpld polling alert status
		reset_error_log_states(CPLD_UNEXPECTED_VAL_TRIGGER_CAUSE);
	}
}

// LEAK1_DETECT_ALERT_CPLD_N is active-low. Also called once by plat_sgpio_init().
void ISR_SGPIO_LEAK1_DETECT()
{
	LOG_INF("LEAK1_DETECT_ALERT_CPLD_N = %d", sgpio_get(LEAK1_DETECT_ALERT_CPLD_N));

	error_log_event(MAKE_ERR_CODE(LEAK_DETECT_TRIGGER_CAUSE, 0),
			(sgpio_get(LEAK1_DETECT_ALERT_CPLD_N) == GPIO_LOW) ? LOG_ASSERT :
									     LOG_DEASSERT);
}

void ISR_SGPIO_PWRSEQ_TO_FAULT()
{
	LOG_INF("PWRSEQ_TO_FAULT = %d", sgpio_get(PWRSEQ_TO_FAULT));

	if (sgpio_get(PWRSEQ_TO_FAULT) == GPIO_HIGH) {
		plat_power_seq_fault_handler();
	}
}
