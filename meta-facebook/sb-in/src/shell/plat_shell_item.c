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
#include <logging/log.h>
#include "plat_cpld.h"
#include "plat_user_setting.h"
#include "plat_shell_item.h"

LOG_MODULE_REGISTER(plat_shell_item);

static uint8_t item_mask(const plat_shell_item_t *item)
{
	return (uint8_t)((1U << item->width) - 1);
}

static const plat_shell_item_t *item_find(const plat_shell_item_t *items, size_t count,
					  const char *name)
{
	for (size_t i = 0; i < count; i++) {
		if (!strcmp(name, items[i].name))
			return &items[i];
	}

	return NULL;
}

static bool item_read(const plat_shell_item_t *item, uint8_t *val)
{
	uint8_t reg = 0;

	if (!plat_read_cpld(item->offset, &reg, 1))
		return false;

	*val = (reg >> item->bit) & item_mask(item);
	return true;
}

static bool item_write(const plat_shell_item_t *item, uint8_t val)
{
	return plat_update_cpld(item->offset, item_mask(item) << item->bit, val << item->bit);
}

static int item_print(const struct shell *shell, const plat_shell_item_t *item)
{
	uint8_t val = 0;

	if (!item_read(item, &val)) {
		shell_error(shell, "%s : read CPLD 0x%02X failed", item->name, item->offset);
		return -EIO;
	}

	shell_print(shell, "%s : %d", item->name, val);
	return 0;
}

static int item_set(const struct shell *shell, const plat_shell_item_t *item, uint8_t val,
		    bool is_perm)
{
	if (!item_write(item, val)) {
		shell_error(shell, "%s : write CPLD 0x%02X failed", item->name, item->offset);
		return -EIO;
	}

	if (is_perm && !plat_perm_set(item->perm_id, &val, sizeof(val))) {
		shell_error(shell, "%s : write perm to eeprom failed", item->name);
		return -EIO;
	}

	shell_print(shell, "%s : set to %d%s", item->name, val, is_perm ? " (perm)" : "");
	return 0;
}

int plat_shell_item_get(const struct shell *shell, const plat_shell_item_t *items, size_t count,
			const char *name)
{
	if (!strcmp(name, "all")) {
		int ret = 0;
		for (size_t i = 0; i < count; i++) {
			if (item_print(shell, &items[i]))
				ret = -EIO;
		}
		return ret;
	}

	const plat_shell_item_t *item = item_find(items, count, name);
	if (!item) {
		shell_error(shell, "Unknown name: %s", name);
		return -EINVAL;
	}

	return item_print(shell, item);
}

int plat_shell_item_set(const struct shell *shell, const plat_shell_item_t *items, size_t count,
			const char *name, const char *value, const char *opt)
{
	bool is_perm = false;
	if (opt) {
		if (strcmp(opt, "perm")) {
			shell_error(shell, "The last argument must be <perm>");
			return -EINVAL;
		}
		is_perm = true;
	}

	char *end = NULL;
	unsigned long val = strtoul(value, &end, 0);

	if (!strcmp(name, "all")) {
		/* every item must accept the value before anything is written */
		for (size_t i = 0; i < count; i++) {
			if (*end != '\0' || val > item_mask(&items[i])) {
				shell_error(shell, "Invalid value %s for %s", value, items[i].name);
				return -EINVAL;
			}
			if (is_perm && items[i].perm_id == PERM_ID_NONE) {
				shell_error(shell, "%s does not support perm", items[i].name);
				return -EINVAL;
			}
		}

		int ret = 0;
		for (size_t i = 0; i < count; i++) {
			if (item_set(shell, &items[i], (uint8_t)val, is_perm))
				ret = -EIO;
		}
		return ret;
	}

	const plat_shell_item_t *item = item_find(items, count, name);
	if (!item) {
		shell_error(shell, "Unknown name: %s", name);
		return -EINVAL;
	}

	if (*end != '\0' || val > item_mask(item)) {
		shell_error(shell, "Invalid value %s, range 0 ~ %d", value, item_mask(item));
		return -EINVAL;
	}

	if (is_perm && item->perm_id == PERM_ID_NONE) {
		shell_error(shell, "%s does not support perm", item->name);
		return -EINVAL;
	}

	return item_set(shell, item, (uint8_t)val, is_perm);
}

/* Write perm values stored in user setting back to CPLD, called at boot */
void plat_shell_item_restore(const plat_shell_item_t *items, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		const plat_shell_item_t *item = &items[i];
		uint8_t val = 0;

		if (item->perm_id == PERM_ID_NONE)
			continue;

		int ret = plat_perm_get(item->perm_id, &val, sizeof(val));
		if (ret == -ENODATA)
			continue;
		if (ret) {
			LOG_ERR("read %s perm failed", item->name);
			continue;
		}

		if (val > item_mask(item)) {
			LOG_WRN("%s perm value 0x%02x out of range, skip", item->name, val);
			continue;
		}

		if (!item_write(item, val))
			LOG_ERR("restore %s = %d failed", item->name, val);
	}
}

void plat_shell_item_dynamic_get(const plat_shell_item_t *items, size_t count, size_t idx,
				 struct shell_static_entry *entry)
{
	entry->syntax = NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;

	if (idx == 0)
		entry->syntax = "all";
	else if (idx <= count)
		entry->syntax = items[idx - 1].name;
}
