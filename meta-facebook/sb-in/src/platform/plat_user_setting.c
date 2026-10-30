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
#include <logging/log.h>
#include "eeprom.h"
#include "plat_fru.h"
#include "plat_temp_threshold.h"
#include "plat_user_setting.h"
#include "shell_plat_perm_config.h"

LOG_MODULE_REGISTER(plat_user_setting);

/* The largest id is the temp threshold array, written in one EEPROM write */
#define PERM_VALUE_MAX_SIZE (TEMP_THRESHOLD_COUNT * sizeof(int32_t))
#define PERM_VALUE_UNSET 0xFF
#define EEPROM_MAX_WRITE_TIME 5 // the BR24G512 eeprom max write time is 3.5 ms

BUILD_ASSERT(PERM_VALUE_MAX_SIZE <= EEPROM_WRITE_SIZE, "perm value exceeds one EEPROM write");

typedef struct {
	const char *name;
	uint16_t offset; /* log EEPROM offset */
	uint8_t size; /* value size in bytes, up to PERM_VALUE_MAX_SIZE */
} plat_perm_layout_t;

static const plat_perm_layout_t perm_layout[PERM_ID_MAX] = {
	[PERM_ID_THERMALTRIP] = { "THERMALTRIP_SWITCH", 0x8000, 1 },
	/* int32_t array, one element per temp threshold table entry */
	[PERM_ID_TEMP_THRESHOLD] = { "TEMP_THRESHOLD", 0x8100,
				     TEMP_THRESHOLD_COUNT * sizeof(int32_t) },
};

static const plat_perm_layout_t *perm_find(uint8_t id, uint8_t len)
{
	if (id == PERM_ID_NONE || id >= PERM_ID_MAX)
		return NULL;

	if (perm_layout[id].size != len)
		return NULL;

	return &perm_layout[id];
}

static bool perm_is_unset(const uint8_t *buf, uint8_t len)
{
	for (uint8_t i = 0; i < len; i++) {
		if (buf[i] != PERM_VALUE_UNSET)
			return false;
	}

	return true;
}

/* The EEPROM NACKs every access during its internal write cycle, wait it out before returning */
static bool perm_write(uint16_t offset, const void *val, uint8_t len)
{
	bool ret = plat_eeprom_write(offset, (uint8_t *)val, len);
	k_msleep(EEPROM_MAX_WRITE_TIME);
	return ret;
}

bool plat_perm_set(uint8_t id, const void *val, uint8_t len)
{
	CHECK_NULL_ARG_WITH_RETURN(val, false);

	const plat_perm_layout_t *perm = perm_find(id, len);
	if (!perm) {
		LOG_ERR("invalid perm id %d, len %d", id, len);
		return false;
	}

	/* all 0xFF reads back as "not set" */
	if (perm_is_unset(val, len)) {
		LOG_ERR("%s value can't be all 0xFF", perm->name);
		return false;
	}

	return perm_write(perm->offset, val, len);
}

/* Return 0 on success, -ENODATA if the id is not set */
int plat_perm_get(uint8_t id, void *val, uint8_t len)
{
	CHECK_NULL_ARG_WITH_RETURN(val, -EINVAL);

	const plat_perm_layout_t *perm = perm_find(id, len);
	if (!perm) {
		LOG_ERR("invalid perm id %d, len %d", id, len);
		return -EINVAL;
	}

	uint8_t buf[PERM_VALUE_MAX_SIZE];
	if (!plat_eeprom_read(perm->offset, buf, len))
		return -EIO;

	if (perm_is_unset(buf, len))
		return -ENODATA;

	memcpy(val, buf, len);
	return 0;
}

bool plat_perm_clear(void)
{
	uint8_t buf[PERM_VALUE_MAX_SIZE];
	bool ret = true;

	memset(buf, PERM_VALUE_UNSET, sizeof(buf));
	for (uint8_t id = PERM_ID_NONE + 1; id < PERM_ID_MAX; id++) {
		if (!perm_write(perm_layout[id].offset, buf, perm_layout[id].size)) {
			LOG_ERR("clear %s perm failed", perm_layout[id].name);
			ret = false;
		}
	}

	return ret;
}

const char *plat_perm_get_name(uint8_t id)
{
	if (id == PERM_ID_NONE || id >= PERM_ID_MAX)
		return NULL;

	return perm_layout[id].name;
}

uint8_t plat_perm_get_size(uint8_t id)
{
	if (id == PERM_ID_NONE || id >= PERM_ID_MAX)
		return 0;

	return perm_layout[id].size;
}

void user_settings_init(void)
{
	thermaltrip_switch_perm_restore();
	plat_temp_threshold_init();
}
