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

#ifndef PLAT_USER_SETTING_H
#define PLAT_USER_SETTING_H

#include "plat_cpld.h"
#include "plat_pldm_sensor.h"

#define MMC_SLOT_USER_SETTING_OFFSET 0x0000
#define MMC_TRAY_LOCATION_OFFSET 0x03FF

/*
 * Perm settings storage in log EEPROM.
 *
 * This module only stores bytes per id, it does not know what the value means
 * or where it is applied. Each owner writes its id with plat_perm_set() and
 * restores it at its own init time with plat_perm_get().
 * An id with all bytes 0xFF is treated as not set.
 */
enum PLAT_PERM_ID {
	PERM_ID_NONE = 0,
	PERM_ID_THERMALTRIP,
	PERM_ID_MAX,
};

bool plat_perm_set(uint8_t id, const void *val, uint8_t len);
int plat_perm_get(uint8_t id, void *val, uint8_t len);
bool plat_perm_clear(void);
const char *plat_perm_get_name(uint8_t id);
uint8_t plat_perm_get_size(uint8_t id);

#endif
