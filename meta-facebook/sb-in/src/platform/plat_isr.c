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
#include "plat_log.h"
#include "plat_work.h"
#include "plat_power_seq.h"

LOG_MODULE_REGISTER(plat_isr);

void ISR_SGPIO_PWR_EN()
{
	LOG_INF("PWR_EN = %d", sgpio_get(PWR_EN));

	if (sgpio_get(PWR_EN) == GPIO_HIGH) {
		error_log_event(MAKE_ERR_CODE(DC_ON_TRIGGER_CAUSE, 0), LOG_ASSERT);
	} else {
		error_log_event(MAKE_ERR_CODE(DC_ON_TRIGGER_CAUSE, 0), LOG_DEASSERT);
		plat_clear_power_seq_fault();
	}
}

// Shared by every VR SMBALERT# SGPIO input and PWRGD_P3V3_R (the alert enable) - the scan work
// finds which one(s) changed.
void ISR_SGPIO_VR_SMBALERT()
{
	plat_vr_smbalert_trigger_scan();
}

// Shared by every input in sgpio_event_table - the scan work reports all of them.
void ISR_SGPIO_EVENT()
{
	plat_sgpio_event_trigger_scan();
}

// PWER_FAULT is latched high by the CPLD on any VR power fault (rising edge only)
void ISR_SGPIO_PWR_FAULT()
{
	plat_power_fault_trigger_scan();
}

void ISR_SGPIO_MODULE_PWRGD()
{
	LOG_INF("MODULE_PWRGD = %d", sgpio_get(MODULE_PWRGD));

	if (sgpio_get(MODULE_PWRGD) == GPIO_HIGH) {
		// power faults are latched until DC on, clear them so the next fault is logged
		reset_error_log_event(POWER_FAULT_TRIGGER_CAUSE);
	}
}

void ISR_SGPIO_PWRSEQ_TO_FAULT()
{
	LOG_INF("PWRSEQ_TO_FAULT = %d", sgpio_get(PWRSEQ_TO_FAULT));

	if (sgpio_get(PWRSEQ_TO_FAULT) == GPIO_HIGH) {
		plat_power_seq_fault_handler();
	}
}
