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

#include <errno.h>
#include <string.h>
#include <logging/log.h>
#include "libutil.h"
#include "sensor.h"
#include "tmp431.h"
#include "emc1413.h"
#include "tmp75.h"
#include "plat_pldm_sensor.h"
#include "plat_user_setting.h"
#include "plat_temp_threshold.h"

LOG_MODULE_REGISTER(plat_temp_threshold);

typedef struct {
	const char *name;
	uint8_t sensor_num;
	uint8_t limit_type; /* TEMP_THRESHOLD_TYPE_E */
	int32_t default_mC;
} temp_threshold_item_t;

/* The table index is also the perm array index in EEPROM: append new entries at the end only */
static const temp_threshold_item_t temp_threshold_table[] = {
	{ "TOP_INLET_TEMP_LOW_LIM", SENSOR_NUM_TOP_INLET_TEMP_C, LOCAL_LOW_LIMIT, 75000 },
	{ "TOP_INLET_TEMP_HIGH_LIM", SENSOR_NUM_TOP_INLET_TEMP_C, LOCAL_HIGH_LIMIT, 85000 },
	{ "BOT_INLET_TEMP_LOW_LIM", SENSOR_NUM_BOT_INLET_TEMP_C, LOCAL_LOW_LIMIT, 75000 },
	{ "BOT_INLET_TEMP_HIGH_LIM", SENSOR_NUM_BOT_INLET_TEMP_C, LOCAL_HIGH_LIMIT, 85000 },
	{ "BOT_OUTLET_TEMP_LOW_LIM", SENSOR_NUM_BOT_OUTLET_TEMP_C, LOCAL_LOW_LIMIT, 75000 },
	{ "BOT_OUTLET_TEMP_HIGH_LIM", SENSOR_NUM_BOT_OUTLET_TEMP_C, LOCAL_HIGH_LIMIT, 85000 },
	{ "ASIC_ZORA00_TEMP_LOW_LIM", SENSOR_NUM_ASIC_ZORA00_TEMP_C, REMOTE_1_LOW_LIMIT, 0 },
	{ "ASIC_ZORA00_TEMP_HIGH_LIM", SENSOR_NUM_ASIC_ZORA00_TEMP_C, REMOTE_1_HIGH_LIMIT, 100000 },
	{ "ASIC_ZORA01_TEMP_LOW_LIM", SENSOR_NUM_ASIC_ZORA01_TEMP_C, REMOTE_2_LOW_LIMIT, 0 },
	{ "ASIC_ZORA01_TEMP_HIGH_LIM", SENSOR_NUM_ASIC_ZORA01_TEMP_C, REMOTE_2_HIGH_LIMIT, 100000 },
	{ "ASIC_OWL_W_TEMP_LOW_LIM", SENSOR_NUM_ASIC_OWL_W_TEMP_C, REMOTE_1_LOW_LIMIT, 0 },
	{ "ASIC_OWL_W_TEMP_HIGH_LIM", SENSOR_NUM_ASIC_OWL_W_TEMP_C, REMOTE_1_HIGH_LIMIT, 100000 },
	{ "ASIC_OWL_E_TEMP_LOW_LIM", SENSOR_NUM_ASIC_OWL_E_TEMP_C, REMOTE_2_LOW_LIMIT, 0 },
	{ "ASIC_OWL_E_TEMP_HIGH_LIM", SENSOR_NUM_ASIC_OWL_E_TEMP_C, REMOTE_2_HIGH_LIMIT, 100000 },
	{ "ASIC_ZORA10_SENSOR_TEMP_LOW_LIM", SENSOR_NUM_ASIC_ZORA10_SENSOR_TEMP_C,
	  REMOTE_1_LOW_LIMIT, 0 },
	{ "ASIC_ZORA10_SENSOR_TEMP_HIGH_LIM", SENSOR_NUM_ASIC_ZORA10_SENSOR_TEMP_C,
	  REMOTE_1_HIGH_LIMIT, 100000 },
	{ "ASIC_ZORA11_SENSOR_TEMP_LOW_LIM", SENSOR_NUM_ASIC_ZORA11_SENSOR_TEMP_C,
	  REMOTE_2_LOW_LIMIT, 0 },
	{ "ASIC_ZORA11_SENSOR_TEMP_HIGH_LIM", SENSOR_NUM_ASIC_ZORA11_SENSOR_TEMP_C,
	  REMOTE_2_HIGH_LIMIT, 100000 },
	{ "ASIC_HAMSA_CRM_TEMP_LOW_LIM", SENSOR_NUM_ASIC_HAMSA_CRM_TEMP_C, REMOTE_1_LOW_LIMIT, 0 },
	{ "ASIC_HAMSA_CRM_TEMP_HIGH_LIM", SENSOR_NUM_ASIC_HAMSA_CRM_TEMP_C, REMOTE_1_HIGH_LIMIT,
	  100000 },
	{ "ASIC_HAMSA_LS_TEMP_LOW_LIM", SENSOR_NUM_ASIC_HAMSA_LS_TEMP_C, REMOTE_2_LOW_LIMIT, 0 },
	{ "ASIC_HAMSA_LS_TEMP_HIGH_LIM", SENSOR_NUM_ASIC_HAMSA_LS_TEMP_C, REMOTE_2_HIGH_LIMIT,
	  100000 },
};
BUILD_ASSERT(ARRAY_SIZE(temp_threshold_table) == TEMP_THRESHOLD_COUNT,
	     "TEMP_THRESHOLD_COUNT must match temp_threshold_table");

const char *plat_temp_threshold_get_name(uint8_t idx)
{
	if (idx >= TEMP_THRESHOLD_COUNT)
		return NULL;

	return temp_threshold_table[idx].name;
}

/* Return the table index, or -1 if the name is not found */
int plat_temp_threshold_find(const char *name)
{
	CHECK_NULL_ARG_WITH_RETURN(name, -1);

	for (int i = 0; i < TEMP_THRESHOLD_COUNT; i++) {
		if (!strcmp(name, temp_threshold_table[i].name))
			return i;
	}

	return -1;
}

static sensor_cfg *item_cfg(const temp_threshold_item_t *item)
{
	sensor_cfg *cfg = get_sensor_cfg_by_sensor_id(item->sensor_num);
	if (!cfg)
		LOG_ERR("%s : sensor 0x%x cfg not found", item->name, item->sensor_num);

	return cfg;
}

bool plat_temp_threshold_get(uint8_t idx, int32_t *millidegree_celsius)
{
	CHECK_NULL_ARG_WITH_RETURN(millidegree_celsius, false);

	if (idx >= TEMP_THRESHOLD_COUNT)
		return false;

	const temp_threshold_item_t *item = &temp_threshold_table[idx];
	sensor_cfg *cfg = item_cfg(item);
	if (!cfg)
		return false;

	switch (cfg->type) {
	case sensor_dev_tmp431:
		return tmp432_get_temp_threshold(cfg, item->limit_type, millidegree_celsius);
	case sensor_dev_emc1413:
		return emc1413_get_temp_threshold(cfg, item->limit_type,
						  (uint32_t *)millidegree_celsius);
	case sensor_dev_tmp75:
		return tmp75_get_temp_threshold(cfg, item->limit_type, millidegree_celsius);
	default:
		LOG_ERR("%s : unsupported temp type 0x%x", item->name, cfg->type);
		return false;
	}
}

bool plat_temp_threshold_set(uint8_t idx, int32_t millidegree_celsius)
{
	if (idx >= TEMP_THRESHOLD_COUNT)
		return false;

	const temp_threshold_item_t *item = &temp_threshold_table[idx];
	sensor_cfg *cfg = item_cfg(item);
	if (!cfg)
		return false;

	uint32_t val = (uint32_t)millidegree_celsius;

	switch (cfg->type) {
	case sensor_dev_tmp431:
		return tmp432_set_temp_threshold(cfg, item->limit_type, &val);
	case sensor_dev_emc1413:
		return emc1413_set_temp_threshold(cfg, item->limit_type, &val);
	case sensor_dev_tmp75:
		return tmp75_set_temp_threshold(cfg, item->limit_type, &val);
	default:
		LOG_ERR("%s : unsupported temp type 0x%x", item->name, cfg->type);
		return false;
	}
}

bool plat_temp_threshold_perm_load(int32_t perm[TEMP_THRESHOLD_COUNT])
{
	CHECK_NULL_ARG_WITH_RETURN(perm, false);

	int ret =
		plat_perm_get(PERM_ID_TEMP_THRESHOLD, perm, TEMP_THRESHOLD_COUNT * sizeof(int32_t));
	if (ret == -ENODATA) {
		memset(perm, 0xFF, TEMP_THRESHOLD_COUNT * sizeof(int32_t));
		return true;
	}

	return (ret == 0);
}

bool plat_temp_threshold_perm_store(const int32_t perm[TEMP_THRESHOLD_COUNT])
{
	CHECK_NULL_ARG_WITH_RETURN(perm, false);

	return plat_perm_set(PERM_ID_TEMP_THRESHOLD, perm, TEMP_THRESHOLD_COUNT * sizeof(int32_t));
}

void plat_temp_threshold_init(void)
{
	int32_t perm[TEMP_THRESHOLD_COUNT];

	if (!plat_temp_threshold_perm_load(perm)) {
		LOG_ERR("read temp threshold perm failed, use default");
		memset(perm, 0xFF, sizeof(perm));
	}

	for (uint8_t i = 0; i < TEMP_THRESHOLD_COUNT; i++) {
		const temp_threshold_item_t *item = &temp_threshold_table[i];
		bool is_perm = (perm[i] != TEMP_THRESHOLD_PERM_UNSET);
		int32_t val = is_perm ? perm[i] : item->default_mC;

		if (!plat_temp_threshold_set(i, val)) {
			LOG_ERR("init %s = %d failed", item->name, val);
			continue;
		}

		if (is_perm)
			LOG_INF("restore %s = %d", item->name, val);
	}
}
