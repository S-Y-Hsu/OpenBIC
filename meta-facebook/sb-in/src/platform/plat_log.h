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

#ifndef PLAT_LOG_H
#define PLAT_LOG_H

#include "plat_pldm_sensor.h"

#define CPLD_REGISTER_MAX_NUM 72
#define CPLD_REGISTER_1ST_PART_START_OFFSET                                                        \
	0x00 // first part of cpld register offset from 0x00 to 0x47
#define CPLD_REGISTER_1ST_PART_NUM 72
#define FRU_LOG_SIZE sizeof(plat_err_log_mapping)

#define LOG_ASSERT 1
#define LOG_DEASSERT 0

void init_load_eeprom_log(void);

void plat_log_read(uint8_t *log_data, uint8_t cmd_size, uint16_t order);
void error_log_event(uint16_t error_code, bool log_status);
uint8_t plat_log_get_num(void);
void plat_clear_log();
void reset_error_log_event(uint8_t err_type);

typedef struct __attribute__((packed)) _plat_err_log_mapping {
	uint16_t index;
	uint16_t err_code;
	uint64_t sys_time;
	uint8_t error_data[20];
	uint8_t cpld_dump[CPLD_REGISTER_MAX_NUM];
	uint8_t reserved[24];
} plat_err_log_mapping;

/*
 * error_code layout: [15:12] trigger cause, [11:0] cause-specific payload.
 * Cause 0x0 is reserved: err_code_caches uses 0 to mark an empty slot.
 */
#define ERR_CODE_CAUSE_SHIFT 12
#define ERR_CODE_CAUSE_MASK 0x0F
#define ERR_CODE_PAYLOAD_MASK 0x0FFF
#define MAKE_ERR_CODE(cause, payload)                                                              \
	((uint16_t)((((cause)&ERR_CODE_CAUSE_MASK) << ERR_CODE_CAUSE_SHIFT) |                      \
		    ((payload)&ERR_CODE_PAYLOAD_MASK)))
#define ERR_CODE_GET_CAUSE(code) (((code) >> ERR_CODE_CAUSE_SHIFT) & ERR_CODE_CAUSE_MASK)
#define ERR_CODE_GET_PAYLOAD(code) ((code)&ERR_CODE_PAYLOAD_MASK)

enum LOG_ERROR_TRIGGER_CAUSE {
	POWER_ON_SEQUENCE_TRIGGER_CAUSE = 0x1,
	AC_ON_TRIGGER_CAUSE = 0x2,
	DC_ON_TRIGGER_CAUSE = 0x3,
	CPLD_UNEXPECTED_VAL_TRIGGER_CAUSE = 0x4, // payload: (bit << 8) | cpld_offset
	VR_SMB_ALERT_TRIGGER_CAUSE = 0x5, // payload: VR_INDEX_E
	LEAK_DETECT_TRIGGER_CAUSE = 0x6, // payload: 0 (LEAK1_DETECT_ALERT_CPLD_N)
	MAX_TRIGGER_CAUSE = 0x10, // trigger cause maximum 4 bits
};

#endif
