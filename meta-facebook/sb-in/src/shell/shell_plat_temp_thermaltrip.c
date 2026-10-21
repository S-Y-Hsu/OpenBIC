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
#include "shell_plat_temp.h"

static const plat_shell_item_t thermaltrip_items[] = {
	PLAT_SHELL_CPLD_BIT_PERM("THERMALTRIP_SWITCH", CPLD_OFFSET_THERMALTRIP_SWITCH, 0,
				 PERM_ID_THERMALTRIP),
};

void thermaltrip_switch_perm_restore(void)
{
	plat_shell_item_restore(thermaltrip_items, ARRAY_SIZE(thermaltrip_items));
}

int cmd_temp_thermaltrip_get(const struct shell *shell, size_t argc, char **argv)
{
	return plat_shell_item_get(shell, thermaltrip_items, ARRAY_SIZE(thermaltrip_items),
				   argv[1]);
}

int cmd_temp_thermaltrip_set(const struct shell *shell, size_t argc, char **argv)
{
	return plat_shell_item_set(shell, thermaltrip_items, ARRAY_SIZE(thermaltrip_items), argv[1],
				   argv[2], (argc == 4) ? argv[3] : NULL);
}

void temp_thermaltrip_name_entry_get(size_t idx, struct shell_static_entry *entry)
{
	plat_shell_item_dynamic_get(thermaltrip_items, ARRAY_SIZE(thermaltrip_items), idx, entry);
}
