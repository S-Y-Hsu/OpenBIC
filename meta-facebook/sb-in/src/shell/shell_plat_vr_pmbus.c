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

#include <stdlib.h>
#include "plat_hook.h"
#include "plat_pldm_sensor.h"
#include "shell_plat_vr.h"

#define VR_PMBUS_DATA_LEN 2

int cmd_vr_pmbus_read(const struct shell *shell, size_t argc, char **argv)
{
	if (!is_p3v3_pwrgd())
		return -1;

	uint8_t rail;
	if (!vr_rail_arg_get(shell, argv[1], &rail))
		return -1;

	uint8_t reg = strtoul(argv[2], NULL, 16);

	uint8_t sensor_id;
	if (!vr_rail_sensor_id_get(rail, &sensor_id)) {
		shell_error(shell, "Can't find sensor id by rail index: %x", rail);
		return -1;
	}

	uint8_t data[VR_PMBUS_DATA_LEN] = { 0 };
	if (!get_raw_data_from_sensor_id(sensor_id, reg, data, sizeof(data))) {
		shell_warn(shell, "%s pmbus read from 0x%02x fail", argv[1], reg);
		return -1;
	}

	shell_print(shell, "%s pmbus read from 0x%02x:", argv[1], reg);
	shell_hexdump(shell, data, sizeof(data));
	return 0;
}

int cmd_vr_pmbus_write(const struct shell *shell, size_t argc, char **argv)
{
	if (!is_p3v3_pwrgd())
		return -1;

	uint8_t rail;
	if (!vr_rail_arg_get(shell, argv[1], &rail))
		return -1;

	uint8_t reg = strtoul(argv[2], NULL, 16);

	uint8_t data[VR_PMBUS_DATA_LEN];
	data[0] = strtoul(argv[3], NULL, 16);
	data[1] = strtoul(argv[4], NULL, 16);

	if (!plat_set_vr_reg(rail, reg, data, sizeof(data))) {
		shell_warn(shell, "%s pmbus write to 0x%02x fail", argv[1], reg);
		return -1;
	}

	shell_print(shell, "%s pmbus write to 0x%02x: 0x%02x 0x%02x", argv[1], reg, data[0],
		    data[1]);
	return 0;
}
