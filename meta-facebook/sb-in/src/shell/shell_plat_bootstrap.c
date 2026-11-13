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
#include <stdlib.h>
#include <string.h>
#include "plat_bootstrap.h"
#include "shell_plat_perm_config.h"

static void bootstrap_print_one(const struct shell *shell, uint8_t rail)
{
	uint8_t *name = NULL;
	int drive_level = -1;

	if (!strap_name_get(rail, &name) || !get_bootstrap_change_drive_level(rail, &drive_level)) {
		shell_error(shell, "Can't get bootstrap by rail index: %d", rail);
		return;
	}

	shell_print(shell, "%-4d|%-40s|0x%-23.2x", rail, name, drive_level);
}

static int cmd_bootstrap_get(const struct shell *shell, size_t argc, char **argv)
{
	if (!strcmp(argv[1], "all")) {
		shell_print(shell, "%-4s|%-40s|%-25s", "id", "strap name", "hex-value");
		for (uint8_t i = 0; i < get_strap_index_max(); i++)
			bootstrap_print_one(shell, i);
		return 0;
	}

	uint8_t rail;
	if (!strap_enum_get(argv[1], &rail)) {
		shell_error(shell, "Invalid rail name: %s", argv[1]);
		return -EINVAL;
	}

	shell_print(shell, "%-4s|%-40s|%-25s", "id", "strap name", "hex-value");
	bootstrap_print_one(shell, rail);
	return 0;
}

static bool bootstrap_set_one(uint8_t rail, uint8_t level, bool is_perm, bool is_default)
{
	uint8_t change_setting_value = 0;

	if (!set_bootstrap_table_and_user_settings(rail, &change_setting_value, level, is_perm,
						   is_default))
		return false;

	// write change_setting_value to cpld, gpio or io-exp
	return set_bootstrap_val_to_device(rail, change_setting_value);
}

static int cmd_bootstrap_set(const struct shell *shell, size_t argc, char **argv)
{
	bool is_perm = false;

	if (argc == 4) {
		if (strcmp(argv[3], "perm")) {
			shell_error(shell, "The last argument must be <perm>");
			return -EINVAL;
		}
		is_perm = true;
	}

	if (!strcmp(argv[1], "all")) {
		if (strcmp(argv[2], "default") || is_perm) {
			shell_error(shell, "Only support bootstrap set all default");
			return -EINVAL;
		}

		bool all_success = true;
		for (uint8_t i = 0; i < get_strap_index_max(); i++) {
			if (!bootstrap_set_one(i, 0, false, true)) {
				shell_error(shell, "bootstrap[%2d] set default failed", i);
				all_success = false;
			}
		}

		if (all_success)
			shell_print(shell, "Set all bootstrap to default");
		return all_success ? 0 : -EIO;
	}

	uint8_t rail;
	if (!strap_enum_get(argv[1], &rail)) {
		shell_error(shell, "Invalid rail name: %s", argv[1]);
		return -EINVAL;
	}

	bool is_default = !strcmp(argv[2], "default");
	uint8_t level = 0;
	if (!is_default) {
		char *end = NULL;
		unsigned long val = strtoul(argv[2], &end, 16);
		if ((end == argv[2]) || (*end != '\0') || (val > 0xFF)) {
			shell_error(shell, "Invalid hex-value: %s", argv[2]);
			return -EINVAL;
		}
		level = (uint8_t)val;
	}

	if (!bootstrap_set_one(rail, level, is_perm, is_default)) {
		shell_error(shell, "plat bootstrap set failed");
		return -EIO;
	}

	shell_print(shell, "Set %s %s, %svolatile", argv[1], argv[2], is_perm ? "non-" : "");
	return 0;
}

int bootstrap_perm_print(const struct shell *shell)
{
	int count = 0;

	for (uint8_t i = 0; i < get_strap_index_max(); i++) {
		uint8_t *name = NULL;
		uint8_t value = 0;

		if (!bootstrap_perm_get(i, &value) || !strap_name_get(i, &name))
			continue;

		shell_print(shell, "%s : 0x%02x", name, value);
		count++;
	}

	return count;
}

static void strap_name_entry_get(size_t idx, struct shell_static_entry *entry)
{
	uint8_t *name = NULL;

	if (idx == 0)
		name = (uint8_t *)"all";
	else
		strap_name_get((uint8_t)(idx - 1), &name);

	entry->syntax = (const char *)name;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}

SHELL_DYNAMIC_CMD_CREATE(strap_name, strap_name_entry_get);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_bootstrap_cmds,
	SHELL_CMD_ARG(get, &strap_name, "get all|<strap-name>", cmd_bootstrap_get, 2, 0),
	SHELL_CMD_ARG(set, &strap_name,
		      "set <strap-name> <hex-value>|default [perm], set all default",
		      cmd_bootstrap_set, 3, 1),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(bootstrap, &sub_bootstrap_cmds, "bootstrap set/get commands", NULL);
