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
#include "plat_cpld.h"
#include "plat_user_setting.h"
#include "shell_plat_item.h"
#include "shell_plat_perm_config.h"

static const plat_shell_item_t thermaltrip_items[] = {
	PLAT_SHELL_CPLD_BIT_PERM("THERMALTRIP_SWITCH", CPLD_OFFSET_THERMALTRIP_SWITCH, 0,
				 PERM_ID_THERMALTRIP),
};

void thermaltrip_switch_perm_restore(void)
{
	plat_shell_item_restore(thermaltrip_items, ARRAY_SIZE(thermaltrip_items));
}

static int cmd_thermaltrip_get(const struct shell *shell, size_t argc, char **argv)
{
	return plat_shell_item_get(shell, thermaltrip_items, ARRAY_SIZE(thermaltrip_items),
				   argv[1]);
}

static int cmd_thermaltrip_set(const struct shell *shell, size_t argc, char **argv)
{
	return plat_shell_item_set(shell, thermaltrip_items, ARRAY_SIZE(thermaltrip_items), argv[1],
				   argv[2], (argc == 4) ? argv[3] : NULL);
}

PLAT_SHELL_ITEM_DYNAMIC_CMD_CREATE(thermaltrip_names, thermaltrip_items);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_thermaltrip_cmds,
			       SHELL_CMD_ARG(get, &thermaltrip_names, "get all|<name>",
					     cmd_thermaltrip_get, 2, 0),
			       SHELL_CMD_ARG(set, &thermaltrip_names,
					     "set all|<name> <0|1> [perm], 1 = enable",
					     cmd_thermaltrip_set, 3, 1),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(thermaltrip_switch, &sub_thermaltrip_cmds, "thermal trip switch commands", NULL);
