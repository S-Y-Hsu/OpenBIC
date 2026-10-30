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

#ifndef PLAT_TEMP_THRESHOLD_H
#define PLAT_TEMP_THRESHOLD_H

#include <stdbool.h>
#include <stdint.h>

/* Number of entries in the temp threshold table, also the element count of its perm array */
#define TEMP_THRESHOLD_COUNT 22

/* Perm array element that is not set (erased EEPROM) */
#define TEMP_THRESHOLD_PERM_UNSET ((int32_t)0xFFFFFFFF)

const char *plat_temp_threshold_get_name(uint8_t idx);
int plat_temp_threshold_find(const char *name);
bool plat_temp_threshold_get_default(uint8_t idx, int32_t *millidegree_celsius);
bool plat_temp_threshold_get(uint8_t idx, int32_t *millidegree_celsius);
bool plat_temp_threshold_set(uint8_t idx, int32_t millidegree_celsius);

/* Perm values of all entries, an entry not set reads back as TEMP_THRESHOLD_PERM_UNSET */
bool plat_temp_threshold_perm_load(int32_t perm[TEMP_THRESHOLD_COUNT]);
bool plat_temp_threshold_perm_store(const int32_t perm[TEMP_THRESHOLD_COUNT]);

/* Write default or perm value of every entry to the temp chips, called at boot */
void plat_temp_threshold_init(void);

#endif
