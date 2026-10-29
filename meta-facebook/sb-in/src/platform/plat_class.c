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
#include <logging/log.h>
#include <shell/shell.h>

#include "plat_class.h"
#include "plat_cpld.h"
#include "plat_pldm_sensor.h"
#include "plat_fru.h"
#include "plat_i2c.h"
#include "plat_util.h"
#include "plat_cpld.h"
#include "plat_mctp.h"
#include "plat_user_setting.h"
#include "plat_ioexp.h"

#define BOARD_TYPE_MASK (BIT(0) | BIT(1))
#define REV_ID_MASK GENMASK(6, 4) // PCA6416A_U753 P0 bit4~6

#define I2C_BUS_TMP I2C_BUS3

LOG_MODULE_REGISTER(plat_class);

static uint8_t vr_module = VR_MODULE_UNKNOWN;
static uint8_t ubc_module = UBC_MODULE_UNKNOWN;
static uint8_t tmp_module = TMP_MODULE_TYPE_UNKNOWN;
static uint8_t vr_vendor_module = VENDOR_TYPE_UNKNOWN;
static uint8_t vr_vendor_raw = 0xFF; // raw TYPE[3:0] kept for debug print, 0xFF = read failed
static uint8_t mmc_slot = 0;
static uint8_t asic_board_id = ASIC_BOARD_ID_UNKNOWN;
static uint8_t tray_location = 0;
static uint8_t board_rev_id = REV_ID_UNKNOWN;

// clang-format off
const char *asic_board_id_name[] = {
	"INGRID_LPD",
	"INGRID_PT",
	"PTTV",
	"EVB",
	"ASIC_BOARD_ID_UNKNOWN",
};

const char *board_rev_id_name[] = {
	"POC1",
	"POC2",
	"EVT1",
	"EVT2",
	"DVT",
	"PVT",
	"MP",
	"TBD",
	"REV_ID_UNKNOWN",
};

const char *tmp_module_name[] = {
	"TMP_TMP75_TMP432",
	"TMP_TMP75_EMC1413",
	"TMP_MODULE_TYPE_UNKNOWN",
};

const char *vr_vendor_module_name[] = {
	"FLEX_UBC_AND_MPS_VR_LPD",
	"FLEX_UBC_AND_SNI_VR_LPD",
	"REED_UBC_AND_RNS_VR_LPD",
	"REED_UBC_AND_MPS_VR_PT",
	"LUX_UBC_AND_SNI_VR_PT",
	"REED_UBC_AND_RNS_VR_PT",
	"VENDOR_TYPE_UNKNOWN",
};

const char *vr_module_name[] = {
	"VR_MODULE_MPS",
	"VR_MODULE_SNI",
	"VR_MODULE_RNS",
	"VR_MODULE_UNKNOWN",
};

const char *ubc_module_name[] = {
	"UBC_MODULE_FLEX",
	"UBC_MODULE_REED",
	"UBC_MODULE_LUX",
	"UBC_MODULE_UNKNOWN",
};
// clang-format on

bool plat_cpld_eerprom_read(uint8_t *data, uint16_t offset, uint8_t len)
{
	CHECK_NULL_ARG_WITH_RETURN(data, false);

	memset(data, 0, 1);

	I2C_MSG i2c_msg = { 0 };
	uint8_t retry = 5;
	i2c_msg.bus = I2C_BUS11;
	i2c_msg.target_addr = CPLD_EEPROM_ADDR;
	i2c_msg.tx_len = 2;
	i2c_msg.rx_len = len;
	i2c_msg.data[0] = offset >> 8;
	i2c_msg.data[1] = offset & 0xFF;

	if (i2c_master_read(&i2c_msg, retry)) {
		LOG_ERR("Failed to read cpld fru offset 0");
		return false;
	}

	memcpy(data, i2c_msg.data, len);
	return true;
}

void init_board_type(void)
{
	uint8_t board_type_data = 0;
	//get CPLD BOARD_TYPE
	if (!plat_read_cpld(CPLD_OFFSET_ASIC_BOARD_ID, &board_type_data, 1)) {
		LOG_ERR("Failed to get CPLD BOARD_TYPE 0x%02X", CPLD_OFFSET_ASIC_BOARD_ID);
		return;
	}

	// all 4 codes of [1:0] are defined, so the masked value maps directly to the enum
	asic_board_id = board_type_data & BOARD_TYPE_MASK;
	LOG_INF("BOARD_TYPE(0x%02X) = %s", asic_board_id, asic_board_id_name[asic_board_id]);
}

void init_board_stage(void)
{
	uint8_t board_stage_data = 0;
	//get IOEXP(U753) BOARD_STAGE
	if (!pca6416a_i2c_read(PCA6416A_U753, PCA6416A_INPUT_PORT_0, &board_stage_data, 1)) {
		LOG_ERR("Failed to get BOARD_STAGE from PCA6416A_U753_INPUT_PORT_0");
		return;
	}

	// all 8 codes of [6:4] are defined, so the field maps directly to the enum
	board_rev_id = FIELD_GET(REV_ID_MASK, board_stage_data);
	LOG_INF("BOARD_STAGE(0x%02X) = %s", board_rev_id, board_rev_id_name[board_rev_id]);
}

void init_tmp_vendor_type(void)
{
	sensor_cfg *cfg = get_sensor_cfg_by_sensor_id(SENSOR_NUM_ASIC_ZORA00_TEMP_C);
	I2C_MSG i2c_msg = { 0 };
	uint8_t retry = 5;
	i2c_msg.bus = cfg->port;
	i2c_msg.target_addr = cfg->target_addr;
	i2c_msg.tx_len = 1;
	i2c_msg.rx_len = 1;
	i2c_msg.data[0] = 0xFE; //MFG ID REG

	int ret = 0;
	if (ret == i2c_master_read_without_error_log(&i2c_msg, retry)) {
		LOG_INF("TEMP_MODULD = TMP432");
		tmp_module = TMP_MODULE_TMP432;
		return;
	} else {
		LOG_INF("TEMP_MODULD = EMC1413");
		tmp_module = TMP_MODULE_EMC1413;
		return;
	}
}

void init_vr_ubc_vendor_type(void)
{
	//get IPexp(u753) VR_VENDOR_TYPE
	if (!pca6416a_i2c_read(PCA6416A_U753, PCA6416A_INPUT_PORT_0, &vr_vendor_module, 1)) {
		LOG_ERR("Failed to get VR_VENDOR_TYPE from PCA6416A_U753_INPUT_PORT_0");
	}

	vr_vendor_module &= 0x0F;
	vr_vendor_raw = vr_vendor_module;

	switch (vr_vendor_module) {
	case FLEX_UBC_AND_MPS_VR_LPD:
		ubc_module = UBC_MODULE_FLEX;
		vr_module = VR_MODULE_MPS;
		break;
	case FLEX_UBC_AND_SNI_VR_LPD:
		ubc_module = UBC_MODULE_FLEX;
		vr_module = VR_MODULE_SNI;
		break;
	case REED_UBC_AND_RNS_VR_LPD:
	case REED_UBC_AND_RNS_VR_PT:
		ubc_module = UBC_MODULE_REED;
		vr_module = VR_MODULE_RNS;
		break;
	case REED_UBC_AND_MPS_VR_PT:
		ubc_module = UBC_MODULE_REED;
		vr_module = VR_MODULE_MPS;
		break;
	case LUX_UBC_AND_SNI_VR_PT:
		ubc_module = UBC_MODULE_LUX;
		vr_module = VR_MODULE_SNI;
		break;
	default:
		LOG_ERR("Unknown VR_VENDOR_TYPE 0x%02X", vr_vendor_module);
		vr_vendor_module = VENDOR_TYPE_UNKNOWN;
		ubc_module = UBC_MODULE_UNKNOWN;
		vr_module = VR_MODULE_UNKNOWN;
		break;
	}

	/* POC1 quirk: FLEX UBC is replaced by REED on all POC1 boards (per EE),
	 * but the strap still reports FLEX. Fixed from POC2. */
	if (board_rev_id == REV_ID_POC1 && ubc_module == UBC_MODULE_FLEX) {
		LOG_WRN("POC1 quirk: UBC FLEX -> REED");
		ubc_module = UBC_MODULE_REED;
	}

	LOG_INF("VR_MODULE = %s", vr_module_name[vr_module]);
	LOG_INF("UBC_MODULE = %s", ubc_module_name[ubc_module]);
}

void init_plat_config()
{
	init_board_type();
	init_board_stage();
	init_tmp_vendor_type();
	change_tmp_sensor_cfg(asic_board_id, tmp_module, board_rev_id);
	init_vr_ubc_vendor_type();
	change_vr_sensor_cfg(asic_board_id, vr_module, board_rev_id);
	change_ubc_sensor_cfg(asic_board_id, ubc_module, board_rev_id);

	// cpld fru offset 0: slot
	plat_cpld_eerprom_read(&mmc_slot, MMC_SLOT_USER_SETTING_OFFSET, 1);
	// mmc slot 1-4 * 0x0A
	uint8_t init_plat_eid = ((get_mmc_slot() + 1) * MCTP_DEFAULT_ENDPOINT);
	plat_set_eid(init_plat_eid);
	LOG_INF("init_plat_eid: 0x%x", init_plat_eid);

	// cpld fru offset 0x3FF: tray location
	plat_cpld_eerprom_read(&tray_location, MMC_TRAY_LOCATION_OFFSET, 1);
}

uint8_t get_vr_module()
{
	return vr_module;
}

uint8_t get_ubc_module()
{
	return ubc_module;
}

uint8_t get_mmc_slot()
{
	return mmc_slot;
}

uint8_t get_asic_board_id()
{
	return asic_board_id;
}

uint8_t get_board_rev_id()
{
	return board_rev_id;
}

uint8_t get_tray_location()
{
	return tray_location;
}

#define NAME_OR_UNSUPPORTED(tbl, idx) (((idx) < ARRAY_SIZE(tbl)) ? (tbl)[(idx)] : "not supported")

void pal_show_extra_info(const struct shell *shell)
{
	shell_print(shell, "* BOARD_TYPE:    (0x%02X)%s", asic_board_id,
		    NAME_OR_UNSUPPORTED(asic_board_id_name, asic_board_id));
	shell_print(shell, "* BOARD_STAGE:   (0x%02X)%s", board_rev_id,
		    NAME_OR_UNSUPPORTED(board_rev_id_name, board_rev_id));
	shell_print(shell, "* VR_VENDOR_TYPE:(0x%02X)%s", vr_vendor_raw,
		    NAME_OR_UNSUPPORTED(vr_vendor_module_name, vr_vendor_module));
	shell_print(shell, "* UBC_TYPE:      (0x%02X)%s", ubc_module,
		    NAME_OR_UNSUPPORTED(ubc_module_name, ubc_module));
	shell_print(shell, "* VR_TYPE:       (0x%02X)%s", vr_module,
		    NAME_OR_UNSUPPORTED(vr_module_name, vr_module));
	shell_print(shell, "* TMP_TYPE:      (0x%02X)%s", tmp_module,
		    NAME_OR_UNSUPPORTED(tmp_module_name, tmp_module));
}
