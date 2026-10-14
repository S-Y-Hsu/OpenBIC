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
#include "plat_cpld.h"
#include "pldm_firmware_update.h"
#include "plat_event.h"
#include "pldm_oem.h"
#include "plat_pldm_sensor.h"
#include "pmbus.h"
#include "pldm_sensor.h"
#include "plat_thermal.h"
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

/* ===== Event-driven scan works ===== */
static bool sgpio_fault_scan_enabled = true;

void set_sgpio_fault_scan_enable_flag(bool enable)
{
	sgpio_fault_scan_enabled = enable;
}

static bool sgpio_fault_scan_allowed(void)
{
	return sgpio_fault_scan_enabled && is_update_state_idle();
}

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

	if (!sgpio_fault_scan_allowed())
		return;

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

/* SGPIO event scan
 * Every input in sgpio_event_table (plat_hook.c) shares ISR_SGPIO_EVENT. On a real transition
 * (error_log_event() returns true) an input with an event source also sends a BMC event.
 */
static void sgpio_event_scan_handler(struct k_work *work);
static K_WORK_DEFINE(sgpio_event_scan_work, sgpio_event_scan_handler);

static void sgpio_event_scan_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!sgpio_fault_scan_allowed())
		return;

	for (uint8_t i = 0; i < sgpio_event_count(); i++) {
		uint8_t sgpio_num = 0;
		uint8_t event_source = ASTRID_EVENT_SOURCE_NONE;
		bool active = false;
		if (!sgpio_event_get_state(i, &sgpio_num, &event_source, &active))
			continue;

		if (!error_log_event(MAKE_ERR_CODE(SGPIO_EVENT_TRIGGER_CAUSE, sgpio_num),
				     active ? LOG_ASSERT : LOG_DEASSERT))
			continue;

		LOG_INF("SGPIO event %s %s", sgpio_name[sgpio_num],
			active ? "asserted" : "deasserted");

		if (event_source != ASTRID_EVENT_SOURCE_NONE)
			plat_send_bmc_event(event_source, active, 0, 0);
	}
}

void plat_sgpio_event_trigger_scan(void)
{
	k_work_submit_to_queue(&plat_work_q_obj, &sgpio_event_scan_work);
}

/* VR power fault scan
 * The CPLD latches PWER_FAULT high and turns every VR off when a power fault happens, so one
 * scan of the VR_POWER_FAULT_n_REG registers on the rising edge finds all the faults. Faults are
 * never deasserted here: DC on (MODULE_PWRGD) clears them with reset_error_log_event().
 */
static void power_fault_scan_handler(struct k_work *work);
static K_WORK_DEFINE(power_fault_scan_work, power_fault_scan_handler);

static void power_fault_scan_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!sgpio_fault_scan_allowed() || sgpio_get(PWER_FAULT) != GPIO_HIGH)
		return;

	uint8_t data[POWER_FAULT_REG_NUM];
	if (!plat_read_cpld(VR_POWER_FAULT_1_REG, data, POWER_FAULT_REG_NUM)) {
		LOG_ERR("Failed to read VR power fault registers");
		return;
	}

	for (uint8_t i = 0; i < POWER_FAULT_REG_NUM; i++) {
		uint8_t cpld_offset = VR_POWER_FAULT_1_REG + i;

		for (uint8_t bit = 0; bit < 8; bit++) {
			uint8_t rail = NOT_VR_RAIL;
			uint8_t event_source = ASTRID_EVENT_SOURCE_NONE;

			if (!(data[i] & BIT(bit)) ||
			    !power_fault_get_bit(cpld_offset, bit, &rail, &event_source))
				continue;

			if (!error_log_event(MAKE_ERR_CODE(POWER_FAULT_TRIGGER_CAUSE,
							   (bit << 8) | cpld_offset),
					     LOG_ASSERT))
				continue;

			LOG_ERR("Power fault: %s (CPLD 0x%02x bit %d)",
				power_fault_get_bit_name(cpld_offset, bit), cpld_offset, bit);

			set_heartbeat_led_event_flag(true);

			if (event_source != ASTRID_EVENT_SOURCE_NONE)
				plat_send_power_fault_bmc_event(event_source, rail);
		}
	}
}

void plat_power_fault_trigger_scan(void)
{
	k_work_submit_to_queue(&plat_work_q_obj, &power_fault_scan_work);
}

/* ===== Periodic works =====
 * Each job is a delayable work that reschedules itself every interval_ms; a job may change its
 * own interval_ms to pace the next run. A work that fails max_fail times in a row is suspended
 * until reboot, so a broken board does not flood the log.
 */
#define CLK_BUF_LOSB_MASK GENMASK(2, 0) // PCA6416A_U753 P1 bit0~2

static int clk_buf_losb_sync(struct plat_work *pw)
{
	ARG_UNUSED(pw);

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

/* Heartbeat LED, same as sb-el: toggle every interval_ms, paced by the hottest ASIC sensor
 *   all <= 70 C: 500 ms, any <= 100 C: 250 ms, any > 100 C or unreadable: 50 ms
 *   all <= 70 C but a power fault happened (heartbeat_led_event): 1000 ms
 */
#define HEARTBEAT_LED_NORMAL_MS 500
#define HEARTBEAT_LED_WARM_MS 250
#define HEARTBEAT_LED_CRITICAL_MS 50
#define HEARTBEAT_LED_EVENT_MS 1000

// Same as sb-el led_set_flag: set when a fault event happens (power fault now), never cleared
static bool heartbeat_led_event;

void set_heartbeat_led_event_flag(bool flag)
{
	heartbeat_led_event = flag;
}

static const uint16_t heartbeat_led_sensors[] = {
	SENSOR_NUM_ASIC_ZORA00_TEMP_C,	      SENSOR_NUM_ASIC_ZORA01_TEMP_C,
	SENSOR_NUM_ASIC_OWL_W_TEMP_C,	      SENSOR_NUM_ASIC_OWL_E_TEMP_C,
	SENSOR_NUM_ASIC_ZORA10_SENSOR_TEMP_C, SENSOR_NUM_ASIC_ZORA11_SENSOR_TEMP_C,
	SENSOR_NUM_ASIC_HAMSA_CRM_TEMP_C,     SENSOR_NUM_ASIC_HAMSA_LS_TEMP_C,
};

static int heartbeat_led(struct plat_work *pw)
{
	static bool led_status = true;
	led_status = !led_status;
	gpio_set(LED_MMC_HEARTBEAT_R, led_status);

	uint32_t interval = HEARTBEAT_LED_NORMAL_MS;

	for (size_t i = 0; i < ARRAY_SIZE(heartbeat_led_sensors); i++) {
		int reading = 0; // millidegree C
		uint8_t op_state = PLDM_SENSOR_STATUSUNKOWN;

		if (pldm_sensor_get_reading_from_cache(heartbeat_led_sensors[i], &reading,
						       &op_state) != PLDM_SUCCESS) {
			interval = HEARTBEAT_LED_CRITICAL_MS;
			continue;
		}

		uint32_t candidate = (reading <= 70000)	 ? HEARTBEAT_LED_NORMAL_MS :
				     (reading <= 100000) ? HEARTBEAT_LED_WARM_MS :
							   HEARTBEAT_LED_CRITICAL_MS;
		if (candidate < interval)
			interval = candidate;
	}

	if (interval == HEARTBEAT_LED_NORMAL_MS && heartbeat_led_event)
		interval = HEARTBEAT_LED_EVENT_MS;

	pw->interval_ms = interval;
	return 0;
}

// ASIC temperature open circuit / limit alert, see plat_thermal.c
static int thermal_alert(struct plat_work *pw)
{
	ARG_UNUSED(pw);

	return plat_thermal_alert_scan();
}

static struct plat_work plat_work_list[] = {
	{ .name = "clk_buf_losb_sync", .fn = clk_buf_losb_sync, .interval_ms = 100, .max_fail = 10 },
	{ .name = "heartbeat_led", .fn = heartbeat_led, .interval_ms = 250, .max_fail = 10 },
	{ .name = "thermal_alert", .fn = thermal_alert, .interval_ms = 1000, .max_fail = 10 },
};

static void plat_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct plat_work *pw = CONTAINER_OF(dwork, struct plat_work, work);

	if (pw->fn(pw) < 0) {
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
