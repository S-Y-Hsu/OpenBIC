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

#include <string.h>
#include "plat_hook.h"
#include "shell_plat_vr.h"

static void vr_peak_print(const struct shell *shell, uint8_t rail)
{
	uint8_t *rail_name = NULL;
	int32_t peak_mv = VR_PEAK_NONE;

	if (!vr_rail_name_get(rail, &rail_name) || !vr_rail_peak_get(rail, &peak_mv)) {
		shell_error(shell, "Can't get voltage peak by rail index: %x", rail);
		return;
	}

	if (peak_mv == VR_PEAK_NONE) {
		shell_print(shell, "[%-2x]%-50s peak: N/A", rail, rail_name);
		return;
	}

	// post_vr_read clamps negative readings to 0, so peak_mv is never negative
	shell_print(shell, "[%-2x]%-50s peak: %d.%03d V", rail, rail_name, peak_mv / 1000,
		    peak_mv % 1000);
}

int cmd_vr_peak_get(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(shell, "vr peak get all|<voltage-rail>");
		return -1;
	}

	if (!strcmp(argv[1], "all")) {
		for (int i = 0; i < vr_rail_count(); i++) {
			vr_peak_print(shell, (uint8_t)i);
		}
		return 0;
	}

	uint8_t rail;
	if (!vr_rail_arg_get(shell, argv[1], &rail))
		return -1;

	vr_peak_print(shell, rail);
	return 0;
}

int cmd_vr_peak_clear(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(shell, "vr peak clear all|<voltage-rail>");
		return -1;
	}

	if (!strcmp(argv[1], "all")) {
		for (int i = 0; i < vr_rail_count(); i++) {
			vr_rail_peak_clear((uint8_t)i);
		}
		shell_print(shell, "All VR clear voltage peak finish");
		return 0;
	}

	uint8_t rail;
	if (!vr_rail_arg_get(shell, argv[1], &rail))
		return -1;

	if (!vr_rail_peak_clear(rail)) {
		shell_error(shell, "Can't clear voltage peak by rail index: %x", rail);
		return -1;
	}

	shell_print(shell, "[%-2x]%-50s clear voltage peak finish", rail, argv[1]);
	return 0;
}
