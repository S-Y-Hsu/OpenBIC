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

SHELL_CMD_REGISTER(work_status, NULL, "Show platform work queue status",
		   cmd_plat_work_status);
