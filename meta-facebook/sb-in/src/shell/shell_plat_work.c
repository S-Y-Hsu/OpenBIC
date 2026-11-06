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
#include "plat_work.h"

void cmd_plat_work_status(const struct shell *shell, size_t argc, char **argv)
{
	size_t count = 0;
	const struct plat_work *list = plat_get_work_list(&count);

	shell_print(shell, "%-20s %-10s %s", "name", "interval", "state");
	for (size_t i = 0; i < count; i++) {
		if (list[i].suspended)
			shell_print(shell, "%-20s %-8ums SUSPENDED (fail %u)", list[i].name,
				    list[i].interval_ms, list[i].fail_cnt);
		else
			shell_print(shell, "%-20s %-8ums running", list[i].name,
				    list[i].interval_ms);
	}
}

void cmd_plat_work_reset(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_warn(shell, "Help: plat_work reset <work name|all>");
		return;
	}

	int ret = plat_work_resume(argv[1]);
	if (ret < 0) {
		shell_error(shell, "No platform work named %s", argv[1]);
		return;
	}

	shell_print(shell, "%d work(s) resumed", ret);
}

/* Tab completion for work names: index 0 is "all", then every entry of plat_work_list */
static void plat_work_name_get(size_t idx, struct shell_static_entry *entry)
{
	size_t count = 0;
	const struct plat_work *list = plat_get_work_list(&count);

	if (idx == 0)
		entry->syntax = "all";
	else if (idx - 1 < count)
		entry->syntax = list[idx - 1].name;
	else
		entry->syntax = NULL; // end of list

	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}

SHELL_DYNAMIC_CMD_CREATE(dsub_plat_work_name, plat_work_name_get);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_plat_work_cmd,
			       SHELL_CMD(status, NULL, "Show platform work status",
					 cmd_plat_work_status),
			       SHELL_CMD(reset, &dsub_plat_work_name,
					 "Resume suspended work: <work name|all>",
					 cmd_plat_work_reset),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(work, &sub_plat_work_cmd, "Platform work queue commands", NULL);
