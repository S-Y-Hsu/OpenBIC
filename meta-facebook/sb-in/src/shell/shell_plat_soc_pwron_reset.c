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
#include <string.h>
#include "plat_cpld.h"
#include "shell_plat_item.h"

/* HAMSA/ZORA reset bits only, other bits in these registers are left untouched */
#define POWER_ON_RESET_OVERRIDE_MASK (BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1))
#define SYS_RESET_OVERRIDE_MASK (BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3))

static const plat_shell_item_t soc_pwron_reset_items[] = {
	PLAT_SHELL_CPLD_BIT("HAMSA_POWER_ON_RESET_PLD_L", CPLD_OFFSET_ASIC_POWER_ON_RESET, 5),
	PLAT_SHELL_CPLD_BIT("ZORA00_POWER_ON_RESET_PLD_L", CPLD_OFFSET_ASIC_POWER_ON_RESET, 4),
	PLAT_SHELL_CPLD_BIT("ZORA01_POWER_ON_RESET_PLD_L", CPLD_OFFSET_ASIC_POWER_ON_RESET, 3),
	PLAT_SHELL_CPLD_BIT("ZORA10_POWER_ON_RESET_PLD_L", CPLD_OFFSET_ASIC_POWER_ON_RESET, 2),
	PLAT_SHELL_CPLD_BIT("ZORA11_POWER_ON_RESET_PLD_L", CPLD_OFFSET_ASIC_POWER_ON_RESET, 1),
	PLAT_SHELL_CPLD_BIT("HAMSA_SYS_RST_PLD_L", CPLD_OFFSET_ASIC_SYS_RESET, 7),
	PLAT_SHELL_CPLD_BIT("ZORA00_SYS_RST_PLD_L", CPLD_OFFSET_ASIC_SYS_RESET, 6),
	PLAT_SHELL_CPLD_BIT("ZORA01_SYS_RST_PLD_L", CPLD_OFFSET_ASIC_SYS_RESET, 5),
	PLAT_SHELL_CPLD_BIT("ZORA10_SYS_RST_PLD_L", CPLD_OFFSET_ASIC_SYS_RESET, 4),
	PLAT_SHELL_CPLD_BIT("ZORA11_SYS_RST_PLD_L", CPLD_OFFSET_ASIC_SYS_RESET, 3),
};

static int cmd_soc_pwron_reset_get(const struct shell *shell, size_t argc, char **argv)
{
	return plat_shell_item_get(shell, soc_pwron_reset_items, ARRAY_SIZE(soc_pwron_reset_items),
				   argv[1]);
}

static int cmd_soc_pwron_reset_set(const struct shell *shell, size_t argc, char **argv)
{
	return plat_shell_item_set(shell, soc_pwron_reset_items, ARRAY_SIZE(soc_pwron_reset_items),
				   argv[1], argv[2], NULL);
}

/*
 * override 1: release all HAMSA/ZORA power-on and sys resets
 * override 0: hold HAMSA/ZORA in power-on reset, sys resets released
 */
static int cmd_soc_pwron_reset_override(const struct shell *shell, size_t argc, char **argv)
{
	uint8_t por_val;

	if (!strcmp(argv[1], "1")) {
		por_val = 0xFF;
	} else if (!strcmp(argv[1], "0")) {
		por_val = 0x00;
	} else {
		shell_error(shell, "Invalid value %s, override <0|1>", argv[1]);
		return -EINVAL;
	}

	if (!plat_update_cpld(CPLD_OFFSET_ASIC_POWER_ON_RESET, POWER_ON_RESET_OVERRIDE_MASK,
			      por_val)) {
		shell_error(shell, "update CPLD 0x%02X failed", CPLD_OFFSET_ASIC_POWER_ON_RESET);
		return -EIO;
	}

	if (!plat_update_cpld(CPLD_OFFSET_ASIC_SYS_RESET, SYS_RESET_OVERRIDE_MASK, 0xFF)) {
		shell_error(shell, "update CPLD 0x%02X failed", CPLD_OFFSET_ASIC_SYS_RESET);
		return -EIO;
	}

	shell_print(shell, "soc_pwron_reset override %s done", argv[1]);
	return 0;
}

PLAT_SHELL_ITEM_DYNAMIC_CMD_CREATE(soc_pwron_reset_names, soc_pwron_reset_items);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_soc_pwron_reset_cmds,
			       SHELL_CMD_ARG(get, &soc_pwron_reset_names, "get all|<name>",
					     cmd_soc_pwron_reset_get, 2, 0),
			       SHELL_CMD_ARG(set, &soc_pwron_reset_names, "set all|<name> <0|1>",
					     cmd_soc_pwron_reset_set, 3, 0),
			       SHELL_CMD_ARG(override, NULL, "override <0|1>",
					     cmd_soc_pwron_reset_override, 2, 0),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(soc_pwron_reset, &sub_soc_pwron_reset_cmds, "soc_pwron_reset commands", NULL);
