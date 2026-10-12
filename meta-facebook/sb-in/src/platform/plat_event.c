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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr.h>
#include "plat_event.h"
#include "plat_log.h"
#include "pldm_oem.h"

LOG_MODULE_REGISTER(plat_event);

/* BMC event
 * send_event_log_to_bmc() waits for the BMC response (up to ~18s when the BMC does not answer),
 * so callers only queue the SEL here and the system work queue sends it. One SEL per run, then
 * the work resubmits itself, so other system work items can run between SELs.
 */
#define BMC_EVENT_QUEUE_DEPTH 8

K_MSGQ_DEFINE(bmc_event_msgq, sizeof(struct pldm_addsel_data), BMC_EVENT_QUEUE_DEPTH, 1);

static void bmc_event_send_handler(struct k_work *work);
static K_WORK_DEFINE(bmc_event_send_work, bmc_event_send_handler);

static void bmc_event_send_handler(struct k_work *work)
{
	struct pldm_addsel_data sel_msg;

	if (k_msgq_get(&bmc_event_msgq, &sel_msg, K_NO_WAIT))
		return;

	if (send_event_log_to_bmc(sel_msg) != PLDM_SUCCESS) {
		LOG_ERR("Failed to send BMC event 0x%02x %s, data 0x%02x 0x%02x",
			sel_msg.event_data_1, sel_msg.assert_type ? "assert" : "deassert",
			sel_msg.event_data_2, sel_msg.event_data_3);
	} else {
		LOG_INF("Send BMC event 0x%02x %s, data 0x%02x 0x%02x", sel_msg.event_data_1,
			sel_msg.assert_type ? "assert" : "deassert", sel_msg.event_data_2,
			sel_msg.event_data_3);
	}

	if (k_msgq_num_used_get(&bmc_event_msgq))
		k_work_submit(work);
}

// Queue one ASTRID_FAULT SEL, event_data_1 is the astrid_event_source. Never blocks; ISR safe.
void plat_send_bmc_event(uint8_t event_source, bool is_assert, uint8_t data2, uint8_t data3)
{
	struct pldm_addsel_data sel_msg = {
		.event_type = ASTRID_FAULT,
		.assert_type = is_assert ? LOG_ASSERT : LOG_DEASSERT,
		.event_data_1 = event_source,
		.event_data_2 = data2,
		.event_data_3 = data3,
	};

	if (k_msgq_put(&bmc_event_msgq, &sel_msg, K_NO_WAIT)) {
		LOG_ERR("BMC event queue full, drop event 0x%02x %s", event_source,
			is_assert ? "assert" : "deassert");
		return;
	}

	k_work_submit(&bmc_event_send_work);
}
