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

#ifndef PLAT_HOOK_H
#define PLAT_HOOK_H

#include "sensor.h"

#define VR_MUTEX_LOCK_TIMEOUT_MS 1000
#define VR_PAGE_NUM 3 // PMBus pages reserved per VR chip in vr_pre_read_args
#define MAX_RAILS_PER_IC 3
#define NOT_VR_RAIL 0xFF

#include "plat_pldm_sensor.h"

enum VR_INDEX_E {
	VR_INDEX_E_1 = 0,
	VR_INDEX_E_2,
	VR_INDEX_E_3,
	VR_INDEX_E_4,
	VR_INDEX_E_5,
	VR_INDEX_E_6,
	VR_INDEX_E_7,
	VR_INDEX_E_8,
	VR_INDEX_E_9,
	VR_INDEX_E_10,
	VR_INDEX_E_11,
	VR_INDEX_E_12,
	VR_INDEX_E_13,
	VR_INDEX_E_14, // P3V3 OSFP (EVB only)
	VR_INDEX_MAX,
};

enum VR_RAIL_E {
	//PU1
	VR_RAIL_E_ASIC_P1V05_VDDC_HBM0145 = 0,
	VR_RAIL_E_ASIC_P0V9_OWL_W_TRVDD,
	VR_RAIL_E_ASIC_P0V75_OWL_W_TRVDD,
	//PU2
	VR_RAIL_E_ASIC_P0V75_OWL_W_VDD,
	VR_RAIL_E_ASIC_P0V8_MAX_S_VDD,
	VR_RAIL_E_ASIC_P0V4_VDDQL_HBM0145,
	//PU3
	VR_RAIL_E_ASIC_P0V75_VDDPHY_HBM0145,
	VR_RAIL_E_ASIC_P0V9_VDDQ_HBM0145,
	VR_RAIL_E_ASIC_P0V75_MAX_N_VDD,
	//PU4
	VR_RAIL_E_ASIC_P1V05_VDDC_HBM2367,
	VR_RAIL_E_ASIC_P0V9_OWL_E_TRVDD,
	VR_RAIL_E_ASIC_P0V75_OWL_E_TRVDD,
	//PU5
	VR_RAIL_E_ASIC_P0V75_OWL_E_VDD,
	VR_RAIL_E_ASIC_P0V4_VDDQL_HBM2367,
	VR_RAIL_E_ASIC_P0V8_MAX_M_VDD,
	//PU6
	VR_RAIL_E_ASIC_P0V75_VDDPHY_HBM2367,
	VR_RAIL_E_ASIC_P0V9_VDDQ_HBM2367,
	VR_RAIL_E_ASIC_P0V83_HAMSA_AVDD_PCIE,
	//PU7
	VR_RAIL_E_ASIC_P0V75_ZORA11_VDDL,
	VR_RAIL_E_ASIC_P0V75_ZORA11_VDDH,
	//PU8
	VR_RAIL_E_ASIC_P0V75_ZORA10_VDDL,
	VR_RAIL_E_ASIC_P0V75_ZORA10_VDDH,
	//PU9
	VR_RAIL_E_ASIC_P0V75_ZORA01_VDDL,
	VR_RAIL_E_ASIC_P0V75_ZORA01_VDDH,
	//PU10
	VR_RAIL_E_ASIC_P0V75_ZORA00_VDDL,
	VR_RAIL_E_ASIC_P0V75_ZORA00_VDDH,
	//PU11
	VR_RAIL_E_ASIC_P1V8_VPP_HBM0145,
	VR_RAIL_E_ASIC_P1V8_VPP_HBM2367,
	VR_RAIL_E_ASIC_P1V8,
	//PU624
	VR_RAIL_E_ASIC_P0V75_MAX_EW2_VDD,
	//PU626
	VR_RAIL_E_ASIC_P0V75_MAX_EW1_VDD,
	VR_RAIL_E_ASIC_P0V85_HAMSA_VDD,
	//PU200000 (EVB only), see vr_rail_count()/vr_rail_count_without_osfp()
	VR_RAIL_E_P3V3_OSFP,
	VR_RAIL_E_MAX,
};

enum VR_STAUS_E {
	VR_STAUS_E_STATUS_BYTE = 0,
	VR_STAUS_E_STATUS_WORD,
	VR_STAUS_E_STATUS_VOUT,
	VR_STAUS_E_STATUS_IOUT,
	VR_STAUS_E_STATUS_INPUT,
	VR_STAUS_E_STATUS_TEMPERATURE,
	VR_STAUS_E_STATUS_CML,
	VR_STAUS_E_MAX,
};

typedef struct vr_mapping_status {
	uint16_t pmbus_reg;
	uint8_t *vr_status_name;
} vr_mapping_status;

typedef struct _vr_pre_proc_arg {
	void *mutex;
	uint8_t vr_page;
} vr_pre_proc_arg;

// Each VR rail owns four consecutive sensors in this order. vr_rail_table stores the base
// (TEMP) sensor id; add one of these offsets to reach the other readings of the same rail.
enum VR_RAIL_SENSOR_E {
	VR_RAIL_SENSOR_TEMP = 0,
	VR_RAIL_SENSOR_VOLT,
	VR_RAIL_SENSOR_CURR,
	VR_RAIL_SENSOR_PWR,
};

// peak_mv value meaning no voltage reading has been recorded since boot/clear
#define VR_PEAK_NONE INT32_MIN

typedef struct vr_mapping_sensor {
	uint8_t sensor_id; // base sensor of the rail, see VR_RAIL_SENSOR_E
	uint8_t *sensor_name;
	int32_t peak_mv; // highest VOLT reading in mV, VR_PEAK_NONE if none
} vr_mapping_sensor;

bool pre_vr_read(sensor_cfg *cfg, void *args);
bool post_vr_read(sensor_cfg *cfg, void *args, int *const reading);
bool is_mb_dc_on();
void *vr_mutex_get(enum VR_INDEX_E vr_index);
void vr_mutex_init(void);
uint8_t vr_rail_count(void);
uint8_t vr_rail_count_without_osfp(void);
bool vr_rail_name_get(uint8_t rail, uint8_t **name);
bool vr_rail_sensor_id_get(uint8_t rail, uint8_t *sensor_id);
bool vr_rail_peak_get(uint8_t rail, int32_t *peak_mv);
bool vr_rail_peak_clear(uint8_t rail);
bool vr_index_get_rails(uint8_t vr_index, const uint8_t **rails, uint8_t *count);
bool vr_index_get_smbalert_sgpio(uint8_t vr_index, uint8_t *sgpio_num);
uint8_t sgpio_event_count(void);
bool sgpio_event_get_state(uint8_t idx, uint8_t *sgpio_num, uint8_t *event_source, bool *active);
const char *sgpio_event_get_name(uint16_t sgpio_num);
bool power_fault_get_bit(uint8_t cpld_offset, uint8_t bit, uint8_t *rail, uint8_t *event_source);
const char *power_fault_get_bit_name(uint8_t cpld_offset, uint8_t bit);
bool vr_status_name_get(uint8_t rail, uint8_t **name);
bool vr_status_pmbus_reg_get(uint8_t vr_status_rail, uint8_t *pmbus_reg);
bool vr_rail_enum_get(uint8_t *name, uint8_t *num);
bool vr_status_enum_get(uint8_t *name, uint8_t *num);
bool plat_get_vr_status(uint8_t rail, uint8_t vr_status_rail, uint16_t *vr_status);
bool plat_clear_vr_status(uint8_t rail);
bool post_common_sensor_read(sensor_cfg *cfg, void *args, int *const reading);
bool post_tmp_read(sensor_cfg *cfg, void *args, int *const reading);

extern mp2971_init_arg mp2971_init_args[];
#endif
