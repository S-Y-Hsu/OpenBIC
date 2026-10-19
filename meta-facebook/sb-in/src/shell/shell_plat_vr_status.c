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

// status == VR_STAUS_E_MAX prints every status of the rail
static void vr_status_print(const struct shell *shell, uint8_t rail, uint8_t status)
{
	uint8_t *rail_name = NULL;
	if (!vr_rail_name_get(rail, &rail_name)) {
		shell_error(shell, "Can't find vr_rail_name by rail index: %x", rail);
		return;
	}

	uint8_t first = status, last = status;
	if (status == VR_STAUS_E_MAX) {
		shell_print(shell, "[%-2x]%-50s", rail, rail_name);
		first = 0;
		last = VR_STAUS_E_MAX - 1;
	}

	for (uint8_t i = first; i <= last; i++) {
		uint8_t *status_name = NULL;
		uint16_t vr_status = 0xFFFF;

		vr_status_name_get(i, &status_name);
		if (!plat_get_vr_status(rail, i, &vr_status)) {
			shell_error(shell, "Can't get %s by rail index: %x", status_name, rail);
			continue;
		}

		if (status == VR_STAUS_E_MAX)
			shell_print(shell, "    %-18s: 0x%04x", status_name, vr_status);
		else
			shell_print(shell, "[%-2x]%-50s %-18s: 0x%04x", rail, rail_name,
				    status_name, vr_status);
	}
}

int cmd_vr_status_get(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 3) {
		shell_error(shell, "vr status get all|<voltage-rail> all|<which-status>");
		return -1;
	}

	if (!is_p3v3_pwrgd())
		return -1;

	uint8_t status = VR_STAUS_E_MAX;
	if (strcmp(argv[2], "all") && !vr_status_enum_get((uint8_t *)argv[2], &status)) {
		shell_error(shell, "Invalid vr status name: %s", argv[2]);
		return -1;
	}

	if (!strcmp(argv[1], "all")) {
		for (int i = 0; i < vr_rail_count(); i++) {
			vr_status_print(shell, (uint8_t)i, status);
		}
		return 0;
	}

	uint8_t rail;
	if (!vr_rail_arg_get(shell, argv[1], &rail))
		return -1;

	vr_status_print(shell, rail, status);
	return 0;
}

int cmd_vr_status_clear(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(shell, "vr status clear all|<voltage-rail>");
		return -1;
	}

	if (!is_p3v3_pwrgd())
		return -1;

	if (!strcmp(argv[1], "all")) {
		for (int i = 0; i < vr_rail_count(); i++) {
			if (!plat_clear_vr_status((uint8_t)i)) {
				shell_error(shell, "Can't clear vr status by rail index: %x", i);
			}
		}
		shell_print(shell, "All VR clear vr status finish");
		return 0;
	}

	uint8_t rail;
	if (!vr_rail_arg_get(shell, argv[1], &rail))
		return -1;

	if (!plat_clear_vr_status(rail)) {
		shell_error(shell, "Can't clear vr status by rail index: %x", rail);
		return -1;
	}

	shell_print(shell, "[%-2x]%-50s clear vr status finish", rail, argv[1]);
	return 0;
}
