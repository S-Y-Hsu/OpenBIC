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
#include <stdlib.h>
#include <logging/log.h>
#include "sensor.h"
#include "libutil.h"
#include "hal_i2c.h"
#include "util_pmbus.h"
#include "pmbus.h"
#include "xdpe1e3g6a.h"

LOG_MODULE_REGISTER(xdpe1e3g6a);

#define XDPE1E3G6A_WAIT_DATA_DELAY_MS 10
#define XDPE1E3G6A_WRITE_PROTECT_DEFAULT_VAL 0xFF

// VOUT_MIN (2Bh), VOUT_MAX is already defined as PMBUS_VOUT_MAX (24h) in pmbus.h
#define XDPE1E3G6A_VOUT_MIN_REG 0x2B

// CRC_CHECKSUM (B8h), block read of the OTP configuration CRC (excludes Trim and Firmware Patches)
#define XDPE1E3G6A_CRC_CHECKSUM_REG 0xB8
#define XDPE1E3G6A_CRC_CHECKSUM_LEN 8

// MFR_AHB_ADDRESS (CEh) is the register address pointer for MFR_REG_WRITE (DEh) / MFR_REG_READ (DFh)
#define XDPE1E3G6A_MFR_AHB_ADDRESS_REG 0xCE
#define XDPE1E3G6A_MFR_REG_WRITE_REG 0xDE
#define XDPE1E3G6A_MFR_REG_READ_REG 0xDF

// MFR_FW_COMMAND_DATA (FDh) holds input/output for the MFR_FW_COMMAND (FEh) sub-command dispatch
#define XDPE1E3G6A_MFR_FW_COMMAND_DATA_REG 0xFD
#define XDPE1E3G6A_MFR_FW_COMMAND_REG 0xFE

// MFR_FW_COMMAND (FEh) byte codes, per XDPE1Exxx Family Configuration Programming Guide Table 4
enum XDPE1E3G6A_FW_CMD {
	XDPE1E3G6A_FW_CMD_STORE_CONFIG = 0x04,
	XDPE1E3G6A_FW_CMD_CONFIGURATOR_STATUS_GET = 0x05,
	XDPE1E3G6A_FW_CMD_RESTORE_CONFIG = 0x08,
	XDPE1E3G6A_FW_CMD_FW_RESET = 0x0E,
	XDPE1E3G6A_FW_CMD_OTP_CONFIG_STORE = 0x11,
	XDPE1E3G6A_FW_CMD_OTP_SECTION_INVALIDATE = 0x12,
	XDPE1E3G6A_FW_CMD_GET_CRC = 0x2D,
	XDPE1E3G6A_FW_CMD_GET_FW_ADDRESS = 0x2E,
};

// Configuration data section header codes, per Configuration Programming Guide Table 7
enum XDPE1E3G6A_CFG_HEADER_CODE {
	XDPE1E3G6A_HC_TRIM = 0x02,
	XDPE1E3G6A_HC_CONFIG = 0x04,
	XDPE1E3G6A_HC_PMBUS = 0x07,
	XDPE1E3G6A_HC_HSIF = 0x0D,
};

// Config file section tags, per Configuration Programming Guide chapter 4/5 (single-device files only)
#define XDPE1E3G6A_DATA_START_TAG "[Configuration Data]"
#define XDPE1E3G6A_DATA_END_TAG "[End Configuration Data]"
#define XDPE1E3G6A_SECTION_TAG "//XV"
#define XDPE1E3G6A_MAX_SECT_NUM 32
#define XDPE1E3G6A_MAX_SECT_DWORDS 512

struct xdpe1e3g6a_config_section {
	uint8_t header_code;
	uint8_t xvcode;
	uint8_t loop;
	uint16_t dword_cnt;
	uint32_t data[XDPE1E3G6A_MAX_SECT_DWORDS];
};

struct xdpe1e3g6a_config {
	uint8_t sect_cnt;
	struct xdpe1e3g6a_config_section section[XDPE1E3G6A_MAX_SECT_NUM];
};

static uint8_t write_protect_default_val = XDPE1E3G6A_WRITE_PROTECT_DEFAULT_VAL;

static bool xdpe1e3g6a_i2c_read(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len)
{
	CHECK_NULL_ARG_WITH_RETURN(data, false);

	memset(data, 0, len);

	I2C_MSG i2c_msg = { 0 };
	uint8_t retry = 5;
	i2c_msg.bus = bus;
	i2c_msg.target_addr = addr;
	i2c_msg.tx_len = 1;
	i2c_msg.rx_len = len;
	i2c_msg.data[0] = reg;

	if (i2c_master_read(&i2c_msg, retry)) {
		LOG_ERR("Failed to read xdpe1e3g6a, bus: %d, addr: 0x%x, reg: 0x%x", bus, addr,
			reg);
		return false;
	}

	memcpy(data, i2c_msg.data, len);
	return true;
}

static bool xdpe1e3g6a_i2c_write(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t *data,
				 uint8_t len)
{
	CHECK_NULL_ARG_WITH_RETURN(data, false);

	I2C_MSG i2c_msg = { 0 };
	uint8_t retry = 5;
	i2c_msg.bus = bus;
	i2c_msg.target_addr = addr;
	i2c_msg.tx_len = len + 1;
	i2c_msg.data[0] = reg;

	if (len > 0)
		memcpy(&i2c_msg.data[1], data, len);

	if (i2c_master_write(&i2c_msg, retry)) {
		LOG_ERR("Failed to write xdpe1e3g6a, bus: %d, addr: 0x%x, reg: 0x%x", bus, addr,
			reg);
		return false;
	}

	return true;
}

static bool xdpe1e3g6a_block_write(uint8_t bus, uint8_t addr, uint8_t reg, const uint8_t *data,
				   uint8_t len)
{
	I2C_MSG i2c_msg = { 0 };
	uint8_t retry = 5;
	i2c_msg.bus = bus;
	i2c_msg.target_addr = addr;
	i2c_msg.tx_len = len + 2;
	i2c_msg.data[0] = reg;
	i2c_msg.data[1] = len;
	memcpy(&i2c_msg.data[2], data, len);

	if (i2c_master_write(&i2c_msg, retry)) {
		LOG_ERR("Failed to block write xdpe1e3g6a, bus: %d, addr: 0x%x, reg: 0x%x", bus,
			addr, reg);
		return false;
	}

	return true;
}

static bool xdpe1e3g6a_block_read(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t *data,
				  uint8_t len)
{
	I2C_MSG i2c_msg = { 0 };
	uint8_t retry = 5;
	i2c_msg.bus = bus;
	i2c_msg.target_addr = addr;
	i2c_msg.tx_len = 1;
	i2c_msg.rx_len = len + 1; // leading byte-count prefix from the block read protocol
	i2c_msg.data[0] = reg;

	if (i2c_master_read(&i2c_msg, retry)) {
		LOG_ERR("Failed to block read xdpe1e3g6a, bus: %d, addr: 0x%x, reg: 0x%x", bus,
			addr, reg);
		return false;
	}

	memcpy(data, &i2c_msg.data[1], len);
	return true;
}

// Executes MFR_FW_COMMAND: optional 4-byte input, the command byte, then an optional 4-byte result read
static bool xdpe1e3g6a_fw_command(uint8_t bus, uint8_t addr, uint8_t cmd, const uint8_t *tx4,
				  uint8_t *rx4, uint32_t wait_ms)
{
	if (tx4 && !xdpe1e3g6a_block_write(bus, addr, XDPE1E3G6A_MFR_FW_COMMAND_DATA_REG, tx4, 4)) {
		return false;
	}

	uint8_t cmd_byte = cmd;
	if (!xdpe1e3g6a_i2c_write(bus, addr, XDPE1E3G6A_MFR_FW_COMMAND_REG, &cmd_byte, 1)) {
		return false;
	}

	if (wait_ms) {
		k_msleep(wait_ms);
	}

	if (rx4 && !xdpe1e3g6a_block_read(bus, addr, XDPE1E3G6A_MFR_FW_COMMAND_DATA_REG, rx4, 4)) {
		return false;
	}

	return true;
}

static bool xdpe1e3g6a_ahb_set_address(uint8_t bus, uint8_t addr, uint32_t ahb_addr)
{
	uint8_t data[4] = { ahb_addr & 0xFF, (ahb_addr >> 8) & 0xFF, (ahb_addr >> 16) & 0xFF,
			    (ahb_addr >> 24) & 0xFF };
	return xdpe1e3g6a_block_write(bus, addr, XDPE1E3G6A_MFR_AHB_ADDRESS_REG, data,
				      sizeof(data));
}

static bool xdpe1e3g6a_ahb_write_dword(uint8_t bus, uint8_t addr, uint32_t val)
{
	uint8_t data[4] = { val & 0xFF, (val >> 8) & 0xFF, (val >> 16) & 0xFF,
			    (val >> 24) & 0xFF };
	return xdpe1e3g6a_block_write(bus, addr, XDPE1E3G6A_MFR_REG_WRITE_REG, data, sizeof(data));
}

static bool xdpe1e3g6a_set_write_protect_reg(uint8_t bus, uint8_t addr, uint8_t optional)
{
	uint8_t set_val = 0;

	switch (optional) {
	case XDPE1E3G6A_ENABLE_WRITE_PROTECT:
		set_val = write_protect_default_val;
		break;
	case XDPE1E3G6A_DISABLE_WRITE_PROTECT:
		set_val = XDPE1E3G6A_DISABLE_WRITE_PROTECT_VAL;
		break;
	default:
		LOG_ERR("Invalid optional: 0x%x to set write protect reg", optional);
		return false;
	}

	if (!xdpe1e3g6a_i2c_write(bus, addr, PMBUS_WRITE_PROTECT, &set_val, sizeof(set_val))) {
		LOG_ERR("Set write protect register fail, bus: 0x%x, addr: 0x%x, set_val: 0x%x",
			bus, addr, set_val);
		return false;
	}

	k_msleep(XDPE1E3G6A_WAIT_DATA_DELAY_MS);

	uint8_t read_val = 0;
	if (!xdpe1e3g6a_i2c_read(bus, addr, PMBUS_WRITE_PROTECT, &read_val, sizeof(read_val))) {
		LOG_ERR("Read write protect register fail, bus: 0x%x, addr: 0x%x", bus, addr);
		return false;
	}

	if (read_val != set_val) {
		LOG_ERR("Set write protect register fail, bus: 0x%x, addr: 0x%x, ret_val: 0x%x, set_val: 0x%x",
			bus, addr, read_val, set_val);
		return false;
	}

	return true;
}

static bool init_write_protect_default_val(uint8_t bus, uint8_t addr)
{
	uint8_t val = 0;

	if (!xdpe1e3g6a_i2c_read(bus, addr, PMBUS_WRITE_PROTECT, &val, sizeof(val))) {
		LOG_ERR("Read write protect register fail, bus: 0x%x, addr: 0x%x", bus, addr);
		return false;
	}

	write_protect_default_val = val;
	return true;
}

bool xdpe1e3g6a_set_write_protect(uint8_t bus, uint8_t addr, uint8_t option)
{
	if (write_protect_default_val != XDPE1E3G6A_DISABLE_WRITE_PROTECT_VAL) {
		if (write_protect_default_val == XDPE1E3G6A_WRITE_PROTECT_DEFAULT_VAL) {
			if (init_write_protect_default_val(bus, addr) != true) {
				return false;
			}
		}
		return xdpe1e3g6a_set_write_protect_reg(bus, addr, option);
	}

	return true;
}

void xdpe1e3g6a_set_write_protect_default_val(uint8_t val)
{
	// Set write protection default value according to different projects
	write_protect_default_val = val;
}

bool xdpe1e3g6a_get_checksum(uint8_t bus, uint8_t addr, uint8_t *checksum)
{
	CHECK_NULL_ARG_WITH_RETURN(checksum, false);

	// Block read reply: byte count (1 byte) followed by CRC data
	uint8_t data[XDPE1E3G6A_CRC_CHECKSUM_LEN + 1] = { 0 };
	if (!xdpe1e3g6a_i2c_read(bus, addr, XDPE1E3G6A_CRC_CHECKSUM_REG, data, sizeof(data))) {
		LOG_ERR("Get checksum fail, bus: 0x%x, addr: 0x%x", bus, addr);
		return false;
	}

	memcpy(checksum, &data[1], XDPE1E3G6A_CRC_CHECKSUM_LEN);
	return true;
}

bool xdpe1e3g6a_get_status_byte(uint8_t bus, uint8_t addr, uint8_t *data)
{
	CHECK_NULL_ARG_WITH_RETURN(data, false);

	return xdpe1e3g6a_i2c_read(bus, addr, PMBUS_STATUS_BYTE, data, sizeof(uint8_t));
}

bool xdpe1e3g6a_get_vout_max(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, false);
	CHECK_NULL_ARG_WITH_RETURN(millivolt, false);

	uint8_t data[2] = { 0 };
	if (!xdpe1e3g6a_i2c_read(cfg->port, cfg->target_addr, PMBUS_VOUT_MAX, data,
				 sizeof(data))) {
		return false;
	}

	float exponent = 0;
	if (!get_exponent_from_vout_mode(cfg, &exponent) || exponent == 0) {
		return false;
	}

	uint16_t read_value = data[0] | (data[1] << 8);
	*millivolt = (uint16_t)(read_value * exponent * 1000);

	return true;
}

bool xdpe1e3g6a_get_vout_min(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, false);
	CHECK_NULL_ARG_WITH_RETURN(millivolt, false);

	uint8_t data[2] = { 0 };
	if (!xdpe1e3g6a_i2c_read(cfg->port, cfg->target_addr, XDPE1E3G6A_VOUT_MIN_REG, data,
				 sizeof(data))) {
		return false;
	}

	float exponent = 0;
	if (!get_exponent_from_vout_mode(cfg, &exponent) || exponent == 0) {
		return false;
	}

	uint16_t read_value = data[0] | (data[1] << 8);
	*millivolt = (uint16_t)(read_value * exponent * 1000);

	return true;
}

bool xdpe1e3g6a_set_vout_max(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, false);
	CHECK_NULL_ARG_WITH_RETURN(millivolt, false);

	float exponent = 0;
	if (!get_exponent_from_vout_mode(cfg, &exponent) || exponent == 0) {
		return false;
	}

	uint16_t write_value = (uint16_t)((*millivolt / 1000.0f) / exponent + 0.5f);

	uint8_t data[2] = { 0 };
	data[0] = write_value & 0xFF;
	data[1] = (write_value >> 8) & 0xFF;

	if (!xdpe1e3g6a_i2c_write(cfg->port, cfg->target_addr, PMBUS_VOUT_MAX, data,
				  sizeof(data))) {
		return false;
	}

	return true;
}

bool xdpe1e3g6a_set_vout_min(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, false);
	CHECK_NULL_ARG_WITH_RETURN(millivolt, false);

	float exponent = 0;
	if (!get_exponent_from_vout_mode(cfg, &exponent) || exponent == 0) {
		return false;
	}

	uint16_t write_value = (uint16_t)((*millivolt / 1000.0f) / exponent + 0.5f);

	uint8_t data[2] = { 0 };
	data[0] = write_value & 0xFF;
	data[1] = (write_value >> 8) & 0xFF;

	if (!xdpe1e3g6a_i2c_write(cfg->port, cfg->target_addr, XDPE1E3G6A_VOUT_MIN_REG, data,
				  sizeof(data))) {
		return false;
	}

	return true;
}

uint8_t xdpe1e3g6a_read(sensor_cfg *cfg, int *reading)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, SENSOR_UNSPECIFIED_ERROR);
	CHECK_NULL_ARG_WITH_RETURN(reading, SENSOR_UNSPECIFIED_ERROR);

	if (cfg->num > SENSOR_NUM_MAX) {
		LOG_ERR("sensor num: 0x%x is invalid", cfg->num);
		return SENSOR_UNSPECIFIED_ERROR;
	}

	uint8_t retry = 5;
	sensor_val *sval = (sensor_val *)reading;
	I2C_MSG msg;
	memset(sval, 0, sizeof(sensor_val));

	msg.bus = cfg->port;
	msg.target_addr = cfg->target_addr;
	msg.tx_len = 1;
	msg.rx_len = 2;
	msg.data[0] = cfg->offset;

	if (i2c_master_read(&msg, retry))
		return SENSOR_FAIL_TO_ACCESS;

	uint8_t offset = cfg->offset;
	if (offset == PMBUS_READ_VOUT) {
		/* ULINEAR16, get exponent from VOUT_MODE */
		float exponent;
		if (!get_exponent_from_vout_mode(cfg, &exponent))
			return SENSOR_FAIL_TO_ACCESS;

		float actual_value = ((msg.data[1] << 8) | msg.data[0]) * exponent;
		sval->integer = actual_value;
		sval->fraction = (actual_value - sval->integer) * 1000;
	} else if (offset == PMBUS_READ_IOUT || offset == PMBUS_READ_TEMPERATURE_1 ||
		   offset == PMBUS_READ_POUT) {
		/* SLINEAR11 */
		uint16_t read_value = (msg.data[1] << 8) | msg.data[0];
		float actual_value = slinear11_to_float(read_value);
		if (offset == PMBUS_READ_IOUT && actual_value < 0) {
			/* In the case POUT is 0, IOUT may read small negative value, replace this case with 0 */
			sval->integer = 0;
			sval->fraction = 0;
		} else {
			sval->integer = actual_value;
			sval->fraction = (actual_value - sval->integer) * 1000;
		}
	} else {
		return SENSOR_FAIL_TO_ACCESS;
	}

	return SENSOR_READ_SUCCESS;
}

uint8_t xdpe1e3g6a_init(sensor_cfg *cfg)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, SENSOR_INIT_UNSPECIFIED_ERROR);

	if (cfg->num > SENSOR_NUM_MAX) {
		return SENSOR_INIT_UNSPECIFIED_ERROR;
	}

	cfg->read = xdpe1e3g6a_read;
	return SENSOR_INIT_SUCCESS;
}

// GET_FW_ADDRESS option 2 returns the scratchpad (SCPAD) address used to stage OTP writes
static bool xdpe1e3g6a_get_scratchpad_addr(uint8_t bus, uint8_t addr, uint32_t *scpad)
{
	uint8_t tx[4] = { 2, 0, 0, 0 };
	uint8_t rx[4] = { 0 };

	if (!xdpe1e3g6a_fw_command(bus, addr, XDPE1E3G6A_FW_CMD_GET_FW_ADDRESS, tx, rx, 1)) {
		return false;
	}

	*scpad = rx[0] | (rx[1] << 8) | (rx[2] << 16) | (rx[3] << 24);
	return true;
}

// header_code=XVcode=0xFE matches every non-trim OTP section ("reprogram entire configuration file")
static bool xdpe1e3g6a_invalidate_all(uint8_t bus, uint8_t addr)
{
	uint8_t tx[4] = { 0xFE, 0xFE, 0, 0 };

	return xdpe1e3g6a_fw_command(bus, addr, XDPE1E3G6A_FW_CMD_OTP_SECTION_INVALIDATE, tx, NULL,
				     100);
}

static bool xdpe1e3g6a_write_scratchpad(uint8_t bus, uint8_t addr, const uint32_t *dwords,
					uint16_t dword_cnt)
{
	uint32_t scpad = 0;

	if (!xdpe1e3g6a_get_scratchpad_addr(bus, addr, &scpad)) {
		return false;
	}

	if (!xdpe1e3g6a_ahb_set_address(bus, addr, scpad)) {
		return false;
	}

	for (uint16_t i = 0; i < dword_cnt; i++) {
		if (!xdpe1e3g6a_ahb_write_dword(bus, addr, dwords[i])) {
			return false;
		}
	}

	return true;
}

static bool xdpe1e3g6a_otp_config_store(uint8_t bus, uint8_t addr, uint16_t size_bytes)
{
	uint8_t page = 0xFF; // set page to all pages
	if (!xdpe1e3g6a_i2c_write(bus, addr, PMBUS_PAGE, &page, 1)) {
		return false;
	}

	I2C_MSG clr_msg = { 0 };
	clr_msg.bus = bus;
	clr_msg.target_addr = addr;
	clr_msg.tx_len = 1;
	clr_msg.data[0] = PMBUS_CLEAR_FAULTS;
	i2c_master_write(&clr_msg, 3); // best-effort, faults are re-checked after the upload

	uint8_t tx[4] = { size_bytes & 0xFF, (size_bytes >> 8) & 0xFF, 0, 0 };

	// soak time is 2 ms/byte uploaded
	return xdpe1e3g6a_fw_command(bus, addr, XDPE1E3G6A_FW_CMD_OTP_CONFIG_STORE, tx, NULL,
				     (uint32_t)size_bytes * 2 + 10);
}

static bool xdpe1e3g6a_check_upload_ok(uint8_t bus, uint8_t addr)
{
	uint8_t cml = 0;

	if (!xdpe1e3g6a_i2c_read(bus, addr, PMBUS_STATUS_CML, &cml, sizeof(cml))) {
		return false;
	}

	return !(cml & BIT(0));
}

// Parses a single-device config file bounded by [Configuration Data]/[End Configuration Data],
// grouping rows under each "//XV<n> <type>" header into one section per
// XDPE1Exxx Family Configuration Programming Guide chapter 5/6.
static int xdpe1e3g6a_parse_config(struct xdpe1e3g6a_config *cfg, uint8_t *buf, uint32_t size)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, -1);
	CHECK_NULL_ARG_WITH_RETURN(buf, -1);

	const size_t len_start = strlen(XDPE1E3G6A_DATA_START_TAG);
	const size_t len_end = strlen(XDPE1E3G6A_DATA_END_TAG);
	const size_t len_sect = strlen(XDPE1E3G6A_SECTION_TAG);
	uint32_t i = 0;
	bool in_data = false;
	int sect_idx = -1;

	memset(cfg, 0, sizeof(*cfg));

	while (i < size) {
		int eol = find_byte_data_in_buf(buf, '\n', i, size);
		uint32_t line_end = (eol < 0) ? size : (uint32_t)eol;
		uint32_t line_len = (line_end > i) ? (line_end - i) : 0;

		if (!in_data) {
			if (line_len >= len_start &&
			    !strncmp((char *)&buf[i], XDPE1E3G6A_DATA_START_TAG, len_start)) {
				in_data = true;
			}
			i = line_end + 1;
			continue;
		}

		if (line_len >= len_end &&
		    !strncmp((char *)&buf[i], XDPE1E3G6A_DATA_END_TAG, len_end)) {
			break;
		}

		if (line_len >= len_sect &&
		    !strncmp((char *)&buf[i], XDPE1E3G6A_SECTION_TAG, len_sect)) {
			if (++sect_idx >= XDPE1E3G6A_MAX_SECT_NUM) {
				LOG_ERR("Too many config sections in file");
				return -1;
			}
			cfg->sect_cnt = sect_idx + 1;
			i = line_end + 1;
			continue;
		}

		if (line_len == 0 || sect_idx < 0) {
			i = line_end + 1;
			continue;
		}

		char line[128] = { 0 };
		size_t copy_len = (line_len < sizeof(line) - 1) ? line_len : sizeof(line) - 1;
		memcpy(line, &buf[i], copy_len);

		struct xdpe1e3g6a_config_section *sect = &cfg->section[sect_idx];
		char *save_ptr;
		char *tok = strtok_r(line, " \t\r", &save_ptr);
		int tok_idx = 0;

		while (tok) {
			if (tok_idx > 0) { // skip the leading row-offset token
				if (sect->dword_cnt >= XDPE1E3G6A_MAX_SECT_DWORDS) {
					LOG_ERR("Too many DWORDs in config section %d", sect_idx);
					return -1;
				}
				sect->data[sect->dword_cnt++] = (uint32_t)strtoul(tok, NULL, 16);
			}
			tok = strtok_r(NULL, " \t\r", &save_ptr);
			tok_idx++;
		}

		i = line_end + 1;
	}

	if (sect_idx < 0) {
		LOG_ERR("No configuration sections found in file");
		return -1;
	}

	for (int s = 0; s <= sect_idx; s++) {
		struct xdpe1e3g6a_config_section *sect = &cfg->section[s];
		if (sect->dword_cnt < 2) {
			LOG_ERR("Config section %d has too little data", s);
			return -1;
		}
		// first DWORD: byte0=header_code, byte1=XVcode, byte3=loop (see chapter 5.3)
		sect->header_code = sect->data[0] & 0xFF;
		sect->xvcode = (sect->data[0] >> 8) & 0xFF;
		sect->loop = (sect->data[0] >> 24) & 0xFF;
	}

	return 0;
}

bool xdpe1e3g6a_get_config_checksum(uint8_t bus, uint8_t addr, uint32_t *checksum)
{
	CHECK_NULL_ARG_WITH_RETURN(checksum, false);

	// header_code=0, XVcode=0 requests the total configuration checksum (chapter 8.1)
	uint8_t tx[4] = { 0, 0, 0, 0 };
	uint8_t rx[4] = { 0 };

	if (!xdpe1e3g6a_fw_command(bus, addr, XDPE1E3G6A_FW_CMD_GET_CRC, tx, rx, 20)) {
		return false;
	}

	*checksum = rx[0] | (rx[1] << 8) | (rx[2] << 16) | (rx[3] << 24);
	return true;
}

bool xdpe1e3g6a_fw_reset(uint8_t bus, uint8_t addr)
{
	// reinitializes the controller so registers are re-downloaded from OTP (chapter 10.3)
	return xdpe1e3g6a_fw_command(bus, addr, XDPE1E3G6A_FW_CMD_FW_RESET, NULL, NULL, 500);
}

bool xdpe1e3g6a_fwupdate(uint8_t bus, uint8_t addr, uint8_t *img_buff, uint32_t img_size)
{
	CHECK_NULL_ARG_WITH_RETURN(img_buff, false);

	bool ret = false;
	struct xdpe1e3g6a_config *cfg =
		(struct xdpe1e3g6a_config *)malloc(sizeof(struct xdpe1e3g6a_config));
	if (!cfg) {
		LOG_ERR("malloc fail");
		return false;
	}

	if (xdpe1e3g6a_parse_config(cfg, img_buff, img_size) != 0) {
		LOG_ERR("Parse config file fail");
		goto exit;
	}

	// MFR_FW_COMMAND/MFR_AHB_ADDRESS/MFR_REG_WRITE/PAGE/CLEAR_FAULTS are blocked unless disabled
	if (!xdpe1e3g6a_set_write_protect(bus, addr, XDPE1E3G6A_DISABLE_WRITE_PROTECT)) {
		LOG_ERR("Disable write protect fail");
		goto exit;
	}

	// Reprogram entire configuration file: invalidate all non-trim OTP sections first
	if (!xdpe1e3g6a_invalidate_all(bus, addr)) {
		LOG_ERR("Invalidate existing OTP data fail");
		goto restore_protect;
	}

	for (uint8_t s = 0; s < cfg->sect_cnt; s++) {
		struct xdpe1e3g6a_config_section *sect = &cfg->section[s];

		if (sect->header_code == XDPE1E3G6A_HC_TRIM) {
			continue; // never overwrite factory trim data
		}

		uint16_t size_bytes = sect->dword_cnt * 4;

		if (!xdpe1e3g6a_write_scratchpad(bus, addr, sect->data, sect->dword_cnt)) {
			LOG_ERR("Section %d (hc: 0x%x, xv: 0x%x): write scratchpad fail", s,
				sect->header_code, sect->xvcode);
			goto restore_protect;
		}

		if (!xdpe1e3g6a_otp_config_store(bus, addr, size_bytes)) {
			LOG_ERR("Section %d (hc: 0x%x, xv: 0x%x): OTP config store fail", s,
				sect->header_code, sect->xvcode);
			goto restore_protect;
		}

		if (!xdpe1e3g6a_check_upload_ok(bus, addr)) {
			LOG_ERR("Section %d (hc: 0x%x, xv: 0x%x): CML fault after upload", s,
				sect->header_code, sect->xvcode);
			goto restore_protect;
		}
	}

	ret = true;
restore_protect:
	if (!xdpe1e3g6a_set_write_protect(bus, addr, XDPE1E3G6A_ENABLE_WRITE_PROTECT)) {
		LOG_ERR("Restore write protect fail");
		ret = false;
	}
exit:
	SAFE_FREE(cfg);
	return ret;
}
