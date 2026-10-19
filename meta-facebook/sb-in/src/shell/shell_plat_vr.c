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
#include "plat_hook.h"
#include "shell_plat_vr.h"

bool vr_rail_arg_get(const struct shell *shell, char *arg, uint8_t *rail)
{
	if (vr_rail_enum_get((uint8_t *)arg, rail) == false) {
		shell_error(shell, "Invalid rail name: %s", arg);
		return false;
	}

	return true;
}

/**************************************************************************************/
/********************************* name completion ************************************/
/**************************************************************************************/
static void vr_status_name_entry_get(size_t idx, struct shell_static_entry *entry)
{
	uint8_t *name = NULL;
	vr_status_name_get((uint8_t)idx, &name);

	if (idx == VR_STAUS_E_MAX)
		name = (uint8_t *)"all";

	entry->syntax = (name) ? (const char *)name : NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}

SHELL_DYNAMIC_CMD_CREATE(vr_status_name_dcmd, vr_status_name_entry_get);

// rail names, optionally followed by "all", with an optional next-level completion
static void vr_rail_entry_fill(size_t idx, struct shell_static_entry *entry, bool with_all,
			       const struct shell_cmd_entry *subcmd)
{
	uint8_t *name = NULL;
	vr_rail_name_get((uint8_t)idx, &name);

	if (with_all && (idx == vr_rail_count()))
		name = (uint8_t *)"all";

	entry->syntax = (name) ? (const char *)name : NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = subcmd;
}

static void vr_rail_entry_get(size_t idx, struct shell_static_entry *entry)
{
	vr_rail_entry_fill(idx, entry, false, NULL);
}

static void vr_rail_all_entry_get(size_t idx, struct shell_static_entry *entry)
{
	vr_rail_entry_fill(idx, entry, true, NULL);
}

static void vr_rail_all_status_entry_get(size_t idx, struct shell_static_entry *entry)
{
	vr_rail_entry_fill(idx, entry, true, &vr_status_name_dcmd);
}

SHELL_DYNAMIC_CMD_CREATE(vr_rail_dcmd, vr_rail_entry_get);
SHELL_DYNAMIC_CMD_CREATE(vr_rail_all_dcmd, vr_rail_all_entry_get);
SHELL_DYNAMIC_CMD_CREATE(vr_rail_all_status_dcmd, vr_rail_all_status_entry_get);

/* level 2 */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_vr_pmbus_cmds,
			       SHELL_CMD_ARG(read, &vr_rail_dcmd, "read <voltage-rail> <reg>",
					     cmd_vr_pmbus_read, 3, 0),
			       SHELL_CMD_ARG(write, &vr_rail_dcmd,
					     "write <voltage-rail> <reg> <data0> <data1>",
					     cmd_vr_pmbus_write, 5, 0),
			       SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_vr_status_cmds,
			       SHELL_CMD(get, &vr_rail_all_status_dcmd,
					 "get all|<voltage-rail> all|<which-status>",
					 cmd_vr_status_get),
			       SHELL_CMD(clear, &vr_rail_all_dcmd, "clear all|<voltage-rail>",
					 cmd_vr_status_clear),
			       SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_vr_peak_cmds,
			       SHELL_CMD(get, &vr_rail_all_dcmd, "get all|<voltage-rail>",
					 cmd_vr_peak_get),
			       SHELL_CMD(clear, &vr_rail_all_dcmd, "clear all|<voltage-rail>",
					 cmd_vr_peak_clear),
			       SHELL_SUBCMD_SET_END);

/* level 1 */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_vr_cmds, SHELL_CMD(pmbus, &sub_vr_pmbus_cmds, "VR rail pmbus read/write", NULL),
	SHELL_CMD(status, &sub_vr_status_cmds, "VR fault status get/clear", NULL),
	SHELL_CMD(peak, &sub_vr_peak_cmds, "VR voltage peak get/clear", NULL),
	SHELL_SUBCMD_SET_END);

/* Root of command vr */
SHELL_CMD_REGISTER(vr, &sub_vr_cmds, "VR rail commands", NULL);
