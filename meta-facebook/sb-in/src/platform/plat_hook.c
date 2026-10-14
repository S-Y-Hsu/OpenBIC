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

#include <stdio.h>
#include <string.h>
#include "libutil.h"
#include <logging/log.h>
#include "plat_hook.h"
#include "pmbus.h"
#include "plat_gpio.h"
#include "plat_cpld.h"
#include "pldm_oem.h"
#include "plat_pldm_sensor.h"
#include "mp2971.h"
#include "mp29816a.h"
#include "raa228249.h"
#include "plat_i2c_target.h"
#include "plat_thermal.h"

LOG_MODULE_REGISTER(plat_hook);

static struct k_mutex vr_mutex[VR_INDEX_MAX];

#define EEPROM_MAX_WRITE_TIME 5 // the BR24G512 eeprom max write time is 3.5 ms

/* one {mutex, page} slot per (VR chip, PMBus page) pair; stride is VR_PAGE_NUM,
 * keep in sync with the VR_INDEX_E_x * VR_PAGE_NUM [+ page] indexing in plat_pldm_sensor.c
 */
// clang-format off
#define VR_PRE_PROC_ARG_ENTRY(n)                                                                 \
	{ .mutex = vr_mutex + (n), .vr_page = 0x0 }, { .mutex = vr_mutex + (n), .vr_page = 0x1 }, \
	{ .mutex = vr_mutex + (n), .vr_page = 0x2 },

vr_pre_proc_arg vr_pre_read_args[] = {
	VR_PRE_PROC_ARG_ENTRY(0)  VR_PRE_PROC_ARG_ENTRY(1)  VR_PRE_PROC_ARG_ENTRY(2)
	VR_PRE_PROC_ARG_ENTRY(3)  VR_PRE_PROC_ARG_ENTRY(4)  VR_PRE_PROC_ARG_ENTRY(5)
	VR_PRE_PROC_ARG_ENTRY(6)  VR_PRE_PROC_ARG_ENTRY(7)  VR_PRE_PROC_ARG_ENTRY(8)
	VR_PRE_PROC_ARG_ENTRY(9)  VR_PRE_PROC_ARG_ENTRY(10) VR_PRE_PROC_ARG_ENTRY(11)
	VR_PRE_PROC_ARG_ENTRY(12)
};
// clang-format on
#undef VR_PRE_PROC_ARG_ENTRY

BUILD_ASSERT(ARRAY_SIZE(vr_pre_read_args) == VR_INDEX_MAX * VR_PAGE_NUM,
	     "vr_pre_read_args size must match VR_INDEX_MAX * VR_PAGE_NUM");

void vr_mutex_init(void)
{
	for (uint8_t i = 0; i < ARRAY_SIZE(vr_mutex); i++) {
		k_mutex_init(vr_mutex + i);
		LOG_DBG("vr_mutex[%d] %p init", i, vr_mutex + i);
	}
}

void *vr_mutex_get(enum VR_INDEX_E vr_index)
{
	if (vr_index >= VR_INDEX_MAX) {
		LOG_ERR("vr_mutex_get, invalid vr_index %d", vr_index);
		return NULL;
	}

	return vr_mutex + vr_index;
}

bool pre_vr_read(sensor_cfg *cfg, void *args)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, false);
	CHECK_NULL_ARG_WITH_RETURN(args, false);

	vr_pre_proc_arg *pre_proc_args = (vr_pre_proc_arg *)args;
	uint8_t retry = 5;
	I2C_MSG msg;

	/* mutex lock */
	if (pre_proc_args->mutex) {
		LOG_DBG("%x l %p", cfg->num, pre_proc_args->mutex);
		if (k_mutex_lock(pre_proc_args->mutex, K_MSEC(VR_MUTEX_LOCK_TIMEOUT_MS))) {
			LOG_ERR("0x%02x pre_vr_read, mutex lock fail", cfg->num);
			return false;
		}
	}

	/* set page */
	msg.bus = cfg->port;
	msg.target_addr = cfg->target_addr;
	msg.tx_len = 2;
	msg.data[0] = 0x00;
	msg.data[1] = pre_proc_args->vr_page;
	if (i2c_master_write(&msg, retry)) {
		k_mutex_unlock(pre_proc_args->mutex);
		LOG_ERR("0x%02x pre_vr_read, set page fail", cfg->num);
		return false;
	}
	return true;
}

bool post_vr_read(sensor_cfg *cfg, void *args, int *const reading)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, false);
	CHECK_NULL_ARG_WITH_RETURN(args, false);

	vr_pre_proc_arg *pre_proc_args = (vr_pre_proc_arg *)args;

	/* mutex unlock */
	if (pre_proc_args->mutex) {
		LOG_DBG("%x u %p", cfg->num, pre_proc_args->mutex);
		if (k_mutex_unlock(pre_proc_args->mutex)) {
			LOG_ERR("0x%02x post_vr_read, mutex unlock fail", cfg->num);
			return false;
		}
	}

	/* set reading val to 0 if reading val is negative */
	sensor_val tmp_reading;
	tmp_reading.integer = (int16_t)(*reading & 0xFFFF);
	tmp_reading.fraction = (int16_t)((*reading >> 16) & 0xFFFF);

	/* sensor_value = 1000 times of true value */
	int32_t sensor_value = tmp_reading.integer * 1000 + tmp_reading.fraction;

	if (sensor_value < 0) {
		LOG_DBG("Original sensor reading: integer = %d, fraction = %d (combined value * 1000: %d)",
			tmp_reading.integer, tmp_reading.fraction, sensor_value);
		*reading = 0;
		LOG_DBG("Negative sensor reading detected. Set reading to 0x%x", *reading);
	}

	return true;
}

bool is_mb_dc_on()
{
	/* MODULE_PWRGD (SGPIO)
	 * 1 -> power on
	 * 0 -> power off
	 */
	return sgpio_get(MODULE_PWRGD) == GPIO_HIGH;
}

/**************************************************************************************/
/******************************** VR_RAIL_INDEX_TABLE *********************************/
/**************************************************************************************/
//the order is following enum VR_RAIL_E; sensor_id is the rail's base (TEMP) sensor,
//see VR_RAIL_SENSOR_E for the VOLT/CURR/PWR sensors that follow it
static vr_mapping_sensor vr_rail_table[] = {
	//PU1:
	{ SENSOR_NUM_ASIC_P1V05_VDDC_HBM0145_TEMP_C, "ASIC_P1V05_VDDC_HBM0145" },
	{ SENSOR_NUM_ASIC_P0V9_OWL_W_TRVDD_TEMP_C, "ASIC_P0V9_OWL_W_TRVDD" },
	{ SENSOR_NUM_ASIC_P0V75_OWL_W_TRVDD_TEMP_C, "ASIC_P0V75_OWL_W_TRVDD" },
	//PU2:
	{ SENSOR_NUM_ASIC_P0V75_OWL_W_VDD_TEMP_C, "ASIC_P0V75_OWL_W_VDD" },
	{ SENSOR_NUM_ASIC_P0V8_MAX_S_VDD_TEMP_C, "ASIC_P0V8_MAX_S_VDD" },
	{ SENSOR_NUM_ASIC_P0V4_VDDQL_HBM0145_TEMP_C, "ASIC_P0V4_VDDQL_HBM0145" },
	//PU3:
	{ SENSOR_NUM_ASIC_P0V75_VDDPHY_HBM0145_TEMP_C, "ASIC_P0V75_VDDPHY_HBM0145" },
	{ SENSOR_NUM_ASIC_P0V9_VDDQ_HBM0145_TEMP_C, "ASIC_P0V9_VDDQ_HBM0145" },
	{ SENSOR_NUM_ASIC_P0V75_MAX_N_VDD_TEMP_C, "ASIC_P0V75_MAX_N_VDD" },
	//PU4:
	{ SENSOR_NUM_ASIC_P1V05_VDDC_HBM2367_TEMP_C, "ASIC_P1V05_VDDC_HBM2367" },
	{ SENSOR_NUM_ASIC_P0V9_OWL_E_TRVDD_TEMP_C, "ASIC_P0V9_OWL_E_TRVDD" },
	{ SENSOR_NUM_ASIC_P0V75_OWL_E_TRVDD_TEMP_C, "ASIC_P0V75_OWL_E_TRVDD" },
	//PU5:
	{ SENSOR_NUM_ASIC_P0V75_OWL_E_VDD_TEMP_C, "ASIC_P0V75_OWL_E_VDD" },
	{ SENSOR_NUM_ASIC_P0V4_VDDQL_HBM2367_TEMP_C, "ASIC_P0V4_VDDQL_HBM2367" },
	{ SENSOR_NUM_ASIC_P0V8_MAX_M_VDD_TEMP_C, "ASIC_P0V8_MAX_M_VDD" },
	//PU6:
	{ SENSOR_NUM_ASIC_P0V75_VDDPHY_HBM2367_TEMP_C, "ASIC_P0V75_VDDPHY_HBM2367" },
	{ SENSOR_NUM_ASIC_P0V9_VDDQ_HBM2367_TEMP_C, "ASIC_P0V9_VDDQ_HBM2367" },
	{ SENSOR_NUM_ASIC_P0V83_HAMSA_AVDD_PCIE_TEMP_C, "ASIC_P0V83_HAMSA_AVDD_PCIE" },
	//PU7:
	{ SENSOR_NUM_ASIC_P0V75_ZORA11_VDDL_TEMP_C, "ASIC_P0V75_ZORA11_VDDL" },
	{ SENSOR_NUM_ASIC_P0V75_ZORA11_VDDH_TEMP_C, "ASIC_P0V75_ZORA11_VDDH" },
	//PU8:
	{ SENSOR_NUM_ASIC_P0V75_ZORA10_VDDL_TEMP_C, "ASIC_P0V75_ZORA10_VDDL" },
	{ SENSOR_NUM_ASIC_P0V75_ZORA10_VDDH_TEMP_C, "ASIC_P0V75_ZORA10_VDDH" },
	//PU9:
	{ SENSOR_NUM_ASIC_P0V75_ZORA01_VDDL_TEMP_C, "ASIC_P0V75_ZORA01_VDDL" },
	{ SENSOR_NUM_ASIC_P0V75_ZORA01_VDDH_TEMP_C, "ASIC_P0V75_ZORA01_VDDH" },
	//PU10:
	{ SENSOR_NUM_ASIC_P0V75_ZORA00_VDDL_TEMP_C, "ASIC_P0V75_ZORA00_VDDL" },
	{ SENSOR_NUM_ASIC_P0V75_ZORA00_VDDH_TEMP_C, "ASIC_P0V75_ZORA00_VDDH" },
	//PU11:
	{ SENSOR_NUM_ASIC_P1V8_VPP_HBM0145_TEMP_C, "ASIC_P1V8_VPP_HBM0145" },
	{ SENSOR_NUM_ASIC_P1V8_VPP_HBM2367_TEMP_C, "ASIC_P1V8_VPP_HBM2367" },
	{ SENSOR_NUM_ASIC_P1V8_TEMP_C, "ASIC_P1V8" },
	//PU624:
	{ SENSOR_NUM_ASIC_P0V75_MAX_EW2_VDD_TEMP_C, "ASIC_P0V75_MAX_EW2_VDD" },
	//PU626:
	{ SENSOR_NUM_ASIC_P0V75_MAX_EW1_VDD_TEMP_C, "ASIC_P0V75_MAX_EW1_VDD" },
	{ SENSOR_NUM_ASIC_P0V85_HAMSA_VDD_TEMP_C, "ASIC_P0V85_HAMSA_VDD" },
};
BUILD_ASSERT(ARRAY_SIZE(vr_rail_table) == VR_RAIL_E_MAX, "vr_rail_table must cover VR_RAIL_E");

bool vr_rail_name_get(uint8_t rail, uint8_t **name)
{
	CHECK_NULL_ARG_WITH_RETURN(name, false);

	if (rail >= VR_RAIL_E_MAX) {
		*name = NULL;
		return false;
	}

	*name = (uint8_t *)vr_rail_table[rail].sensor_name;
	return true;
}

bool vr_rail_enum_get(uint8_t *name, uint8_t *num)
{
	CHECK_NULL_ARG_WITH_RETURN(name, false);
	CHECK_NULL_ARG_WITH_RETURN(num, false);

	for (int i = 0; i < VR_RAIL_E_MAX; i++) {
		if (strcmp(name, vr_rail_table[i].sensor_name) == 0) {
			*num = i;
			return true;
		}
	}

	LOG_ERR("invalid rail name %s", name);
	return false;
}

bool vr_rail_sensor_id_get(uint8_t rail, uint8_t *sensor_id)
{
	CHECK_NULL_ARG_WITH_RETURN(sensor_id, false);

	if (rail >= VR_RAIL_E_MAX) {
		return false;
	}

	*sensor_id = vr_rail_table[rail].sensor_id;
	return true;
}

/**************************************************************************************/
/******************************* VR_INDEX_RAIL_TABLE ***********************************/
/**************************************************************************************/
// Which VR_RAIL_E pages belong to each VR_INDEX_E (IC), and which SGPIO input carries that IC's
// SMBALERT# (one alert line per IC). Rails mirror the //PU groupings in vr_rail_table above -
// each IC's rails are listed in the same order they appear there.
typedef struct _vr_index_rail_map_ {
	uint8_t smbalert_sgpio;
	uint8_t rail_count;
	uint8_t rails[MAX_RAILS_PER_IC];
} vr_index_rail_map_t;

static const vr_index_rail_map_t vr_index_rail_table[] = {
	[VR_INDEX_E_1] = { VDDC_HBM0145_SMBALERT_N,
			   3,
			   { VR_RAIL_E_ASIC_P1V05_VDDC_HBM0145, VR_RAIL_E_ASIC_P0V9_OWL_W_TRVDD,
			     VR_RAIL_E_ASIC_P0V75_OWL_W_TRVDD } },
	[VR_INDEX_E_2] = { OWL_W_SMBALRT_N,
			   3,
			   { VR_RAIL_E_ASIC_P0V75_OWL_W_VDD, VR_RAIL_E_ASIC_P0V8_MAX_S_VDD,
			     VR_RAIL_E_ASIC_P0V4_VDDQL_HBM0145 } },
	[VR_INDEX_E_3] = { VDDPHY_HBM0145_SMBALERT_N,
			   3,
			   { VR_RAIL_E_ASIC_P0V75_VDDPHY_HBM0145, VR_RAIL_E_ASIC_P0V9_VDDQ_HBM0145,
			     VR_RAIL_E_ASIC_P0V75_MAX_N_VDD } },
	[VR_INDEX_E_4] = { VDDC_HBM2367_SMBALERT_N,
			   3,
			   { VR_RAIL_E_ASIC_P1V05_VDDC_HBM2367, VR_RAIL_E_ASIC_P0V9_OWL_E_TRVDD,
			     VR_RAIL_E_ASIC_P0V75_OWL_E_TRVDD } },
	[VR_INDEX_E_5] = { OWL_E_SMBALRT_N,
			   3,
			   { VR_RAIL_E_ASIC_P0V75_OWL_E_VDD, VR_RAIL_E_ASIC_P0V4_VDDQL_HBM2367,
			     VR_RAIL_E_ASIC_P0V8_MAX_M_VDD } },
	[VR_INDEX_E_6] = { VDDPHY_HBM2367_SMBALERT_N,
			   3,
			   { VR_RAIL_E_ASIC_P0V75_VDDPHY_HBM2367, VR_RAIL_E_ASIC_P0V9_VDDQ_HBM2367,
			     VR_RAIL_E_ASIC_P0V83_HAMSA_AVDD_PCIE } },
	[VR_INDEX_E_7] = { ZORA11_VDD_SMBALERT_N,
			   2,
			   { VR_RAIL_E_ASIC_P0V75_ZORA11_VDDL, VR_RAIL_E_ASIC_P0V75_ZORA11_VDDH } },
	[VR_INDEX_E_8] = { ZORA10_VDD_SMBALERT_N,
			   2,
			   { VR_RAIL_E_ASIC_P0V75_ZORA10_VDDL, VR_RAIL_E_ASIC_P0V75_ZORA10_VDDH } },
	[VR_INDEX_E_9] = { ZORA01_VDD_SMBALERT_N,
			   2,
			   { VR_RAIL_E_ASIC_P0V75_ZORA01_VDDL, VR_RAIL_E_ASIC_P0V75_ZORA01_VDDH } },
	[VR_INDEX_E_10] = { ZORA00_VDD_SMBALERT_N,
			    2,
			    { VR_RAIL_E_ASIC_P0V75_ZORA00_VDDL,
			      VR_RAIL_E_ASIC_P0V75_ZORA00_VDDH } },
	[VR_INDEX_E_11] = { VPP_HBM_P1V8_SMBALERT_N,
			    3,
			    { VR_RAIL_E_ASIC_P1V8_VPP_HBM0145, VR_RAIL_E_ASIC_P1V8_VPP_HBM2367,
			      VR_RAIL_E_ASIC_P1V8 } },
	[VR_INDEX_E_12] = { MAX_EW2_VDD_SMBALERT_N, 1, { VR_RAIL_E_ASIC_P0V75_MAX_EW2_VDD } },
	[VR_INDEX_E_13] = { MAX_EW1_VDD_SMBALERT_N,
			    2,
			    { VR_RAIL_E_ASIC_P0V75_MAX_EW1_VDD, VR_RAIL_E_ASIC_P0V85_HAMSA_VDD } },
};

bool vr_index_get_rails(uint8_t vr_index, const uint8_t **rails, uint8_t *count)
{
	CHECK_NULL_ARG_WITH_RETURN(rails, false);
	CHECK_NULL_ARG_WITH_RETURN(count, false);

	if (vr_index >= VR_INDEX_MAX) {
		return false;
	}

	*rails = vr_index_rail_table[vr_index].rails;
	*count = vr_index_rail_table[vr_index].rail_count;
	return true;
}

bool vr_index_get_smbalert_sgpio(uint8_t vr_index, uint8_t *sgpio_num)
{
	CHECK_NULL_ARG_WITH_RETURN(sgpio_num, false);

	if (vr_index >= VR_INDEX_MAX) {
		return false;
	}

	*sgpio_num = vr_index_rail_table[vr_index].smbalert_sgpio;
	return true;
}

/**************************************************************************************/
/********************************* SGPIO_EVENT_TABLE **********************************/
/**************************************************************************************/
// SGPIO inputs that only need an assert/deassert blackbox log: no error data and no action on
// change. They all share ISR_SGPIO_EVENT and log as SGPIO_EVENT_TRIGGER_CAUSE with the SGPIO
// number as payload. An input that needs error data or an action gets its own cause instead.
typedef struct _sgpio_event_map_ {
	uint8_t sgpio_num;
	uint8_t active_level; // GPIO_LOW / GPIO_HIGH
	uint8_t event_source; // astrid_event_source, ASTRID_EVENT_SOURCE_NONE (0): no BMC event
} sgpio_event_map_t;

static const sgpio_event_map_t sgpio_event_table[] = {
	{ LEAK1_DETECT_ALERT_CPLD_N, GPIO_LOW, ASTRID_EVENT_SOURCE_NONE },
	// ASIC CATTRIP, latched by the CPLD (active low, same as CPLD reg 0x27)
	{ ZORA00_HBM_CATTRIP_ALARM, GPIO_LOW, ASTRID_ZORA00_HBM_CATTRIP },
	{ ZORA01_HBM_CATTRIP_ALARM, GPIO_LOW, ASTRID_ZORA01_HBM_CATTRIP },
	{ ZORA10_HBM_CATTRIP_ALARM, GPIO_LOW, ASTRID_ZORA10_HBM_CATTRIP },
	{ ZORA11_HBM_CATTRIP_ALARM, GPIO_LOW, ASTRID_ZORA11_HBM_CATTRIP },
	{ OWL_E_SOC_CATTRIP_ALARM, GPIO_LOW, ASTRID_OWL_E_SOC_CATTRIP },
	{ OWL_W_SOC_CATTRIP_ALARM, GPIO_LOW, ASTRID_OWL_W_SOC_CATTRIP },
	{ HAMSA_CATTRIP_ALARM, GPIO_LOW, ASTRID_HAMSA_CATTRIP },
	// System alert fault (same as CPLD reg 0x28 bit7)
	{ FM_MODULE_PWRBRK_R_N, GPIO_LOW, ASTRID_EVENT_SOURCE_NONE },
	// ASIC temp over (same as CPLD reg 0x29 bit7~3)
	{ FM_ASIC_0_THERMTRIP_N_LATCH, GPIO_LOW, ASTRID_ASIC_THERMTRIP },
	{ TEMP_MON2_OVERT_N, GPIO_LOW, ASTRID_TEMP_MON2_OVERT_N },
	{ TEMP_MON3_OVERT_N, GPIO_LOW, ASTRID_TEMP_MON3_OVERT_N },
	{ TEMP_MON4_OVERT_N, GPIO_LOW, ASTRID_TEMP_MON4_OVERT_N },
	{ TEMP_MON5_OVERT_N, GPIO_LOW, ASTRID_TEMP_MON5_OVERT_N },
	// Temperature IC OVERT fault (same as CPLD reg 0x2A bit7~5)
	{ IRQ_TMP75_1_ALERT_R_N, GPIO_LOW, ASTRID_EVENT_SOURCE_NONE },
	{ IRQ_TMP75_2_ALERT_R_N, GPIO_LOW, ASTRID_EVENT_SOURCE_NONE },
	{ IRQ_TMP75_3_ALERT_R_N, GPIO_LOW, ASTRID_EVENT_SOURCE_NONE },
};

uint8_t sgpio_event_count(void)
{
	return ARRAY_SIZE(sgpio_event_table);
}

// SGPIO number, BMC event source and current active state of the idx-th event
bool sgpio_event_get_state(uint8_t idx, uint8_t *sgpio_num, uint8_t *event_source, bool *active)
{
	CHECK_NULL_ARG_WITH_RETURN(sgpio_num, false);
	CHECK_NULL_ARG_WITH_RETURN(event_source, false);
	CHECK_NULL_ARG_WITH_RETURN(active, false);

	if (idx >= ARRAY_SIZE(sgpio_event_table)) {
		return false;
	}

	*sgpio_num = sgpio_event_table[idx].sgpio_num;
	*event_source = sgpio_event_table[idx].event_source;
	*active = (sgpio_get(*sgpio_num) == sgpio_event_table[idx].active_level);
	return true;
}

// Name of an SGPIO_EVENT_TRIGGER_CAUSE payload. Only SGPIO numbers in the table are accepted, so
// a corrupt log entry cannot index sgpio_name[] out of range.
const char *sgpio_event_get_name(uint16_t sgpio_num)
{
	for (uint8_t i = 0; i < ARRAY_SIZE(sgpio_event_table); i++) {
		if (sgpio_event_table[i].sgpio_num == sgpio_num) {
			return sgpio_name[sgpio_num];
		}
	}

	return "UNKNOWN_SGPIO_EVENT";
}

/**************************************************************************************/
/********************************* POWER_FAULT_TABLE **********************************/
/**************************************************************************************/
// CPLD VR power fault registers. For each bit: the VR_RAIL_E whose PMBus status is read for the
// blackbox / BMC event (NOT_VR_RAIL if it is not a PMBus VR), the BMC event source, and the name.
// A bit with a NULL name is not a fault bit and must be ignored.
typedef struct _power_fault_bit_ {
	uint8_t rail;
	uint8_t event_source; // astrid_event_source
	const char *name;
} power_fault_bit_t;

typedef struct _power_fault_reg_ {
	uint8_t cpld_offset;
	power_fault_bit_t bits[8]; // bits[n] is bit n
} power_fault_reg_t;

// clang-format off
static const power_fault_reg_t power_fault_table[] = {
	{ VR_POWER_FAULT_1_REG,
	  {
		  { NOT_VR_RAIL, ASTRID_P5V, "PWRGD_P5V_R" }, // bit0
		  { NOT_VR_RAIL, ASTRID_LDO_IN_1V2, "PWRGD_LDO_IN_1V2_R" }, // bit1
		  { NOT_VR_RAIL, ASTRID_P3V3_CLK_1, "PWRGD_P3V3_CLK_1_R" }, // bit2
		  { NOT_VR_RAIL, ASTRID_P3V3_CLK, "PWRGD_P3V3_CLK_R" }, // bit3
		  { NOT_VR_RAIL, ASTRID_P4V2, "PWRGD_P4V2_R" }, // bit4
		  { NOT_VR_RAIL, ASTRID_P1V8_RC38108, "PWRGD_P1V8_RC38108" }, // bit5
		  { NOT_VR_RAIL, ASTRID_P3V3, "PWRGD_P3V3_R" }, // bit6
		  { NOT_VR_RAIL, ASTRID_P12V_UBC_PWRGD, "P12V_UBC_PWRGD" }, // bit7
	  } },
	{ VR_POWER_FAULT_2_REG,
	  {
		  { VR_RAIL_E_ASIC_P0V75_ZORA01_VDDL, ASTRID_ZORA01_VDDL, "PWRGD_ZORA01_VDDL" }, // bit0
		  { VR_RAIL_E_ASIC_P0V75_ZORA00_VDDL, ASTRID_ZORA00_VDDL, "PWRGD_ZORA00_VDDL" }, // bit1
		  { VR_RAIL_E_ASIC_P0V75_ZORA11_VDDH, ASTRID_ZORA11_VDDH, "PWRGD_ZORA11_VDDH" }, // bit2
		  { VR_RAIL_E_ASIC_P0V75_ZORA10_VDDH, ASTRID_ZORA10_VDDH, "PWRGD_ZORA10_VDDH" }, // bit3
		  { VR_RAIL_E_ASIC_P0V75_ZORA01_VDDH, ASTRID_ZORA01_VDDH, "PWRGD_ZORA01_VDDH" }, // bit4
		  { VR_RAIL_E_ASIC_P0V75_ZORA00_VDDH, ASTRID_ZORA00_VDDH, "PWRGD_ZORA00_VDDH" }, // bit5
		  { NOT_VR_RAIL, ASTRID_P0V75_AVDD_HCSL, "PWRGD_P0V75_AVDD_HCSL_R" }, // bit6
		  { VR_RAIL_E_ASIC_P1V8, ASTRID_P1V8, "PWRGD_P1V8_R" }, // bit7
	  } },
	{ VR_POWER_FAULT_3_REG,
	  {
		  { VR_RAIL_E_ASIC_P0V8_MAX_M_VDD, ASTRID_MAX_M_VDD, "PWRGD_MAX_M_VDD_R" }, // bit0
		  { VR_RAIL_E_ASIC_P0V75_MAX_EW2_VDD, ASTRID_MAX_EW2_VDD, "PWRGD_MAX_EW2_VDD_R" }, // bit1
		  { VR_RAIL_E_ASIC_P0V75_MAX_N_VDD, ASTRID_MAX_N_VDD, "PWRGD_MAX_N_VDD_R" }, // bit2
		  { VR_RAIL_E_ASIC_P0V75_OWL_W_VDD, ASTRID_OWL_W_VDD, "PWRGD_OWL_W_VDD_R" }, // bit3
		  { VR_RAIL_E_ASIC_P0V75_OWL_E_VDD, ASTRID_OWL_E_VDD, "PWRGD_OWL_E_VDD_R" }, // bit4
		  { VR_RAIL_E_ASIC_P0V85_HAMSA_VDD, ASTRID_HAMSA_VDD, "PWRGD_HAMSA_VDD_R" }, // bit5
		  { VR_RAIL_E_ASIC_P0V75_ZORA11_VDDL, ASTRID_ZORA11_VDDL, "PWRGD_ZORA11_VDDL" }, // bit6
		  { VR_RAIL_E_ASIC_P0V75_ZORA10_VDDL, ASTRID_ZORA10_VDDL, "PWRGD_ZORA10_VDDL" }, // bit7
	  } },
	{ VR_POWER_FAULT_4_REG,
	  {
		  { NOT_VR_RAIL, ASTRID_P1V5_PLL_VDDA_SOC, "PWRGD_P1V5_PLL_VDDA_SOC" }, // bit0
		  { NOT_VR_RAIL, ASTRID_P1V5_PLL_VDDA_OWL, "PWRGD_P1V5_PLL_VDDA_OWL" }, // bit1
		  { VR_RAIL_E_ASIC_P0V75_VDDPHY_HBM2367, ASTRID_VDDPHY_HBM2367, "PWRGD_VDDPHY_HBM2367_R" }, // bit2
		  { VR_RAIL_E_ASIC_P0V75_VDDPHY_HBM0145, ASTRID_VDDPHY_HBM0145, "PWRGD_VDDPHY_HBM0145_R" }, // bit3
		  { VR_RAIL_E_ASIC_P0V75_OWL_W_TRVDD, ASTRID_OWL_W_TRVDD0P75, "PWRGD_OWL_W_TRVDD0P75_R" }, // bit4
		  { VR_RAIL_E_ASIC_P0V75_OWL_E_TRVDD, ASTRID_OWL_E_TRVDD0P75, "PWRGD_OWL_E_TRVDD0P75_R" }, // bit5
		  { VR_RAIL_E_ASIC_P0V8_MAX_S_VDD, ASTRID_MAX_S_VDD, "PWRGD_MAX_S_VDD_R" }, // bit6
		  { VR_RAIL_E_ASIC_P0V75_MAX_EW1_VDD, ASTRID_MAX_EW1_VDD, "PWRGD_MAX_EW1_VDD_R" }, // bit7
	  } },
	{ VR_POWER_FAULT_5_REG,
	  {
		  { VR_RAIL_E_ASIC_P0V9_VDDQ_HBM0145, ASTRID_VDDQ_HBM0145, "PWRGD_VDDQ_HBM0145_R" }, // bit0
		  { VR_RAIL_E_ASIC_P0V9_VDDQ_HBM2367, ASTRID_VDDQ_HBM2367, "PWRGD_VDDQ_HBM2367_R" }, // bit1
		  { VR_RAIL_E_ASIC_P1V05_VDDC_HBM0145, ASTRID_VDDC_HBM0145, "PWRGD_VDDC_HBM0145_R" }, // bit2
		  { VR_RAIL_E_ASIC_P1V05_VDDC_HBM2367, ASTRID_VDDC_HBM2367, "PWRGD_VDDC_HBM2367_R" }, // bit3
		  { VR_RAIL_E_ASIC_P1V8_VPP_HBM2367, ASTRID_VPP_HBM2367, "PWRGD_VPP_HBM2367_R" }, // bit4
		  { VR_RAIL_E_ASIC_P1V8_VPP_HBM0145, ASTRID_VPP_HBM0145, "PWRGD_VPP_HBM0145_R" }, // bit5
		  { NOT_VR_RAIL, ASTRID_P1V2_PLL_VDDA_OWL_W, "PWRGD_P1V2_PLL_VDDA_OWL_W" }, // bit6
		  { NOT_VR_RAIL, ASTRID_P1V2_PLL_VDDA_SOC, "PWRGD_P1V2_PLL_VDDA_SOC" }, // bit7
	  } },
	{ VR_POWER_FAULT_6_REG,
	  {
		  { NOT_VR_RAIL, ASTRID_PVDD1P5, "PWRGD_PVDD1P5" }, // bit0
		  { NOT_VR_RAIL, ASTRID_P0V9_OWL_W_PVDD, "PWRGD_P0V9_OWL_W_PVDD" }, // bit1
		  { NOT_VR_RAIL, ASTRID_P0V9_OWL_E_PVDD, "PWRGD_P0V9_OWL_E_PVDD" }, // bit2
		  { VR_RAIL_E_ASIC_P0V9_OWL_W_TRVDD, ASTRID_OWL_W_TRVDD0P9, "PWRGD_OWL_W_TRVDD0P9_R" }, // bit3
		  { VR_RAIL_E_ASIC_P0V9_OWL_E_TRVDD, ASTRID_OWL_E_TRVDD0P9, "PWRGD_OWL_E_TRVDD0P9_R" }, // bit4
		  { VR_RAIL_E_ASIC_P0V83_HAMSA_AVDD_PCIE, ASTRID_HAMSA_AVDD_PCIE, "PWRGD_HAMSA_AVDD_PCIE_R" }, // bit5
		  { VR_RAIL_E_ASIC_P0V4_VDDQL_HBM0145, ASTRID_VDDQL_HBM0145, "PWRGD_VDDQL_HBM0145_R" }, // bit6
		  { VR_RAIL_E_ASIC_P0V4_VDDQL_HBM2367, ASTRID_VDDQL_HBM2367, "PWRGD_VDDQL_HBM2367_R" }, // bit7
	  } },
	{ VR_POWER_FAULT_7_REG,
	  {
		  { NOT_VR_RAIL, ASTRID_EVENT_SOURCE_NONE, NULL }, // bit0: tied to 1'b1
		  { NOT_VR_RAIL, ASTRID_EVENT_SOURCE_NONE, NULL }, // bit1: tied to 1'b0
		  { NOT_VR_RAIL, ASTRID_HAMSA_VDDHRXTX_PCIE3, "PWRGD_HAMSA_VDDHRXTX_PCIE3_R" }, // bit2
		  { NOT_VR_RAIL, ASTRID_HAMSA_VDDHRXTX_PCIE2, "PWRGD_HAMSA_VDDHRXTX_PCIE2_R" }, // bit3
		  { NOT_VR_RAIL, ASTRID_HAMSA_VDDHRXTX_PCIE1, "PWRGD_HAMSA_VDDHRXTX_PCIE1_R" }, // bit4
		  { NOT_VR_RAIL, ASTRID_HAMSA_VDDHRXTX_PCIE0, "PWRGD_HAMSA_VDDHRXTX_PCIE0_R" }, // bit5
		  { NOT_VR_RAIL, ASTRID_P1V5_W_RVDD, "PWRGD_P1V5_W_RVDD" }, // bit6
		  { NOT_VR_RAIL, ASTRID_P1V5_E_RVDD, "PWRGD_P1V5_E_RVDD" }, // bit7
	  } },
};
// clang-format on

static const power_fault_reg_t *power_fault_find_reg(uint8_t cpld_offset)
{
	for (uint8_t i = 0; i < ARRAY_SIZE(power_fault_table); i++) {
		if (power_fault_table[i].cpld_offset == cpld_offset) {
			return &power_fault_table[i];
		}
	}

	return NULL;
}

// Rail and BMC event source of one power fault bit. Returns false if it is not a fault bit.
bool power_fault_get_bit(uint8_t cpld_offset, uint8_t bit, uint8_t *rail, uint8_t *event_source)
{
	CHECK_NULL_ARG_WITH_RETURN(rail, false);
	CHECK_NULL_ARG_WITH_RETURN(event_source, false);

	const power_fault_reg_t *reg = power_fault_find_reg(cpld_offset);
	if (!reg || bit >= 8 || !reg->bits[bit].name) {
		return false;
	}

	*rail = reg->bits[bit].rail;
	*event_source = reg->bits[bit].event_source;
	return true;
}

const char *power_fault_get_bit_name(uint8_t cpld_offset, uint8_t bit)
{
	const power_fault_reg_t *reg = power_fault_find_reg(cpld_offset);
	if (!reg || bit >= 8 || !reg->bits[bit].name) {
		return "UNKNOWN_POWER_FAULT_BIT";
	}

	return reg->bits[bit].name;
}

/**************************************************************************************/
/********************************** VR_STATUS_TABLE ***********************************/
/**************************************************************************************/
static vr_mapping_status vr_status_table[] = {
	{ PMBUS_STATUS_BYTE, "STATUS_BYTE" },
	{ PMBUS_STATUS_WORD, "STATUS_WORD" },
	{ PMBUS_STATUS_VOUT, "STATUS_VOUT" },
	{ PMBUS_STATUS_IOUT, "STATUS_IOUT" },
	{ PMBUS_STATUS_INPUT, "STATUS_INPUT" },
	{ PMBUS_STATUS_TEMPERATURE, "STATUS_TEMPERATURE" },
	{ PMBUS_STATUS_CML, "STATUS_CML_PMBUS" },
};

bool vr_status_name_get(uint8_t rail, uint8_t **name)
{
	CHECK_NULL_ARG_WITH_RETURN(name, false);

	if (rail >= VR_STAUS_E_MAX) {
		*name = NULL;
		return false;
	}

	*name = (uint8_t *)vr_status_table[rail].vr_status_name;
	return true;
}

bool vr_status_pmbus_reg_get(uint8_t vr_status_rail, uint8_t *pmbus_reg)
{
	CHECK_NULL_ARG_WITH_RETURN(pmbus_reg, false);

	if (vr_status_rail >= VR_STAUS_E_MAX) {
		return false;
	}

	*pmbus_reg = (uint8_t)vr_status_table[vr_status_rail].pmbus_reg;
	return true;
}

bool vr_status_enum_get(uint8_t *name, uint8_t *num)
{
	CHECK_NULL_ARG_WITH_RETURN(name, false);
	CHECK_NULL_ARG_WITH_RETURN(num, false);

	for (int i = 0; i < VR_STAUS_E_MAX; i++) {
		if (strcmp(name, vr_status_table[i].vr_status_name) == 0) {
			*num = i;
			return true;
		}
	}

	LOG_ERR("invalid vr status name %s", name);
	return false;
}

bool plat_get_vr_status(uint8_t rail, uint8_t vr_status_rail, uint16_t *vr_status)
{
	CHECK_NULL_ARG_WITH_RETURN(vr_status, false);

	bool ret = false;
	uint8_t sensor_id;
	if (!vr_rail_sensor_id_get(rail, &sensor_id)) {
		LOG_ERR("invalid vr rail %d", rail);
		return false;
	}
	sensor_cfg *cfg = get_sensor_cfg_by_sensor_id(sensor_id);
	CHECK_NULL_ARG_WITH_RETURN(cfg, ret);

	if ((cfg->pre_sensor_read_hook)) {
		if ((cfg->pre_sensor_read_hook)(cfg, cfg->pre_sensor_read_args) == false) {
			LOG_DBG("%d read vr status pre hook fail!", sensor_id);
			return false;
		}
	};

	vr_pre_proc_arg *pre_proc_args = (vr_pre_proc_arg *)cfg->pre_sensor_read_args;

	uint16_t pmbus_reg_id = vr_status_table[vr_status_rail].pmbus_reg;

	switch (cfg->type) {
	case sensor_dev_mp2971:
		if (!mp2971_get_vr_status(cfg, pre_proc_args->vr_page, pmbus_reg_id, vr_status)) {
			LOG_ERR("The VR MPS2971 vr status reading failed");
			goto err;
		}
		break;
	case sensor_dev_mp29816a:
		if (!mp29816a_get_vr_status(cfg, pre_proc_args->vr_page, pmbus_reg_id, vr_status)) {
			LOG_ERR("The VR MPS29816a vr status reading failed");
			goto err;
		}
		break;
	case sensor_dev_raa228249:
		if (!raa228249_get_vr_status(cfg, pre_proc_args->vr_page, pmbus_reg_id,
					     vr_status)) {
			LOG_ERR("The VR RAA228249 vr status reading failed");
			goto err;
		}
		break;
	default:
		LOG_ERR("Unsupport VR type(%x)", cfg->type);
		goto err;
	}

	ret = true;
err:
	if (cfg->post_sensor_read_hook) {
		if (cfg->post_sensor_read_hook(cfg, cfg->post_sensor_read_args, NULL) == false) {
			LOG_ERR("%d read vr status post hook fail!", sensor_id);
		}
	}
	return ret;
}

bool plat_clear_vr_status(uint8_t rail)
{
	bool ret = false;
	uint8_t sensor_id;
	if (!vr_rail_sensor_id_get(rail, &sensor_id)) {
		LOG_ERR("invalid vr rail %d", rail);
		return false;
	}
	sensor_cfg *cfg = get_sensor_cfg_by_sensor_id(sensor_id);

	if (cfg == NULL) {
		LOG_ERR("Failed to get sensor config for sensor 0x%x", sensor_id);
		return false;
	}

	vr_pre_proc_arg *pre_proc_args = (vr_pre_proc_arg *)cfg->pre_sensor_read_args;

	if (cfg->pre_sensor_read_hook) {
		if (!cfg->pre_sensor_read_hook(cfg, cfg->pre_sensor_read_args)) {
			LOG_ERR("%d clear vr status pre hook fail!", sensor_id);
			goto err;
		}
	}

	switch (cfg->type) {
	case sensor_dev_mp2971:
		if (!mp2971_clear_vr_status(cfg, pre_proc_args->vr_page)) {
			LOG_ERR("The VR MPS2971 vr status clear failed");
			goto err;
		}
		break;
	case sensor_dev_mp29816a:
		if (!mp29816a_clear_vr_status(cfg, pre_proc_args->vr_page)) {
			LOG_ERR("The VR MPS29816a vr status clear failed");
			goto err;
		}
		break;
	case sensor_dev_raa228249:
		if (!raa228249_clear_vr_status(cfg, pre_proc_args->vr_page)) {
			LOG_ERR("The VR RAA228249 vr status clear failed");
			goto err;
		}
		break;
	default:
		LOG_ERR("Unsupport VR type(%x)", cfg->type);
		goto err;
	}

	ret = true;
err:
	if (cfg->post_sensor_read_hook) {
		if (cfg->post_sensor_read_hook(cfg, cfg->post_sensor_read_args, NULL) == false) {
			LOG_ERR("%d clear vr status post hook fail!", sensor_id);
		}
	}
	return ret;
}
/**************************************************************************************/

bool post_sensor_reading_hook_func(uint8_t sensor_number)
{
	update_sensor_reading_by_sensor_number(sensor_number);
	return true;
}

bool post_common_sensor_read(sensor_cfg *cfg, void *args, int *const reading)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, false);
	ARG_UNUSED(args);

	uint8_t sensor_number = cfg->num;
	if (!post_sensor_reading_hook_func(sensor_number))
		return false;
	return true;
}

// ASIC TEMP: report open circuit found by the thermal alert scan. The open status register
// clears on read, so only the scan reads it; this hook just checks its flag.
bool post_tmp_read(sensor_cfg *cfg, void *args, int *const reading)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, false);

	if (!post_common_sensor_read(cfg, args, reading))
		return false;

	if (reading && plat_thermal_is_open(cfg->num)) {
		cfg->cache_status = SENSOR_OPEN_CIRCUIT;
		return false;
	}
	return true;
}