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

#include <stdlib.h>
#include <string.h>
#include <zephyr.h>
#include <shell/shell.h>
#include "plat_temp_threshold.h"
#include "shell_plat_perm_config.h"
#include "shell_plat_temp.h"

/* Widest range of the temp chips (TMP432/EMC1413 extended range, TMP75) */
#define TEMP_THRESHOLD_MIN_MC (-55000)
#define TEMP_THRESHOLD_MAX_MC 150000

static int threshold_print(const struct shell *shell, uint8_t idx)
{
	int32_t val = 0;
	int32_t def = 0;

	if (!plat_temp_threshold_get(idx, &val) || !plat_temp_threshold_get_default(idx, &def)) {
		shell_error(shell, "%s : read failed", plat_temp_threshold_get_name(idx));
		return -EIO;
	}

	shell_print(shell, "%s : %d (default %d)", plat_temp_threshold_get_name(idx), val, def);
	return 0;
}

int cmd_temp_threshold_get(const struct shell *shell, size_t argc, char **argv)
{
	if (!strcmp(argv[1], "all")) {
		int ret = 0;
		for (uint8_t i = 0; i < TEMP_THRESHOLD_COUNT; i++) {
			if (threshold_print(shell, i))
				ret = -EIO;
		}
		return ret;
	}

	int idx = plat_temp_threshold_find(argv[1]);
	if (idx < 0) {
		shell_error(shell, "Unknown name: %s", argv[1]);
		return -EINVAL;
	}

	return threshold_print(shell, idx);
}

int cmd_temp_threshold_set(const struct shell *shell, size_t argc, char **argv)
{
	bool is_perm = false;
	if (argc == 4) {
		if (strcmp(argv[3], "perm")) {
			shell_error(shell, "The last argument must be <perm>");
			return -EINVAL;
		}
		is_perm = true;
	}

	/* default only writes the chip, perm values are cleared by perm_config clear */
	bool is_default = !strcmp(argv[2], "default");
	if (is_default && is_perm) {
		shell_error(shell, "default can't be stored as perm, use perm_config clear");
		return -EINVAL;
	}

	long val = 0;
	if (!is_default) {
		char *end = NULL;
		val = strtol(argv[2], &end, 0);
		if (*end != '\0' || val < TEMP_THRESHOLD_MIN_MC || val > TEMP_THRESHOLD_MAX_MC) {
			shell_error(shell, "Invalid value %s, range %d ~ %d millidegree C", argv[2],
				    TEMP_THRESHOLD_MIN_MC, TEMP_THRESHOLD_MAX_MC);
			return -EINVAL;
		}
	}

	/* the perm array uses this value as "not set" */
	if (is_perm && (int32_t)val == TEMP_THRESHOLD_PERM_UNSET) {
		shell_error(shell, "%ld can't be stored as perm", val);
		return -EINVAL;
	}

	uint8_t first = 0;
	uint8_t last = TEMP_THRESHOLD_COUNT - 1;
	if (strcmp(argv[1], "all")) {
		int idx = plat_temp_threshold_find(argv[1]);
		if (idx < 0) {
			shell_error(shell, "Unknown name: %s", argv[1]);
			return -EINVAL;
		}
		first = last = idx;
	}

	int32_t perm[TEMP_THRESHOLD_COUNT];
	if (is_perm && !plat_temp_threshold_perm_load(perm)) {
		shell_error(shell, "read perm from eeprom failed");
		return -EIO;
	}

	int ret = 0;
	for (uint8_t i = first; i <= last; i++) {
		const char *name = plat_temp_threshold_get_name(i);
		int32_t set_val = val;

		if (is_default)
			plat_temp_threshold_get_default(i, &set_val);

		if (!plat_temp_threshold_set(i, set_val)) {
			shell_error(shell, "%s : write failed", name);
			ret = -EIO;
			continue;
		}

		if (is_perm)
			perm[i] = set_val;

		shell_print(shell, "%s : set to %d%s", name, set_val,
			    is_perm ? " (perm)" : (is_default ? " (default)" : ""));
	}

	/* one EEPROM write for all updated entries */
	if (is_perm && !plat_temp_threshold_perm_store(perm)) {
		shell_error(shell, "write perm to eeprom failed");
		return -EIO;
	}

	return ret;
}

int temp_threshold_perm_print(const struct shell *shell)
{
	int32_t perm[TEMP_THRESHOLD_COUNT];

	if (!plat_temp_threshold_perm_load(perm)) {
		shell_error(shell, "TEMP_THRESHOLD : read perm failed");
		return 0;
	}

	int count = 0;
	for (uint8_t i = 0; i < TEMP_THRESHOLD_COUNT; i++) {
		if (perm[i] == TEMP_THRESHOLD_PERM_UNSET)
			continue;

		shell_print(shell, "%s : %d", plat_temp_threshold_get_name(i), perm[i]);
		count++;
	}

	return count;
}
