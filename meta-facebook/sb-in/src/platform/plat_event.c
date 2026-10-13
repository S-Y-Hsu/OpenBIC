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
#include "plat_hook.h"
#include "pmbus.h"

LOG_MODULE_REGISTER(plat_event);

/* BMC event
 * send_event_log_to_bmc() waits for the BMC response (up to ~18s when the BMC does not answer),
 * so callers only queue the SEL here and the system work queue sends it. One SEL per run, then
 * the work resubmits itself, so other system work items can run between SELs.
 */
#define BMC_EVENT_QUEUE_DEPTH 64 // one PMBus VR power fault alone takes VR_STAUS_E_MAX SELs

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

/* Power fault BMC event, assert only (power faults are cleared by DC on, not deasserted).
 * Not a PMBus VR: one SEL. PMBus VR: one SEL per VR status, same format as sb-el:
 * event_data_2 is the PMBus status register (STATUS_WORD: its high byte), event_data_3 is the
 * status low byte; a status that cannot be read is sent as 0xFFFF.
 */
void plat_send_power_fault_bmc_event(uint8_t event_source, uint8_t rail)
{
	if (rail == NOT_VR_RAIL) {
		plat_send_bmc_event(event_source, true, 0, 0);
		return;
	}

	for (uint8_t i = 0; i < VR_STAUS_E_MAX; i++) {
		uint8_t pmbus_reg = 0;
		uint16_t vr_status = 0xFFFF;

		if (!vr_status_pmbus_reg_get(i, &pmbus_reg))
			continue;

		if (!plat_get_vr_status(rail, i, &vr_status)) {
			LOG_ERR("Failed to get VR rail %d status 0x%02x for BMC event", rail,
				pmbus_reg);
			vr_status = 0xFFFF;
		}

		uint8_t data2 =
			(pmbus_reg == PMBUS_STATUS_WORD) ? (uint8_t)(vr_status >> 8) : pmbus_reg;
		plat_send_bmc_event(event_source, true, data2, (uint8_t)(vr_status & 0xFF));
	}
}
