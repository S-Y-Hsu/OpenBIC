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

#include <string.h>
#include <shell/shell.h>
#include "plat_hook.h"
#include "shell_plat_vr.h"

/* Average of the last PWR_HIST_SIZE power readings of each UBC and VR rail.
 * The values come from ubc_rail_avg_pwr_get() and vr_rail_avg_pwr_get().
 */

static void avg_pwr_print(const struct shell *shell, const uint8_t *name, int32_t avg_mw)
{
	if (avg_mw == PWR_AVG_NONE) {
		shell_print(shell, "%-50s avg: N/A", name);
		return;
	}

	// the hooks clamp negative readings to 0, so avg_mw is never negative
	shell_print(shell, "%-50s avg: %d.%03d W", name, avg_mw / 1000, avg_mw % 1000);
}

static void ubc_avg_pwr_print(const struct shell *shell, uint8_t rail)
{
	uint8_t *name = NULL;
	int32_t avg_mw = PWR_AVG_NONE;

	if (!ubc_rail_name_get(rail, &name) || !ubc_rail_avg_pwr_get(rail, &avg_mw)) {
		shell_error(shell, "Can't get average power by UBC index: %x", rail);
		return;
	}

	avg_pwr_print(shell, name, avg_mw);
}

static void vr_avg_pwr_print(const struct shell *shell, uint8_t rail)
{
	uint8_t *name = NULL;
	int32_t avg_mw = PWR_AVG_NONE;

	if (!vr_rail_name_get(rail, &name) || !vr_rail_avg_pwr_get(rail, &avg_mw)) {
		shell_error(shell, "Can't get average power by rail index: %x", rail);
		return;
	}

	avg_pwr_print(shell, name, avg_mw);
}

static int cmd_average_power_get(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(shell, "average_power get all|<ubc>|<voltage-rail>");
		return -1;
	}

	if (!strcmp(argv[1], "all")) {
		for (int i = 0; i < UBC_RAIL_E_MAX; i++) {
			ubc_avg_pwr_print(shell, (uint8_t)i);
		}
		for (int i = 0; i < vr_rail_count(); i++) {
			vr_avg_pwr_print(shell, (uint8_t)i);
		}
		return 0;
	}

	uint8_t rail;
	if (ubc_rail_enum_get((uint8_t *)argv[1], &rail)) {
		ubc_avg_pwr_print(shell, rail);
		return 0;
	}

	if (!vr_rail_arg_get(shell, argv[1], &rail))
		return -1;

	vr_avg_pwr_print(shell, rail);
	return 0;
}

// completion list: UBC names, then VR rail names, then "all"
static void avg_pwr_name_entry_get(size_t idx, struct shell_static_entry *entry)
{
	uint8_t *name = NULL;

	if (idx < UBC_RAIL_E_MAX) {
		ubc_rail_name_get((uint8_t)idx, &name);
	} else if (idx < UBC_RAIL_E_MAX + vr_rail_count()) {
		vr_rail_name_get((uint8_t)(idx - UBC_RAIL_E_MAX), &name);
	} else if (idx == UBC_RAIL_E_MAX + vr_rail_count()) {
		name = (uint8_t *)"all";
	}

	entry->syntax = (name) ? (const char *)name : NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}

SHELL_DYNAMIC_CMD_CREATE(avg_pwr_name_dcmd, avg_pwr_name_entry_get);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_average_power_cmds,
			       SHELL_CMD(get, &avg_pwr_name_dcmd, "get all|<ubc>|<voltage-rail>",
					 cmd_average_power_get),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(average_power, &sub_average_power_cmds, "average power commands", NULL);
