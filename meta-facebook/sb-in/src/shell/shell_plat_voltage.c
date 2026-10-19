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

#include <shell/shell.h>
#include <stdlib.h>
#include <string.h>
#include "plat_hook.h"
#include "plat_pldm_sensor.h"

#define VR_PMBUS_DATA_LEN 2

static int cmd_voltage_pmbus_read(const struct shell *shell, size_t argc, char **argv)
{
	if (!is_p3v3_pwrgd())
		return -1;

	uint8_t rail;
	if (vr_rail_enum_get(argv[1], &rail) == false) {
		shell_error(shell, "Invalid rail name: %s", argv[1]);
		return -1;
	}

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

static int cmd_voltage_pmbus_write(const struct shell *shell, size_t argc, char **argv)
{
	if (!is_p3v3_pwrgd())
		return -1;

	uint8_t rail;
	if (vr_rail_enum_get(argv[1], &rail) == false) {
		shell_error(shell, "Invalid rail name: %s", argv[1]);
		return -1;
	}

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

static void voltage_rname_get(size_t idx, struct shell_static_entry *entry)
{
	uint8_t *name = NULL;
	vr_rail_name_get((uint8_t)idx, &name);

	entry->syntax = (name) ? (const char *)name : NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}

SHELL_DYNAMIC_CMD_CREATE(vr_rname_for_voltage, voltage_rname_get);

/* level 2 */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_voltage_pmbus_cmds,
			       SHELL_CMD_ARG(read, &vr_rname_for_voltage,
					     "pmbus read <voltage-rail> <reg>",
					     cmd_voltage_pmbus_read, 3, 0),
			       SHELL_CMD_ARG(write, &vr_rname_for_voltage,
					     "pmbus write <voltage-rail> <reg> <data0> <data1>",
					     cmd_voltage_pmbus_write, 5, 0),
			       SHELL_SUBCMD_SET_END);

/* level 1 */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_voltage_cmds,
			       SHELL_CMD(pmbus, &sub_voltage_pmbus_cmds,
					 "VR rail pmbus read/write commands", NULL),
			       SHELL_SUBCMD_SET_END);

/* Root of command voltage */
SHELL_CMD_REGISTER(voltage, &sub_voltage_cmds, "voltage commands", NULL);
