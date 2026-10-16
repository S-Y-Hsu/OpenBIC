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
#include "plat_fru.h"
#include "plat_user_setting.h"

LOG_MODULE_REGISTER(plat_user_setting);

#define PERM_VALUE_MAX_SIZE 4
#define PERM_VALUE_UNSET 0xFF

typedef struct {
	const char *name;
	uint16_t offset; /* log EEPROM offset */
	uint8_t size; /* value size in bytes, up to PERM_VALUE_MAX_SIZE */
} plat_perm_layout_t;

static const plat_perm_layout_t perm_layout[PERM_ID_MAX] = {
	[PERM_ID_THERMALTRIP] = { "THERMALTRIP_SWITCH", 0x8000, 1 },
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

	return plat_eeprom_write(perm->offset, (uint8_t *)val, len);
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
		if (!plat_eeprom_write(perm_layout[id].offset, buf, perm_layout[id].size)) {
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
