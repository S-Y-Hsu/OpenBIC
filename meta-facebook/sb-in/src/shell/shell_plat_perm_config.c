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
#include "plat_user_setting.h"
#include "shell_plat_perm_config.h"

/* Restore perm settings of every shell command that supports [perm], called at boot */
void plat_shell_perm_restore(void)
{
	thermaltrip_switch_perm_restore();
}

static int cmd_perm_config_get(const struct shell *shell, size_t argc, char **argv)
{
	int config_count = 0;

	for (uint8_t id = PERM_ID_NONE + 1; id < PERM_ID_MAX; id++) {
		uint32_t val = 0; /* little-endian, up to 4 bytes */

		int ret = plat_perm_get(id, &val, plat_perm_get_size(id));
		if (ret == -ENODATA)
			continue;
		if (ret) {
			shell_error(shell, "%s : read perm failed", plat_perm_get_name(id));
			continue;
		}

		shell_print(shell, "%s : %u", plat_perm_get_name(id), val);
		config_count++;
	}

	if (!config_count)
		shell_print(shell, "no perm setting exist");

	return 0;
}

static int cmd_perm_config_clear(const struct shell *shell, size_t argc, char **argv)
{
	if (!plat_perm_clear()) {
		shell_error(shell, "clear perm settings failed");
		return -EIO;
	}

	shell_print(shell, "clear perm settings done, takes effect after reboot");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_perm_config_cmds,
			       SHELL_CMD(get, NULL, "get perm settings", cmd_perm_config_get),
			       SHELL_CMD(clear, NULL, "clear perm settings", cmd_perm_config_clear),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(perm_config, &sub_perm_config_cmds, "perm_config get/clear commands", NULL);
