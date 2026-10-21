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

#ifndef PLAT_THERMAL_H
#define PLAT_THERMAL_H

#include <zephyr.h>
#include <stdbool.h>
#include <stdint.h>

/* TMP432 / EMC1413 registers, same offsets on both parts */
#define TEMP_STATUS_REG 0x02
#define TEMP_OPEN_STATUS_REG 0x1B
#define TEMP_HIGH_LIMIT_STATUS_REG 0x35
#define TEMP_LOW_LIMIT_STATUS_REG 0x36

/* TEMP_STATUS_REG bits */
#define TEMP_STATUS_H_LIMIT BIT(4)
#define TEMP_STATUS_L_LIMIT BIT(3)
#define TEMP_STATUS_OPEN BIT(2)

/* TEMPERATURE_TRIGGER_CAUSE payload: (kind << 8) | sensor num */
enum THERMAL_ALERT_KIND {
	THERMAL_ALERT_OPEN = 0x1,
	THERMAL_ALERT_HIGH_LIMIT = 0x2,
	THERMAL_ALERT_LOW_LIMIT = 0x3,
	THERMAL_ALERT_KIND_MAX,
};

#define THERMAL_ALERT_PAYLOAD(kind, sensor_num) ((((kind)&0x0F) << 8) | ((sensor_num)&0xFF))
#define THERMAL_ALERT_GET_KIND(payload) (((payload) >> 8) & 0x0F)
#define THERMAL_ALERT_GET_SENSOR(payload) ((payload)&0xFF)

int plat_thermal_alert_scan(void);
bool plat_thermal_get_error_data(uint16_t payload, uint8_t *data);
bool plat_thermal_is_open(uint8_t sensor_num);
const char *plat_thermal_get_sensor_name(uint8_t sensor_num);
const char *plat_thermal_get_kind_name(uint8_t kind);

/* TEMP_STATUS_REG of each alert sensor taken from the last scan, no I2C access */
uint8_t plat_thermal_get_alert_count(void);
bool plat_thermal_get_alert_status(uint8_t idx, uint8_t *sensor_num, uint8_t *status);

#endif
