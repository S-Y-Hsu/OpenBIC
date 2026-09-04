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

#ifndef XDPE1E3G6A_H
#define XDPE1E3G6A_H

#include "stdint.h"
#include "sensor.h"

enum XDPE1E3G6A_WRITE_PROTECT_OPTIONAL {
	XDPE1E3G6A_ENABLE_WRITE_PROTECT,
	XDPE1E3G6A_DISABLE_WRITE_PROTECT,
};

enum XDPE1E3G6A_WRITE_PROTECT_REG_VAL {
	XDPE1E3G6A_DISABLE_WRITE_PROTECT_VAL = 0x00,
	// Disable all writes except the WRITE_PROTECT, OPERATION, PAGE, ON_OFF_CONFIG, and VOUT_COMMAND commands
	XDPE1E3G6A_DISABLE_ALL_WRITE_EXCEPT_FIVE_COMMANDS_VAL = 0x20,
	// Disable all writes except the WRITE_PROTECT, OPERATION, and PAGE commands
	XDPE1E3G6A_DISABLE_ALL_WRITE_EXCEPT_THREE_COMMANDS_VAL = 0x40,
	// Disable all writes except the WRITE_PROTECT command
	XDPE1E3G6A_DISABLE_ALL_WRITE_EXCEPT_WRITE_PROTECT_VAL = 0x80,
};

// STATUS_BYTE (78h) bit definitions
enum XDPE1E3G6A_STATUS_BIT {
	XDPE1E3G6A_NONE_OF_ABOVE = BIT(0),
	XDPE1E3G6A_COMMUNICATION_MEMORY_LOGIC_BIT = BIT(1),
	XDPE1E3G6A_TEMPERATURE_FAULT_BIT = BIT(2),
	XDPE1E3G6A_UNDER_VOLTAGE_FAULT_BIT = BIT(3),
	XDPE1E3G6A_OVER_CURRENT_FAULT_BIT = BIT(4),
	XDPE1E3G6A_OVER_VOLTAGE_FAULT_BIT = BIT(5),
	XDPE1E3G6A_OUTPUT_OFF_BIT = BIT(6),
	XDPE1E3G6A_BUSY_BIT = BIT(7),
};

// PAGE (00h) loop select, Page 0 = LoopA, Page 1 = LoopB, Page 2 = LoopC
enum XDPE1E3G6A_LOOP {
	XDPE1E3G6A_LOOP_A = 0x00,
	XDPE1E3G6A_LOOP_B = 0x01,
	XDPE1E3G6A_LOOP_C = 0x02,
};

bool xdpe1e3g6a_set_write_protect(uint8_t bus, uint8_t addr, uint8_t option);
void xdpe1e3g6a_set_write_protect_default_val(uint8_t val);
bool xdpe1e3g6a_get_checksum(uint8_t bus, uint8_t addr, uint8_t *checksum);
bool xdpe1e3g6a_get_status_byte(uint8_t bus, uint8_t addr, uint8_t *data);
bool xdpe1e3g6a_get_vout_max(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt);
bool xdpe1e3g6a_get_vout_min(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt);
bool xdpe1e3g6a_set_vout_max(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt);
bool xdpe1e3g6a_set_vout_min(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt);
bool xdpe1e3g6a_get_config_checksum(uint8_t bus, uint8_t addr, uint32_t *checksum);
bool xdpe1e3g6a_fw_reset(uint8_t bus, uint8_t addr);
bool xdpe1e3g6a_fwupdate(uint8_t bus, uint8_t addr, uint8_t *img_buff, uint32_t img_size);
uint8_t xdpe1e3g6a_init(sensor_cfg *cfg);

#endif
