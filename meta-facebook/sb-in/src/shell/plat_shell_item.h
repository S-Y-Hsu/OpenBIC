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

#ifndef PLAT_SHELL_ITEM_H
#define PLAT_SHELL_ITEM_H

#include <zephyr.h>
#include <shell/shell.h>

/*
 * Table-driven shell helper for commands that read/write named hardware signals.
 *
 * Every command built on this helper follows the same syntax:
 *   <cmd> get all|<name>
 *   <cmd> set all|<name> <value>
 */

typedef struct {
	const char *name;
	uint8_t offset; /* CPLD register offset */
	uint8_t bit; /* lowest bit of the field */
	uint8_t width; /* field width in bits, 1 = single bit */
} plat_shell_item_t;

#define PLAT_SHELL_CPLD_BIT(_name, _offset, _bit)                                                  \
	{                                                                                          \
		.name = _name, .offset = _offset, .bit = _bit, .width = 1                          \
	}

int plat_shell_item_get(const struct shell *shell, const plat_shell_item_t *items, size_t count,
			const char *name);
int plat_shell_item_set(const struct shell *shell, const plat_shell_item_t *items, size_t count,
			const char *name, const char *value);
void plat_shell_item_dynamic_get(const plat_shell_item_t *items, size_t count, size_t idx,
				 struct shell_static_entry *entry);

/* Create a dynamic sub-command listing "all" followed by every item name */
#define PLAT_SHELL_ITEM_DYNAMIC_CMD_CREATE(_name, _items)                                          \
	static void _name##_get(size_t idx, struct shell_static_entry *entry)                      \
	{                                                                                          \
		plat_shell_item_dynamic_get(_items, ARRAY_SIZE(_items), idx, entry);               \
	}                                                                                          \
	SHELL_DYNAMIC_CMD_CREATE(_name, _name##_get)

#endif
