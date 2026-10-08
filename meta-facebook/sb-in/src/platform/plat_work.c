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
 * Replaces the quick_sensor_poll_handler thread used in sb-el. Two kinds of work run on it:
 * - event-driven scan works, submitted by a shared SGPIO ISR
 * - periodic works in plat_work_list, each rescheduling itself
 */
#define PLAT_WORK_Q_STACK_SIZE 2048

K_THREAD_STACK_DEFINE(plat_work_q_stack, PLAT_WORK_Q_STACK_SIZE);
static struct k_work_q plat_work_q_obj;

/* ===== Event-driven scan works =====
 * Each group of SGPIO inputs shares one ISR, which only submits the group's scan work; the work
 * rescans every pin of the group. That covers simultaneous edges (the SGPIO IRQ dispatches only
 * the first changed pin per interrupt), and an edge arriving mid-scan just queues one more scan.
 * Every pin is reported to error_log_event() as it is; its return value tells whether the
 * state really changed, so the scans keep no state of their own.
 * Not scheduled, so they do not take a slot in plat_work_list. Submitting before
 * plat_init_platform_queue() starts the queue returns -ENODEV; plat_sgpio_init() scans once.
 */

/* VR SMBALERT# scan */
#define HAMSA_VRHOT_VR_INDEX VR_INDEX_E_13 // PU626: MAX_EW1_VDD + HAMSA_VDD
#define PMBUS_STATUS_WORD_TEMPERATURE BIT(2)

static void vr_smbalert_scan_handler(struct k_work *work);
static K_WORK_DEFINE(vr_smbalert_scan_work, vr_smbalert_scan_handler);

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
		if (!error_log_event(MAKE_ERR_CODE(VR_SMB_ALERT_TRIGGER_CAUSE, vr),
				     active ? LOG_ASSERT : LOG_DEASSERT))
			continue;

		LOG_INF("VR index %d SMBALERT %s", vr, active ? "asserted" : "deasserted");

		if (active && vr == HAMSA_VRHOT_VR_INDEX)
			hamsa_vrhot_check();
	}
}

void plat_vr_smbalert_trigger_scan(void)
{
	k_work_submit_to_queue(&plat_work_q_obj, &vr_smbalert_scan_work);
}

/* ASIC CATTRIP scan
 * The CPLD latches each CATTRIP onto its SGPIO input (active low, same as CPLD reg 0x27).
 * Logging is the only action, so the return value of error_log_event() is not needed.
 */
static void asic_cattrip_scan_handler(struct k_work *work);
static K_WORK_DEFINE(asic_cattrip_scan_work, asic_cattrip_scan_handler);

static void asic_cattrip_scan_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	for (uint8_t idx = 0; idx < ASIC_CATTRIP_MAX; idx++) {
		uint8_t sgpio_num = 0;
		if (!asic_cattrip_get_sgpio(idx, &sgpio_num))
			continue;

		bool active = (sgpio_get(sgpio_num) == GPIO_LOW);
		error_log_event(MAKE_ERR_CODE(ASIC_CATTRIP_TRIGGER_CAUSE, idx),
				active ? LOG_ASSERT : LOG_DEASSERT);
	}
}

void plat_asic_cattrip_trigger_scan(void)
{
	k_work_submit_to_queue(&plat_work_q_obj, &asic_cattrip_scan_work);
}

/* ===== Periodic works =====
 * Each job is a delayable work that reschedules itself every interval_ms. A work that fails
 * max_fail times in a row is suspended until reboot, so a broken board does not flood the log.
 */
#define CLK_BUF_LOSB_MASK GENMASK(2, 0) // PCA6416A_U753 P1 bit0~2

static int clk_buf_losb_sync(void)
{
	static bool synced = false;
	static uint8_t last_val = 0;
	uint8_t val = 0;

	if (!pca6416a_i2c_read(PCA6416A_U753, PCA6416A_INPUT_PORT_1, &val, 1))
		return -EIO;

	val &= CLK_BUF_LOSB_MASK;
	if (synced && val == last_val)
		return 0;

	sgpio_set(wMMC_SGPIO_BUFF0_100M_LOSB_N, (val & BIT(0)) ? GPIO_HIGH : GPIO_LOW);
	sgpio_set(wMMC_SGPIO_BUFF1_100M_LOSB_N, (val & BIT(1)) ? GPIO_HIGH : GPIO_LOW);
	sgpio_set(wMMC_SGPIO_BUFF2_100M_LOSB_N, (val & BIT(2)) ? GPIO_HIGH : GPIO_LOW);
	sgpio_set(wMMC_SGPIO_BUFF3_100M_LOSB_N,
		  gpio_get(BUFF3_100M_LOSB_MMC) == GPIO_HIGH ? GPIO_HIGH : GPIO_LOW);

	last_val = val;
	synced = true;
	return 0;
}

static struct plat_work plat_work_list[] = {
	{ .name = "clk_buf_losb_sync", .fn = clk_buf_losb_sync, .interval_ms = 100, .max_fail = 10 },
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
