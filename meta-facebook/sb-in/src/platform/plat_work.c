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

#include "plat_work.h"
#include "plat_gpio.h"
#include "plat_ioexp.h"
#include "plat_hook.h"
#include "plat_log.h"
#include "plat_pldm_sensor.h"
#include "pmbus.h"
#include <errno.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(plat_work);

/* platform work queue
 * Replaces the quick_sensor_poll_handler thread used in sb-el: each periodic job is a
 * delayable work on this queue instead of sharing one polling loop. A work that fails
 * max_fail times in a row is suspended until reboot, so a broken board does not flood the log.
 */
#define PLAT_WORK_Q_STACK_SIZE 2048
#define SGPIO_BUFF_LOSB_MASK GENMASK(2, 0) // PCA6416A_U753 P1 bit0~2

K_THREAD_STACK_DEFINE(plat_work_q_stack, PLAT_WORK_Q_STACK_SIZE);
static struct k_work_q plat_work_q_obj;

static int sgpio_buff_losb_poll(void)
{
	static bool synced = false;
	static uint8_t last_val = 0;
	uint8_t val = 0;

	if (!pca6416a_i2c_read(PCA6416A_U753, PCA6416A_INPUT_PORT_1, &val, 1))
		return -EIO;

	val &= SGPIO_BUFF_LOSB_MASK;
	if (synced && val == last_val)
		return 0;

	sgpio_set(wMMC_SGPIO_BUFF0_100M_LOSB_N, (val & BIT(0)) ? GPIO_HIGH : GPIO_LOW);
	sgpio_set(wMMC_SGPIO_BUFF1_100M_LOSB_N, (val & BIT(1)) ? GPIO_HIGH : GPIO_LOW);
	sgpio_set(wMMC_SGPIO_BUFF2_100M_LOSB_N, (val & BIT(2)) ? GPIO_HIGH : GPIO_LOW);

	last_val = val;
	synced = true;
	return 0;
}

/* VR SMBALERT# scan
 * Every VR SMBALERT# SGPIO input shares ISR_SGPIO_VR_SMBALERT, which only submits this work; the
 * work rescans all of them. That covers simultaneous edges (the SGPIO IRQ dispatches only the
 * first changed pin per interrupt), and an edge arriving mid-scan just queues one more scan.
 * Event driven, so it does not take a slot in plat_work_list.
 */
#define HAMSA_VRHOT_VR_INDEX VR_INDEX_E_13 // PU626: MAX_EW1_VDD + HAMSA_VDD
#define PMBUS_STATUS_WORD_TEMPERATURE BIT(2)

BUILD_ASSERT(VR_INDEX_MAX <= 16, "vr_smbalert_active_map is 16 bits");

static void vr_smbalert_scan_handler(struct k_work *work);
static K_WORK_DEFINE(vr_smbalert_scan_work, vr_smbalert_scan_handler);
static uint16_t vr_smbalert_active_map; // bit n: VR_INDEX_E n is logged as asserted

// HAMSA's VR_HOT has no direct pin to the CPLD, so raise it when either page of the IC reports
// over-temperature. Assert only - nothing releases it yet.
static void hamsa_vrhot_check(void)
{
	const uint8_t *rails;
	uint8_t rail_count;
	if (!vr_index_get_rails(HAMSA_VRHOT_VR_INDEX, &rails, &rail_count))
		return;

	for (uint8_t i = 0; i < rail_count; i++) {
		uint8_t sensor_id = 0;
		uint8_t status_word[2] = { 0 };

		if (!vr_rail_sensor_id_get(rails[i], &sensor_id) ||
		    !get_raw_data_from_sensor_id(sensor_id, PMBUS_STATUS_WORD, status_word, 2)) {
			LOG_ERR("Failed to read STATUS_WORD of VR rail %d for HAMSA VRHOT",
				rails[i]);
			continue;
		}

		if (status_word[0] & PMBUS_STATUS_WORD_TEMPERATURE) {
			LOG_WRN("VR rail %d over-temperature, assert HAMSA VRHOT", rails[i]);
			sgpio_set(wMMC_SGPIO_HAMSA_VRHOT, GPIO_HIGH);
			return;
		}
	}
}

// SMBALERT# is only meaningful once PWRGD_P3V3_R is up. That pin shares
// ISR_SGPIO_VR_SMBALERT, so it rising/falling rescans and asserts/clears the alerts.
static bool vr_smbalert_enabled(void)
{
	return sgpio_get(PWRGD_P3V3_R) == GPIO_HIGH;
}

static void vr_smbalert_scan_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	bool enabled = vr_smbalert_enabled();

	for (uint8_t vr = 0; vr < VR_INDEX_MAX; vr++) {
		uint8_t sgpio_num = 0;
		if (!vr_index_get_smbalert_sgpio(vr, &sgpio_num))
			continue;

		bool active = enabled && (sgpio_get(sgpio_num) == GPIO_LOW);
		if (active == !!(vr_smbalert_active_map & BIT(vr)))
			continue;

		WRITE_BIT(vr_smbalert_active_map, vr, active);

		if (active && vr == HAMSA_VRHOT_VR_INDEX)
			hamsa_vrhot_check();

		LOG_INF("VR index %d SMBALERT %s", vr, active ? "asserted" : "deasserted");
		error_log_event(MAKE_ERR_CODE(VR_SMB_ALERT_TRIGGER_CAUSE, vr),
				active ? LOG_ASSERT : LOG_DEASSERT);
	}
}

void plat_vr_smbalert_trigger_scan(void)
{
	// -ENODEV before plat_init_platform_queue() starts the queue; plat_sgpio_init() scans once
	k_work_submit_to_queue(&plat_work_q_obj, &vr_smbalert_scan_work);
}

static struct plat_work plat_work_list[] = {
	{ .name = "sgpio_buff_losb",
	  .fn = sgpio_buff_losb_poll,
	  .interval_ms = 100,
	  .max_fail = 10 },
};

static void plat_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct plat_work *pw = CONTAINER_OF(dwork, struct plat_work, work);

	if (pw->fn() < 0) {
		if (++pw->fail_cnt >= pw->max_fail) {
			pw->suspended = true;
			LOG_ERR("plat work %s suspended after %u consecutive failures", pw->name,
				pw->fail_cnt);
			return;
		}
	} else {
		pw->fail_cnt = 0;
	}

	k_work_schedule_for_queue(&plat_work_q_obj, &pw->work, K_MSEC(pw->interval_ms));
}

const struct plat_work *plat_get_work_list(size_t *count)
{
	*count = ARRAY_SIZE(plat_work_list);
	return plat_work_list;
}

void plat_init_platform_queue(void)
{
	k_work_queue_start(&plat_work_q_obj, plat_work_q_stack,
			   K_THREAD_STACK_SIZEOF(plat_work_q_stack), CONFIG_MAIN_THREAD_PRIORITY,
			   NULL);
	k_thread_name_set(&plat_work_q_obj.thread, "plat_work_q");

	for (size_t i = 0; i < ARRAY_SIZE(plat_work_list); i++) {
		k_work_init_delayable(&plat_work_list[i].work, plat_work_handler);
		k_work_schedule_for_queue(&plat_work_q_obj, &plat_work_list[i].work, K_NO_WAIT);
	}
}
