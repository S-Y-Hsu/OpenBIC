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
#include "plat_shell_item.h"

/* STRAP_HIZ_EN is low active: 0 = CPLD strap outputs Hi-Z (manual), 1 = CPLD drives straps */
static const plat_shell_item_t strap_control_manual_items[] = {
	PLAT_SHELL_CPLD_BIT("STRAP_HIZ_EN", CPLD_OFFSET_MANUAL_CONTROL_STRAP, 0),
};

static int cmd_strap_control_manual_get(const struct shell *shell, size_t argc, char **argv)
{
	return plat_shell_item_get(shell, strap_control_manual_items,
				   ARRAY_SIZE(strap_control_manual_items), argv[1]);
}

static int cmd_strap_control_manual_set(const struct shell *shell, size_t argc, char **argv)
{
	return plat_shell_item_set(shell, strap_control_manual_items,
				   ARRAY_SIZE(strap_control_manual_items), argv[1], argv[2], NULL);
}

PLAT_SHELL_ITEM_DYNAMIC_CMD_CREATE(strap_control_manual_names, strap_control_manual_items);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_strap_control_manual_cmds,
	SHELL_CMD_ARG(get, &strap_control_manual_names, "get all|<name>",
		      cmd_strap_control_manual_get, 2, 0),
	SHELL_CMD_ARG(set, &strap_control_manual_names,
		      "set all|<name> <0|1>, 0 = Hi-Z (manual), 1 = CPLD drive",
		      cmd_strap_control_manual_set, 3, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(strap_control_manual, &sub_strap_control_manual_cmds,
		   "manual strap control (STRAP_HIZ_EN, low active)", NULL);
