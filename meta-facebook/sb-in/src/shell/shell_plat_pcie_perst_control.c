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

static const plat_shell_item_t pcie_perst_items[] = {
	PLAT_SHELL_CPLD_BIT("HAMSA_PCIE0_PERST_B_PLD_N", CPLD_OFFSET_PCIE_PERST, 0),
	PLAT_SHELL_CPLD_BIT("HAMSA_PCIE1_PERST_B_PLD_N", CPLD_OFFSET_PCIE_PERST, 1),
	PLAT_SHELL_CPLD_BIT("HAMSA_PCIE2_PERST_B_PLD_N", CPLD_OFFSET_PCIE_PERST, 2),
	PLAT_SHELL_CPLD_BIT("HAMSA_PCIE3_PERST_B_PLD_N", CPLD_OFFSET_PCIE_PERST, 3),
};

static int cmd_pcie_perst_get(const struct shell *shell, size_t argc, char **argv)
{
	return plat_shell_item_get(shell, pcie_perst_items, ARRAY_SIZE(pcie_perst_items), argv[1]);
}

static int cmd_pcie_perst_set(const struct shell *shell, size_t argc, char **argv)
{
	return plat_shell_item_set(shell, pcie_perst_items, ARRAY_SIZE(pcie_perst_items), argv[1],
				   argv[2]);
}

PLAT_SHELL_ITEM_DYNAMIC_CMD_CREATE(pcie_perst_names, pcie_perst_items);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_pcie_perst_cmds,
			       SHELL_CMD_ARG(get, &pcie_perst_names, "get all|<name>",
					     cmd_pcie_perst_get, 2, 0),
			       SHELL_CMD_ARG(set, &pcie_perst_names, "set all|<name> <0|1>",
					     cmd_pcie_perst_set, 3, 0),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(pcie_perst_control, &sub_pcie_perst_cmds, "PCIe PERST control via CPLD", NULL);
