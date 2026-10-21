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

#include <errno.h>
#include <logging/log.h>
#include "libutil.h"
#include "sensor.h"
#include "plat_thermal.h"
#include "plat_log.h"
#include "plat_util.h"
#include "plat_work.h"
#include "plat_pldm_sensor.h"

LOG_MODULE_REGISTER(plat_thermal);

/* ASIC temperature alert
 * Replaces the check_thermal_handler thread used in sb-el; plat_work runs the scan every second.
 * Each TMP432/EMC1413 serves two remote channels (two sensors), and its open/limit status
 * registers clear on read, so this scan is the only reader: it reads every chip once per round
 * and keeps a snapshot for get_error_data() and an open flag for post_tmp_read().
 */
typedef struct {
	uint8_t sensor_num;
	const char *name;
	bool open;
	uint8_t status; // TEMP_STATUS_REG snapshot
	uint8_t detail[THERMAL_ALERT_KIND_MAX]; // open / limit status register snapshot per kind
} thermal_alert_sensor;

/* Two entries per chip, in order: remote 1 then remote 2 of the same TMP432/EMC1413.
 * The scan reads the chip through the first entry's cfg and checks BIT(1) / BIT(2) of the
 * open/limit status registers for the first / second entry.
 */
static thermal_alert_sensor thermal_alert_table[] = {
	{ SENSOR_NUM_ASIC_ZORA00_TEMP_C, "ASIC_ZORA00_TEMP" },
	{ SENSOR_NUM_ASIC_ZORA01_TEMP_C, "ASIC_ZORA01_TEMP" },
	{ SENSOR_NUM_ASIC_OWL_W_TEMP_C, "ASIC_OWL_W_TEMP" },
	{ SENSOR_NUM_ASIC_OWL_E_TEMP_C, "ASIC_OWL_E_TEMP" },
	{ SENSOR_NUM_ASIC_ZORA10_SENSOR_TEMP_C, "ASIC_ZORA10_SENSOR_TEMP" },
	{ SENSOR_NUM_ASIC_ZORA11_SENSOR_TEMP_C, "ASIC_ZORA11_SENSOR_TEMP" },
	{ SENSOR_NUM_ASIC_HAMSA_CRM_TEMP_C, "ASIC_HAMSA_CRM_TEMP" },
	{ SENSOR_NUM_ASIC_HAMSA_LS_TEMP_C, "ASIC_HAMSA_LS_TEMP" },
};
BUILD_ASSERT(ARRAY_SIZE(thermal_alert_table) % 2 == 0, "thermal_alert_table must be in pairs");

#define REMOTE_1_BIT BIT(1)
#define REMOTE_2_BIT BIT(2)

// status registers of one chip
typedef struct {
	uint8_t status;
	uint8_t open;
	uint8_t high;
	uint8_t low;
} thermal_chip_status;

static thermal_alert_sensor *find_alert_sensor(uint8_t sensor_num)
{
	for (size_t i = 0; i < ARRAY_SIZE(thermal_alert_table); i++) {
		if (thermal_alert_table[i].sensor_num == sensor_num)
			return &thermal_alert_table[i];
	}
	return NULL;
}

static bool read_chip_status(const sensor_cfg *cfg, thermal_chip_status *chip)
{
	uint8_t bus = cfg->port;
	uint8_t addr = cfg->target_addr;

	if (!plat_i2c_read(bus, addr, TEMP_STATUS_REG, &chip->status, 1))
		return false;

	if ((chip->status & TEMP_STATUS_OPEN) &&
	    !plat_i2c_read(bus, addr, TEMP_OPEN_STATUS_REG, &chip->open, 1))
		return false;

	if ((chip->status & TEMP_STATUS_H_LIMIT) &&
	    !plat_i2c_read(bus, addr, TEMP_HIGH_LIMIT_STATUS_REG, &chip->high, 1))
		return false;

	if ((chip->status & TEMP_STATUS_L_LIMIT) &&
	    !plat_i2c_read(bus, addr, TEMP_LOW_LIMIT_STATUS_REG, &chip->low, 1))
		return false;

	return true;
}

static void thermal_alert_update(thermal_alert_sensor *s, uint8_t kind, uint8_t reg_val,
				 uint8_t remote_bit)
{
	bool active = (reg_val & remote_bit) != 0;

	// snapshot must be ready before error_log_event() calls get_error_data()
	if (active)
		s->detail[kind] = reg_val;

	if (!error_log_event(MAKE_ERR_CODE(TEMPERATURE_TRIGGER_CAUSE,
					   THERMAL_ALERT_PAYLOAD(kind, s->sensor_num)),
			     active ? LOG_ASSERT : LOG_DEASSERT))
		return;

	LOG_INF("%s %s %s", s->name, plat_thermal_get_kind_name(kind),
		active ? "asserted" : "deasserted");

	if (active)
		set_heartbeat_led_event_flag(true);
}

static void thermal_alert_check(thermal_alert_sensor *s, const thermal_chip_status *chip,
				uint8_t remote_bit)
{
	s->status = chip->status;
	s->open = (chip->open & remote_bit) != 0;

	thermal_alert_update(s, THERMAL_ALERT_OPEN, chip->open, remote_bit);
	thermal_alert_update(s, THERMAL_ALERT_HIGH_LIMIT, chip->high, remote_bit);
	thermal_alert_update(s, THERMAL_ALERT_LOW_LIMIT, chip->low, remote_bit);
}

int plat_thermal_alert_scan(void)
{
	// the chips are only reachable while TEMP sensor polling is enabled
	if (!is_temp_access(0))
		return 0;

	bool any_read = false;

	for (size_t i = 0; i < ARRAY_SIZE(thermal_alert_table); i += 2) {
		thermal_alert_sensor *remote_1 = &thermal_alert_table[i];
		thermal_alert_sensor *remote_2 = &thermal_alert_table[i + 1];

		sensor_cfg *cfg = get_sensor_cfg_by_sensor_id(remote_1->sensor_num);
		if (cfg == NULL) {
			LOG_ERR("Sensor 0x%02x not found", remote_1->sensor_num);
			continue;
		}

		// keep the last state on a read failure, it is not a recovery
		thermal_chip_status chip = { 0 };
		if (!read_chip_status(cfg, &chip)) {
			LOG_DBG("Failed to read temp status of bus %d addr 0x%02x", cfg->port,
				cfg->target_addr);
			continue;
		}

		any_read = true;
		thermal_alert_check(remote_1, &chip, REMOTE_1_BIT);
		thermal_alert_check(remote_2, &chip, REMOTE_2_BIT);
	}

	return any_read ? 0 : -EIO;
}

// data[0]: status(02h), data[1]: open(1Bh) / high limit(35h) / low limit(36h) status,
// data[2]: sensor num. Taken from the scan snapshot, no I2C access.
bool plat_thermal_get_error_data(uint16_t payload, uint8_t *data)
{
	CHECK_NULL_ARG_WITH_RETURN(data, false);

	uint8_t kind = THERMAL_ALERT_GET_KIND(payload);
	thermal_alert_sensor *s = find_alert_sensor(THERMAL_ALERT_GET_SENSOR(payload));
	if (s == NULL || kind == 0 || kind >= THERMAL_ALERT_KIND_MAX)
		return false;

	data[0] = s->status;
	data[1] = s->detail[kind];
	data[2] = s->sensor_num;
	return true;
}

bool plat_thermal_is_open(uint8_t sensor_num)
{
	thermal_alert_sensor *s = find_alert_sensor(sensor_num);
	return s ? s->open : false;
}

const char *plat_thermal_get_sensor_name(uint8_t sensor_num)
{
	thermal_alert_sensor *s = find_alert_sensor(sensor_num);
	return s ? s->name : "UNKNOWN_TEMP";
}

const char *plat_thermal_get_kind_name(uint8_t kind)
{
	switch (kind) {
	case THERMAL_ALERT_OPEN:
		return "open circuit";
	case THERMAL_ALERT_HIGH_LIMIT:
		return "high limit";
	case THERMAL_ALERT_LOW_LIMIT:
		return "low limit";
	default:
		return "unknown";
	}
}

uint8_t plat_thermal_get_alert_count(void)
{
	return ARRAY_SIZE(thermal_alert_table);
}

bool plat_thermal_get_alert_status(uint8_t idx, uint8_t *sensor_num, uint8_t *status)
{
	CHECK_NULL_ARG_WITH_RETURN(sensor_num, false);
	CHECK_NULL_ARG_WITH_RETURN(status, false);

	if (idx >= ARRAY_SIZE(thermal_alert_table))
		return false;

	*sensor_num = thermal_alert_table[idx].sensor_num;
	*status = thermal_alert_table[idx].status;
	return true;
}
