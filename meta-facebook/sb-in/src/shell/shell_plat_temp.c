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
#include "plat_temp_threshold.h"
#include "shell_plat_temp.h"

/**************************************************************************************/
/********************************* name completion ************************************/
/**************************************************************************************/
static void temp_threshold_name_entry_get(size_t idx, struct shell_static_entry *entry)
{
	entry->syntax = NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;

	if (idx == 0)
		entry->syntax = "all";
	else if (idx <= TEMP_THRESHOLD_COUNT)
		entry->syntax = plat_temp_threshold_get_name(idx - 1);
}

SHELL_DYNAMIC_CMD_CREATE(temp_threshold_name_dcmd, temp_threshold_name_entry_get);
SHELL_DYNAMIC_CMD_CREATE(temp_thermaltrip_name_dcmd, temp_thermaltrip_name_entry_get);

/* level 2 */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_temp_threshold_cmds,
			       SHELL_CMD_ARG(get, &temp_threshold_name_dcmd, "get all|<name>",
					     cmd_temp_threshold_get, 2, 0),
			       SHELL_CMD_ARG(set, &temp_threshold_name_dcmd,
					     "set all|<name> <millidegree C> [perm] | default",
					     cmd_temp_threshold_set, 3, 1),
			       SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_temp_thermaltrip_cmds,
			       SHELL_CMD_ARG(get, &temp_thermaltrip_name_dcmd, "get all|<name>",
					     cmd_temp_thermaltrip_get, 2, 0),
			       SHELL_CMD_ARG(set, &temp_thermaltrip_name_dcmd,
					     "set all|<name> <0|1> [perm], 1 = enable",
					     cmd_temp_thermaltrip_set, 3, 1),
			       SHELL_SUBCMD_SET_END);

/* level 1 */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_temp_cmds,
	SHELL_CMD(status, NULL, "temp chip status and TMP75 ALERT pins", cmd_temp_status),
	SHELL_CMD(threshold, &sub_temp_threshold_cmds, "temp threshold get/set", NULL),
	SHELL_CMD(thermaltrip, &sub_temp_thermaltrip_cmds, "thermal trip switch get/set", NULL),
	SHELL_SUBCMD_SET_END);

/* Root of command temp */
SHELL_CMD_REGISTER(temp, &sub_temp_cmds, "temperature sensor commands", NULL);
