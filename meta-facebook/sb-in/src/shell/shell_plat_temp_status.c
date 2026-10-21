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
#include <shell/shell.h>
#include "sensor.h"
#include "hal_gpio.h"
#include "plat_gpio.h"
#include "plat_pldm_sensor.h"
#include "plat_thermal.h"
#include "shell_plat_temp.h"

/* TMP75 has no status register, its ALERT pin goes to BIC through SGPIO (active low) */
static const uint8_t tmp75_alert_sgpio[] = {
	IRQ_TMP75_1_ALERT_R_N,
	IRQ_TMP75_2_ALERT_R_N,
	IRQ_TMP75_3_ALERT_R_N,
};

/* TMP432 and EMC1413 are second sources of each other, the type is set at boot */
static const char *temp_chip_name(uint8_t sensor_num)
{
	sensor_cfg *cfg = get_sensor_cfg_by_sensor_id(sensor_num);
	if (!cfg)
		return "UNKNOWN";

	switch (cfg->type) {
	case sensor_dev_tmp431:
		return "TMP432";
	case sensor_dev_emc1413:
		return "EMC1413";
	default:
		return "UNKNOWN";
	}
}

/*
 * The status registers clear on read and plat_thermal is their only reader,
 * so this command prints the last scan snapshot instead of reading the chips.
 */
int cmd_temp_status(const struct shell *shell, size_t argc, char **argv)
{
	for (uint8_t i = 0; i < ARRAY_SIZE(tmp75_alert_sgpio); i++) {
		uint8_t num = tmp75_alert_sgpio[i];
		shell_print(shell, "%s (TMP75) : %d", sgpio_name[num], sgpio_get(num));
	}

	for (uint8_t i = 0; i < plat_thermal_get_alert_count(); i++) {
		uint8_t sensor_num = 0;
		uint8_t status = 0;

		if (!plat_thermal_get_alert_status(i, &sensor_num, &status))
			continue;

		shell_print(shell, "%s (%s) : 0x%02x", plat_thermal_get_sensor_name(sensor_num),
			    temp_chip_name(sensor_num), status);
	}

	return 0;
}
