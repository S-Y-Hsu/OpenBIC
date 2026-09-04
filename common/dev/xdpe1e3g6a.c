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

// IC_DEVICE_ID (ADh), block read count = 2 (programming guide chapter 3.5)
#define XDPE1E_IC_DEVICE_ID_REG 0xAD
#define XDPE1E_IC_DEVICE_ID_LEN 2

// MFR_AHB_ADDRESS (CEh) is the register address pointer for MFR_REG_WRITE (DEh) / MFR_REG_READ (DFh)
#define XDPE1E3G6A_MFR_AHB_ADDRESS_REG 0xCE
#define XDPE1E3G6A_MFR_REG_WRITE_REG 0xDE
#define XDPE1E3G6A_MFR_REG_READ_REG 0xDF

// MFR_FW_COMMAND_DATA (FDh) holds input/output for the MFR_FW_COMMAND (FEh) sub-command dispatch
#define XDPE1E3G6A_MFR_FW_COMMAND_DATA_REG 0xFD
#define XDPE1E3G6A_MFR_FW_COMMAND_REG 0xFE

// XDPE1E496A security / extended write protect
#define XDPE1E496A_MFR_DISABLE_SECURITY_ONCE_REG 0xCB
#define XDPE1E496A_MFR_DISABLE_SECURITY_ONCE_LEN 4
#define XDPE1E496A_EXT_WRITE_PROTECT_REG 0xD6

// Partition 0 holds the configuration file (programming guide chapter 9)
#define XDPE1E_OTP_CONFIG_PARTITION 0

// MFR_FW_COMMAND (FEh) byte codes, programming guide Table 4
enum XDPE1E_FW_CMD {
	XDPE1E_FW_CMD_FW_VERSION = 0x01,
	XDPE1E_FW_CMD_STORE_CONFIG = 0x04,
	XDPE1E_FW_CMD_CONFIGURATOR_STATUS_GET = 0x05,
	XDPE1E_FW_CMD_RESTORE_CONFIG = 0x08,
	XDPE1E_FW_CMD_FW_RESET = 0x0E,
	XDPE1E_FW_CMD_OTP_PARTITION_SIZE_REMAINING = 0x10,
	XDPE1E_FW_CMD_OTP_CONFIG_STORE = 0x11,
	XDPE1E_FW_CMD_OTP_SECTION_INVALIDATE = 0x12,
	XDPE1E_FW_CMD_STORE_PARTIAL_CONFIG = 0x14,
	XDPE1E_FW_CMD_RESTORE_PARTIAL_CONFIG = 0x15,
	XDPE1E_FW_CMD_FW_UPDATE_HSIF = 0x1D,
	XDPE1E_FW_CMD_STORE_HSIF_CODE = 0x1E,
	XDPE1E_FW_CMD_STORE_HSIF_ALL = 0x1F,
	XDPE1E_FW_CMD_WRITE_HSIF_CODE = 0x20,
	XDPE1E_FW_CMD_READ_HSIF_CODE = 0x21,
	XDPE1E_FW_CMD_GET_CRC = 0x2D,
	XDPE1E_FW_CMD_GET_FW_ADDRESS = 0x2E,
};

// CONFIGURATOR_STATUS_GET (FEh/05h) return codes, programming guide chapter 6.5
enum XDPE1E_CONFIGURATOR_STATUS {
	XDPE1E_UPLOAD_SUCCESS = 0,
	XDPE1E_UPLOAD_BUSY = 1,
};

// Configuration data section header codes, programming guide Table 7
enum XDPE1E_CFG_HEADER_CODE {
	XDPE1E_HC_UNPROGRAMMED = 0x00,
	XDPE1E_HC_TRIM = 0x02,
	XDPE1E_HC_CONFIG = 0x04,
	XDPE1E_HC_PMBUS = 0x07,
	XDPE1E_HC_HSIF = 0x0D,
	XDPE1E_HC_CONFIG_PARTIAL = 0x0A,
	XDPE1E_HC_PMBUS_PARTIAL = 0x0B,
	XDPE1E_HC_PATCH = 0x10,
	XDPE1E_HC_HSIF_PARTIAL = 0x11,
	XDPE1E_HC_INVALIDATED = 0xFF,
};

// GET_CRC (FEh/2Dh) sentinel returns, programming guide chapter 8.4
#define XDPE1E_CRC_SECTION_NOT_FOUND 0x00000000
#define XDPE1E_CRC_OTP_CORRUPT 0xFFFFFFFF

// Config file section tags, programming guide chapter 4
#define XDPE1E_DATA_START_TAG "[Configuration Data]"
#define XDPE1E_DATA_END_TAG "[End Configuration Data]"
#define XDPE1E_SECTION_TAG "//XV"
#define XDPE1E_PART_NUMBER_TAG "Part Number"
#define XDPE1E_CFG_CHECKSUM_TAG "Configuration Checksum"

/*
 * A data row is "RRR DDDDDDDD DDDDDDDD DDDDDDDD DDDDDDDD" (max 40 chars). The
 * generous bound catches padded or reformatted files; anything longer is
 * rejected rather than silently truncated into a bogus DWORD.
 */
#define XDPE1E_MAX_LINE_LEN 96
#define XDPE1E_MAX_DWORDS_PER_ROW 4

// A 4-loop config file with per-XVcode partials runs well under this
#define XDPE1E_MAX_SECT_NUM 64

#define XDPE1E_PART_NUMBER_LEN 24

/*
 * Section descriptor. The image data itself is never copied: data_off/data_end
 * bound the section inside the caller's buffer and the rows are re-parsed on
 * the fly when the section is streamed to the scratchpad. This keeps the
 * working set at roughly 1.3 kB regardless of image size.
 */
struct xdpe1e_sect_desc {
	uint32_t data_off; // offset of the first data row
	uint32_t data_end; // offset one past the last data row
	uint32_t dword_cnt;
	uint32_t hdr_size; // size field from the 2nd DWORD (non-partial sections)
	uint8_t header_code;
	uint8_t xvcode;
	uint8_t loop;
};

struct xdpe1e_config_file {
	char part_number[XDPE1E_PART_NUMBER_LEN];
	uint32_t cfg_checksum;
	bool has_checksum;
	uint8_t sect_cnt;
	struct xdpe1e_sect_desc sect[XDPE1E_MAX_SECT_NUM];
};

struct xdpe1e_dev_info {
	const char *name;
	const char *file_tag; // substring expected in the config file's Part Number line
	uint8_t product_id; // IC_DEVICE_ID (ADh) byte1
	uint8_t loop_cnt; // number of real loops / PMBus pages
	bool has_security; // MFR security / EXT_WRITE_PROTECT pre-flight applies
};

static const struct xdpe1e_dev_info xdpe1e_dev_table[XDPE1E_DEV_MAX] = {
	[XDPE1E_DEV_3G6A] = { "XDPE1E3G6A", "XDPE1E3G6", XDPE1E3G6_PRODUCT_ID, 3, false },
	[XDPE1E_DEV_496A] = { "XDPE1E496A", "XDPE1E496", XDPE1E496_PRODUCT_ID, 4, true },
};

static uint8_t write_protect_default_val = XDPE1E3G6A_WRITE_PROTECT_DEFAULT_VAL;

static const struct xdpe1e_dev_info *xdpe1e_get_dev_info(uint8_t dev)
{
	if (dev >= XDPE1E_DEV_MAX) {
		LOG_ERR("Invalid xdpe1e device type: %d", dev);
		return NULL;
	}

	return &xdpe1e_dev_table[dev];
}

static bool xdpe1e_is_partial_section(uint8_t header_code)
{
	return (header_code == XDPE1E_HC_CONFIG_PARTIAL ||
		header_code == XDPE1E_HC_PMBUS_PARTIAL || header_code == XDPE1E_HC_HSIF_PARTIAL);
}

/* -------------------------------------------------------------------------
 * SMBus primitives
 * ------------------------------------------------------------------------- */

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
		LOG_ERR("Failed to read xdpe1e, bus: %d, addr: 0x%x, reg: 0x%x", bus, addr, reg);
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
		LOG_ERR("Failed to write xdpe1e, bus: %d, addr: 0x%x, reg: 0x%x", bus, addr, reg);
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
		LOG_ERR("Failed to block write xdpe1e, bus: %d, addr: 0x%x, reg: 0x%x", bus, addr,
			reg);
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
		LOG_ERR("Failed to block read xdpe1e, bus: %d, addr: 0x%x, reg: 0x%x", bus, addr,
			reg);
		return false;
	}

	if (i2c_msg.data[0] != len) {
		LOG_WRN("Block read reg 0x%x returned byte count %d, expected %d", reg,
			i2c_msg.data[0], len);
	}

	memcpy(data, &i2c_msg.data[1], len);
	return true;
}

/*
 * PAGE (00h) is global device state. Paged accesses here save the current PAGE
 * and restore it so a rail query does not silently re-point a concurrent sensor
 * read. This is save/modify/restore, not atomic: it still races another thread
 * touching PAGE on the same target. Both parts support PAGE_PLUS_READ (06h) /
 * PAGE_PLUS_WRITE (05h), which is the race-free alternative.
 */
static bool xdpe1e_get_page(uint8_t bus, uint8_t addr, uint8_t *page)
{
	return xdpe1e3g6a_i2c_read(bus, addr, PMBUS_PAGE, page, 1);
}

static bool xdpe1e_set_page(uint8_t bus, uint8_t addr, uint8_t page)
{
	return xdpe1e3g6a_i2c_write(bus, addr, PMBUS_PAGE, &page, 1);
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

/* -------------------------------------------------------------------------
 * Write protect
 * ------------------------------------------------------------------------- */

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
		LOG_ERR("Set write protect register fail, bus: 0x%x, addr: 0x%x, set_val: 0x%x", bus,
			addr, set_val);
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

/* -------------------------------------------------------------------------
 * Device identity
 * ------------------------------------------------------------------------- */

/*
 * IC_DEVICE_ID (ADh) is a 2-byte block read where byte0 is the revision code
 * and byte1 is the product ID (programming guide chapter 3.5). The 496A
 * datasheet quotes this as "B100h", which is product 0xB1 with revision 0x00
 * (Revision A) - so comparing the packed 16-bit value would reject an otherwise
 * good Revision B part. Only the product ID is matched here.
 */
bool xdpe1e_get_device_id(uint8_t bus, uint8_t addr, uint8_t *product_id, uint8_t *revision)
{
	CHECK_NULL_ARG_WITH_RETURN(product_id, false);
	CHECK_NULL_ARG_WITH_RETURN(revision, false);

	uint8_t data[XDPE1E_IC_DEVICE_ID_LEN] = { 0 };

	if (!xdpe1e3g6a_block_read(bus, addr, XDPE1E_IC_DEVICE_ID_REG, data, sizeof(data))) {
		return false;
	}

	*revision = data[0];
	*product_id = data[1];
	return true;
}

static bool xdpe1e_verify_dev(uint8_t dev, uint8_t bus, uint8_t addr, uint8_t *revision)
{
	const struct xdpe1e_dev_info *info = xdpe1e_get_dev_info(dev);
	if (!info) {
		return false;
	}

	uint8_t product_id = 0;
	uint8_t rev = 0;

	if (!xdpe1e_get_device_id(bus, addr, &product_id, &rev)) {
		LOG_ERR("%s: IC_DEVICE_ID read fail on bus %d addr 0x%x", info->name, bus, addr);
		return false;
	}

	if (product_id != info->product_id) {
		LOG_ERR("Device on bus %d addr 0x%x reports product ID 0x%02x, expected 0x%02x for %s",
			bus, addr, product_id, info->product_id, info->name);
		return false;
	}

	if (rev != XDPE1E_REV_A && rev != XDPE1E_REV_B) {
		// Revision selects the scratchpad and XVcode readback addresses (Table 6) and the
		// default section sizes (Table 8), so an unknown one is worth surfacing
		LOG_WRN("%s: unrecognised revision code 0x%02x", info->name, rev);
	}

	if (revision) {
		*revision = rev;
	}

	LOG_INF("%s detected on bus %d addr 0x%x, revision 0x%02x", info->name, bus, addr, rev);
	return true;
}

/* -------------------------------------------------------------------------
 * XDPE1E496A security / extended write protect
 * ------------------------------------------------------------------------- */

bool xdpe1e496a_get_security_state(uint8_t bus, uint8_t addr, uint8_t *state)
{
	CHECK_NULL_ARG_WITH_RETURN(state, false);

	uint8_t data[XDPE1E496A_MFR_DISABLE_SECURITY_ONCE_LEN] = { 0 };

	if (!xdpe1e3g6a_block_read(bus, addr, XDPE1E496A_MFR_DISABLE_SECURITY_ONCE_REG, data,
				   sizeof(data))) {
		return false;
	}

	*state = data[0];
	return true;
}

bool xdpe1e496a_get_ext_write_protect(uint8_t bus, uint8_t addr, uint8_t *val)
{
	CHECK_NULL_ARG_WITH_RETURN(val, false);

	return xdpe1e3g6a_i2c_read(bus, addr, XDPE1E496A_EXT_WRITE_PROTECT_REG, val, 1);
}

/*
 * WRITE_PROTECT (10h) is not the only gate on the XDPE1E496A. The security
 * command group (CAh/CBh/D2h/D3h) and EXT_WRITE_PROTECT (D6h) can mask MFR_*
 * and NVM-store commands independently. When they do, the block writes below
 * still ACK on the wire and the update fails after the OTP has already been
 * invalidated, so check before touching anything.
 */
static bool xdpe1e496a_check_update_allowed(uint8_t bus, uint8_t addr)
{
	uint8_t state = 0;

	if (xdpe1e496a_get_security_state(bus, addr, &state)) {
		if (state == XDPE1E496A_SECURITY_LOCKED) {
			LOG_ERR("Security locked, MFR commands are masked; unlock with MFR_SETUP_PASSWORD first");
			return false;
		}
		if (state == XDPE1E496A_SECURITY_DEADLOCKED) {
			LOG_ERR("Security deadlocked, a chip reset is required before an update");
			return false;
		}
	} else {
		LOG_WRN("MFR_DISABLE_SECURITY_ONCE read fail, continuing without a security check");
	}

	uint8_t ext_wp = 0;
	if (xdpe1e496a_get_ext_write_protect(bus, addr, &ext_wp)) {
		uint8_t blocking = ext_wp & (XDPE1E496A_EXT_WP_LOCK_BIT |
					     XDPE1E496A_EXT_WP_STORE_NVM_BIT |
					     XDPE1E496A_EXT_WP_MFR_BIT);
		if (blocking) {
			LOG_ERR("EXT_WRITE_PROTECT 0x%02x blocks MFR/NVM writes, update would not stick",
				ext_wp);
			return false;
		}
	} else {
		LOG_WRN("EXT_WRITE_PROTECT read fail, continuing without an ext write protect check");
	}

	return true;
}

/* -------------------------------------------------------------------------
 * Telemetry / configuration helpers
 * ------------------------------------------------------------------------- */

/*
 * CRC_CHECKSUM (B8h) is an 8-byte block read on every part in the family, so
 * this is device independent. The caller's buffer must hold
 * XDPE1E_CRC_CHECKSUM_LEN bytes.
 */
bool xdpe1e3g6a_get_checksum(uint8_t bus, uint8_t addr, uint8_t *checksum)
{
	CHECK_NULL_ARG_WITH_RETURN(checksum, false);

	if (!xdpe1e3g6a_block_read(bus, addr, XDPE1E3G6A_CRC_CHECKSUM_REG, checksum,
				   XDPE1E_CRC_CHECKSUM_LEN)) {
		LOG_ERR("Get checksum fail, bus: 0x%x, addr: 0x%x", bus, addr);
		return false;
	}

	return true;
}

bool xdpe1e496a_get_checksum(uint8_t bus, uint8_t addr, uint8_t *checksum)
{
	return xdpe1e3g6a_get_checksum(bus, addr, checksum);
}

bool xdpe1e3g6a_get_status_byte(uint8_t bus, uint8_t addr, uint8_t *data)
{
	CHECK_NULL_ARG_WITH_RETURN(data, false);

	return xdpe1e3g6a_i2c_read(bus, addr, PMBUS_STATUS_BYTE, data, sizeof(uint8_t));
}

// GET_CRC with header_code = XVcode = loop = 0 returns the total configuration checksum (chapter 8.1)
bool xdpe1e3g6a_get_config_checksum(uint8_t bus, uint8_t addr, uint32_t *checksum)
{
	CHECK_NULL_ARG_WITH_RETURN(checksum, false);

	uint8_t tx[4] = { 0, 0, 0, 0 };
	uint8_t rx[4] = { 0 };

	if (!xdpe1e3g6a_fw_command(bus, addr, XDPE1E_FW_CMD_GET_CRC, tx, rx, 20)) {
		return false;
	}

	uint32_t crc = rx[0] | (rx[1] << 8) | (rx[2] << 16) | (rx[3] << 24);

	if (crc == XDPE1E_CRC_OTP_CORRUPT) {
		LOG_ERR("GET_CRC returned 0xffffffff, OTP is reported corrupt");
		return false;
	}

	*checksum = crc;
	return true;
}

bool xdpe1e_get_otp_remaining(uint8_t bus, uint8_t addr, uint8_t partition, uint32_t *bytes)
{
	CHECK_NULL_ARG_WITH_RETURN(bytes, false);

	uint8_t tx[4] = { 0, 0, 0, partition };
	uint8_t rx[4] = { 0 };

	if (!xdpe1e3g6a_fw_command(bus, addr, XDPE1E_FW_CMD_OTP_PARTITION_SIZE_REMAINING, tx, rx,
				   1)) {
		return false;
	}

	*bytes = (uint32_t)rx[0] + ((uint32_t)rx[1] * 256);
	return true;
}

bool xdpe1e3g6a_fw_reset(uint8_t bus, uint8_t addr)
{
	// Chapter 10.3: 12 V must be off, and the device may return at a different PMBus address
	return xdpe1e3g6a_fw_command(bus, addr, XDPE1E_FW_CMD_FW_RESET, NULL, NULL, 500);
}

/* -------------------------------------------------------------------------
 * VOUT_MAX (24h) / VOUT_MIN (2Bh), per rail
 * ------------------------------------------------------------------------- */

static bool xdpe1e_get_vout_limit(uint8_t dev, sensor_cfg *cfg, uint8_t rail, uint8_t reg,
				  uint16_t *millivolt)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, false);
	CHECK_NULL_ARG_WITH_RETURN(millivolt, false);

	const struct xdpe1e_dev_info *info = xdpe1e_get_dev_info(dev);
	if (!info) {
		return false;
	}

	if (rail >= info->loop_cnt) {
		LOG_ERR("%s: rail %d out of range, %d loops", info->name, rail, info->loop_cnt);
		return false;
	}

	uint8_t saved_page = 0;
	uint8_t data[2] = { 0 };
	uint16_t read_value = 0;
	float exponent = 0;
	bool ret = false;
	bool page_saved = xdpe1e_get_page(cfg->port, cfg->target_addr, &saved_page);

	if (!xdpe1e_set_page(cfg->port, cfg->target_addr, rail)) {
		return false;
	}

	if (!xdpe1e3g6a_i2c_read(cfg->port, cfg->target_addr, reg, data, sizeof(data))) {
		goto restore_page;
	}

	// VOUT_MODE is paged too, so it has to be sampled while this rail is selected
	if (!get_exponent_from_vout_mode(cfg, &exponent) || exponent == 0) {
		goto restore_page;
	}

	read_value = data[0] | (data[1] << 8);
	*millivolt = (uint16_t)(read_value * exponent * 1000);
	ret = true;

restore_page:
	if (page_saved && !xdpe1e_set_page(cfg->port, cfg->target_addr, saved_page)) {
		LOG_ERR("%s: failed to restore PAGE 0x%x", info->name, saved_page);
		ret = false;
	}

	return ret;
}

static bool xdpe1e_set_vout_limit(uint8_t dev, sensor_cfg *cfg, uint8_t rail, uint8_t reg,
				  uint16_t *millivolt)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, false);
	CHECK_NULL_ARG_WITH_RETURN(millivolt, false);

	const struct xdpe1e_dev_info *info = xdpe1e_get_dev_info(dev);
	if (!info) {
		return false;
	}

	if (rail >= info->loop_cnt) {
		LOG_ERR("%s: rail %d out of range, %d loops", info->name, rail, info->loop_cnt);
		return false;
	}

	uint8_t saved_page = 0;
	uint8_t data[2] = { 0 };
	uint16_t write_value = 0;
	float exponent = 0;
	bool ret = false;
	bool page_saved = xdpe1e_get_page(cfg->port, cfg->target_addr, &saved_page);

	if (!xdpe1e_set_page(cfg->port, cfg->target_addr, rail)) {
		return false;
	}

	if (!get_exponent_from_vout_mode(cfg, &exponent) || exponent == 0) {
		goto restore_page;
	}

	write_value = (uint16_t)((*millivolt / 1000.0f) / exponent + 0.5f);
	data[0] = write_value & 0xFF;
	data[1] = (write_value >> 8) & 0xFF;

	if (!xdpe1e3g6a_i2c_write(cfg->port, cfg->target_addr, reg, data, sizeof(data))) {
		goto restore_page;
	}

	ret = true;

restore_page:
	if (page_saved && !xdpe1e_set_page(cfg->port, cfg->target_addr, saved_page)) {
		LOG_ERR("%s: failed to restore PAGE 0x%x", info->name, saved_page);
		ret = false;
	}

	return ret;
}

bool xdpe1e3g6a_get_vout_max(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt)
{
	return xdpe1e_get_vout_limit(XDPE1E_DEV_3G6A, cfg, rail, PMBUS_VOUT_MAX, millivolt);
}

bool xdpe1e3g6a_get_vout_min(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt)
{
	return xdpe1e_get_vout_limit(XDPE1E_DEV_3G6A, cfg, rail, XDPE1E3G6A_VOUT_MIN_REG,
				     millivolt);
}

bool xdpe1e3g6a_set_vout_max(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt)
{
	return xdpe1e_set_vout_limit(XDPE1E_DEV_3G6A, cfg, rail, PMBUS_VOUT_MAX, millivolt);
}

bool xdpe1e3g6a_set_vout_min(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt)
{
	return xdpe1e_set_vout_limit(XDPE1E_DEV_3G6A, cfg, rail, XDPE1E3G6A_VOUT_MIN_REG,
				     millivolt);
}

bool xdpe1e496a_get_vout_max(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt)
{
	return xdpe1e_get_vout_limit(XDPE1E_DEV_496A, cfg, rail, PMBUS_VOUT_MAX, millivolt);
}

bool xdpe1e496a_get_vout_min(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt)
{
	return xdpe1e_get_vout_limit(XDPE1E_DEV_496A, cfg, rail, XDPE1E3G6A_VOUT_MIN_REG,
				     millivolt);
}

bool xdpe1e496a_set_vout_max(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt)
{
	return xdpe1e_set_vout_limit(XDPE1E_DEV_496A, cfg, rail, PMBUS_VOUT_MAX, millivolt);
}

bool xdpe1e496a_set_vout_min(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt)
{
	return xdpe1e_set_vout_limit(XDPE1E_DEV_496A, cfg, rail, XDPE1E3G6A_VOUT_MIN_REG,
				     millivolt);
}

/* -------------------------------------------------------------------------
 * Sensor read
 * ------------------------------------------------------------------------- */

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
		   offset == PMBUS_READ_TEMPERATURE_2 || offset == PMBUS_READ_POUT ||
		   offset == PMBUS_READ_VIN || offset == PMBUS_READ_IIN ||
		   offset == PMBUS_READ_PIN) {
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

// Telemetry path is identical; the separate entry point keeps the sensor table explicit
uint8_t xdpe1e496a_init(sensor_cfg *cfg)
{
	CHECK_NULL_ARG_WITH_RETURN(cfg, SENSOR_INIT_UNSPECIFIED_ERROR);

	if (cfg->num > SENSOR_NUM_MAX) {
		return SENSOR_INIT_UNSPECIFIED_ERROR;
	}

	cfg->read = xdpe1e3g6a_read;
	return SENSOR_INIT_SUCCESS;
}

/* -------------------------------------------------------------------------
 * Streaming configuration file parser
 *
 * The file is walked twice over the caller's buffer. The scan pass records only
 * a descriptor per section (offsets, header fields, DWORD count); the program
 * pass re-parses each section's rows and pushes DWORDs straight at the
 * scratchpad. No image data is ever copied into a driver buffer.
 * ------------------------------------------------------------------------- */

// Yields the next line as an offset/length pair, stripping a trailing CR
static bool xdpe1e_next_line(const uint8_t *buf, uint32_t size, uint32_t *pos, uint32_t *start,
			     uint32_t *len)
{
	if (*pos >= size) {
		return false;
	}

	uint32_t end = *pos;
	while (end < size && buf[end] != '\n') {
		end++;
	}

	*start = *pos;
	*len = end - *pos;
	if (*len > 0 && buf[*start + *len - 1] == '\r') {
		(*len)--;
	}

	*pos = (end < size) ? (end + 1) : size;
	return true;
}

// Copies a line into a NUL-terminated scratch buffer. Fails on over-long lines
// rather than truncating, since a truncated hex token parses as a valid DWORD.
static bool xdpe1e_copy_line(const uint8_t *buf, uint32_t start, uint32_t len, char *out,
			     size_t out_sz)
{
	if (len >= out_sz) {
		return false;
	}

	memcpy(out, &buf[start], len);
	out[len] = '\0';
	return true;
}

/*
 * Parses one data row. The leading token is the running byte count (chapter
 * 5.2) and is discarded; the remaining 1..4 tokens are the DWORDs. Returns the
 * DWORD count, or -1 on a malformed row.
 */
static int xdpe1e_parse_row(const uint8_t *buf, uint32_t start, uint32_t len, uint32_t *dw,
			    uint8_t max_dw)
{
	char line[XDPE1E_MAX_LINE_LEN];

	if (len == 0) {
		return 0;
	}

	if (!xdpe1e_copy_line(buf, start, len, line, sizeof(line))) {
		LOG_ERR("Config row longer than %d bytes", XDPE1E_MAX_LINE_LEN - 1);
		return -1;
	}

	char *save_ptr = NULL;
	char *tok = strtok_r(line, " \t", &save_ptr);
	int tok_idx = 0;
	int n = 0;

	while (tok) {
		if (tok_idx > 0) {
			if (n >= max_dw) {
				LOG_ERR("Config row has more than %d DWORDs", max_dw);
				return -1;
			}

			char *endp = NULL;
			unsigned long val = strtoul(tok, &endp, 16);
			if (endp == tok || *endp != '\0') {
				LOG_ERR("Malformed DWORD token in config row");
				return -1;
			}
			dw[n++] = (uint32_t)val;
		}
		tok = strtok_r(NULL, " \t", &save_ptr);
		tok_idx++;
	}

	return n;
}

// Picks up "Part Number" and "Configuration Checksum" from the basic device information block
static void xdpe1e_parse_info_line(struct xdpe1e_config_file *file, const char *line)
{
	const char *val = NULL;

	if (!strncmp(line, XDPE1E_PART_NUMBER_TAG, strlen(XDPE1E_PART_NUMBER_TAG))) {
		if (file->part_number[0] != '\0') {
			return; // first device in the file wins
		}
		val = strchr(line, ':');
		if (!val) {
			return;
		}
		val++;
		while (*val == ' ' || *val == '\t') {
			val++;
		}

		size_t i = 0;
		while (val[i] && val[i] != ' ' && val[i] != '\t' &&
		       i < sizeof(file->part_number) - 1) {
			file->part_number[i] = val[i];
			i++;
		}
		file->part_number[i] = '\0';
		return;
	}

	if (!strncmp(line, XDPE1E_CFG_CHECKSUM_TAG, strlen(XDPE1E_CFG_CHECKSUM_TAG))) {
		if (file->has_checksum) {
			return;
		}
		val = strchr(line, ':');
		if (!val) {
			return;
		}
		val++;
		while (*val == ' ' || *val == '\t') {
			val++;
		}

		char *endp = NULL;
		unsigned long crc = strtoul(val, &endp, 0);
		if (endp != val) {
			file->cfg_checksum = (uint32_t)crc;
			file->has_checksum = true;
		}
	}
}

static void xdpe1e_close_section(struct xdpe1e_config_file *file, int idx, uint32_t data_end)
{
	if (idx >= 0) {
		file->sect[idx].data_end = data_end;
	}
}

/*
 * Scans the file, recording one descriptor per configuration section. Header
 * fields come from the first data row: DWORD0 byte0 = header code, byte1 =
 * XVcode, byte3 = loop (chapter 5.3); DWORD1 carries the section size twice
 * (chapter 5.4).
 */
static int xdpe1e_scan_config(struct xdpe1e_config_file *file, const uint8_t *buf, uint32_t size)
{
	CHECK_NULL_ARG_WITH_RETURN(file, -1);
	CHECK_NULL_ARG_WITH_RETURN(buf, -1);

	const size_t len_start = strlen(XDPE1E_DATA_START_TAG);
	const size_t len_end = strlen(XDPE1E_DATA_END_TAG);
	const size_t len_sect = strlen(XDPE1E_SECTION_TAG);

	uint32_t pos = 0;
	uint32_t line_start = 0;
	uint32_t line_len = 0;
	bool in_data = false;
	int sect_idx = -1;

	memset(file, 0, sizeof(*file));

	while (xdpe1e_next_line(buf, size, &pos, &line_start, &line_len)) {
		if (!in_data) {
			if (line_len >= len_start &&
			    !strncmp((const char *)&buf[line_start], XDPE1E_DATA_START_TAG,
				     len_start)) {
				in_data = true;
				continue;
			}

			char info[XDPE1E_MAX_LINE_LEN];
			if (xdpe1e_copy_line(buf, line_start, line_len, info, sizeof(info))) {
				xdpe1e_parse_info_line(file, info);
			}
			continue;
		}

		if (line_len >= len_end && !strncmp((const char *)&buf[line_start],
						    XDPE1E_DATA_END_TAG, len_end)) {
			xdpe1e_close_section(file, sect_idx, line_start);
			sect_idx = -1;
			break;
		}

		if (line_len >= len_sect &&
		    !strncmp((const char *)&buf[line_start], XDPE1E_SECTION_TAG, len_sect)) {
			xdpe1e_close_section(file, sect_idx, line_start);

			if (file->sect_cnt >= XDPE1E_MAX_SECT_NUM) {
				LOG_ERR("More than %d config sections in file",
					XDPE1E_MAX_SECT_NUM);
				return -1;
			}

			sect_idx = file->sect_cnt++;
			memset(&file->sect[sect_idx], 0, sizeof(file->sect[sect_idx]));
			file->sect[sect_idx].data_off = pos; // first row follows the header line
			continue;
		}

		if (line_len == 0 || sect_idx < 0) {
			continue;
		}

		uint32_t dw[XDPE1E_MAX_DWORDS_PER_ROW] = { 0 };
		int n = xdpe1e_parse_row(buf, line_start, line_len, dw, XDPE1E_MAX_DWORDS_PER_ROW);
		if (n < 0) {
			LOG_ERR("Config section %d has a malformed row", sect_idx);
			return -1;
		}

		struct xdpe1e_sect_desc *sect = &file->sect[sect_idx];

		if (sect->dword_cnt == 0 && n >= 2) {
			sect->header_code = dw[0] & 0xFF;
			sect->xvcode = (dw[0] >> 8) & 0xFF;
			sect->loop = (dw[0] >> 24) & 0xFF;
			sect->hdr_size = dw[1] & 0xFFFF;

			if ((dw[1] >> 16) != sect->hdr_size) {
				LOG_ERR("Config section %d size mismatch: 0x%04x vs 0x%04lx",
					sect_idx, (unsigned int)sect->hdr_size,
					(unsigned long)(dw[1] >> 16));
				return -1;
			}
		}

		sect->dword_cnt += n;
	}

	// A file with no [End Configuration Data] still gets its last section closed
	xdpe1e_close_section(file, sect_idx, size);

	if (file->sect_cnt == 0) {
		LOG_ERR("No configuration sections found in file");
		return -1;
	}

	for (uint8_t s = 0; s < file->sect_cnt; s++) {
		if (file->sect[s].dword_cnt < 3) {
			LOG_ERR("Config section %d is too short (%d DWORDs)", s,
				file->sect[s].dword_cnt);
			return -1;
		}
	}

	return 0;
}

/*
 * Total bytes the section occupies in OTP. Chapter 6.3 defines this as the
 * DWORD count times 4 for every section type, and for non-partial sections it
 * must also equal the size recorded in the header (chapter 6.1). Chapter 6.4's
 * worked example quotes 0x754 for the Figure 9 section whose header says 0x760;
 * Table 8 lists 1888 bytes (0x760) for that same header code, so the header
 * value is the one used here.
 */
static uint32_t xdpe1e_sect_size(const struct xdpe1e_sect_desc *sect)
{
	return sect->dword_cnt * 4;
}

static bool xdpe1e_validate_sections(uint8_t dev, const struct xdpe1e_config_file *file,
				     uint32_t *total_bytes)
{
	const struct xdpe1e_dev_info *info = xdpe1e_get_dev_info(dev);
	if (!info) {
		return false;
	}

	uint32_t total = 0;

	for (uint8_t s = 0; s < file->sect_cnt; s++) {
		const struct xdpe1e_sect_desc *sect = &file->sect[s];
		uint32_t size = xdpe1e_sect_size(sect);

		// Loop 4 is the Psys pseudo-loop and is valid on both parts
		if (sect->loop != XDPE1E_LOOP_PSYS && sect->loop >= info->loop_cnt) {
			LOG_ERR("Section %d targets loop %d, %s has %d loops plus Psys", s,
				sect->loop, info->name, info->loop_cnt);
			return false;
		}

		if (!xdpe1e_is_partial_section(sect->header_code) && sect->hdr_size != size) {
			LOG_ERR("Section %d header size 0x%04x does not match 0x%04x bytes of data",
				s, (unsigned int)sect->hdr_size, (unsigned int)size);
			return false;
		}

		if (sect->header_code == XDPE1E_HC_PATCH) {
			LOG_WRN("Section %d is a firmware patch (hc 0x10); patches are not handled by this path",
				s);
		}

		if (sect->header_code == XDPE1E_HC_TRIM) {
			continue; // never programmed, so it does not consume new OTP space
		}

		total += size;
	}

	if (total_bytes) {
		*total_bytes = total;
	}

	return true;
}

/* -------------------------------------------------------------------------
 * Configuration (OTP) update
 * ------------------------------------------------------------------------- */

// GET_FW_ADDRESS option 2 returns the scratchpad (SCPAD) address used to stage OTP writes
static bool xdpe1e_get_scratchpad_addr(uint8_t bus, uint8_t addr, uint32_t *scpad)
{
	uint8_t tx[4] = { 2, 0, 0, 0 };
	uint8_t rx[4] = { 0 };

	if (!xdpe1e3g6a_fw_command(bus, addr, XDPE1E_FW_CMD_GET_FW_ADDRESS, tx, rx, 1)) {
		return false;
	}

	*scpad = rx[0] | (rx[1] << 8) | (rx[2] << 16) | (rx[3] << 24);

	if (*scpad == 0 || *scpad == 0xFFFFFFFF) {
		LOG_ERR("Implausible scratchpad address 0x%08x", *scpad);
		return false;
	}

	return true;
}

// header_code = XVcode = 0xFE matches every non-trim OTP section (chapter 6.2.1)
static bool xdpe1e_invalidate_all(uint8_t bus, uint8_t addr)
{
	uint8_t tx[4] = { 0xFE, 0xFE, 0, 0 };

	// 4 ms of soak per invalidated header; 100 ms covers roughly 25 sections
	return xdpe1e3g6a_fw_command(bus, addr, XDPE1E_FW_CMD_OTP_SECTION_INVALIDATE, tx, NULL,
				     100);
}

// Re-parses one section's rows and pushes each DWORD straight at the scratchpad
static bool xdpe1e_stream_section(uint8_t bus, uint8_t addr, const uint8_t *buf,
				  const struct xdpe1e_sect_desc *sect)
{
	uint32_t scpad = 0;

	if (!xdpe1e_get_scratchpad_addr(bus, addr, &scpad)) {
		return false;
	}

	if (!xdpe1e3g6a_ahb_set_address(bus, addr, scpad)) {
		return false;
	}

	uint32_t pos = sect->data_off;
	uint32_t line_start = 0;
	uint32_t line_len = 0;
	uint32_t written = 0;

	while (xdpe1e_next_line(buf, sect->data_end, &pos, &line_start, &line_len)) {
		uint32_t dw[XDPE1E_MAX_DWORDS_PER_ROW] = { 0 };
		int n = xdpe1e_parse_row(buf, line_start, line_len, dw, XDPE1E_MAX_DWORDS_PER_ROW);
		if (n < 0) {
			return false;
		}

		for (int i = 0; i < n; i++) {
			// MFR_REG_WRITE auto-increments the register pointer by 4 (chapter 6.3)
			if (!xdpe1e3g6a_ahb_write_dword(bus, addr, dw[i])) {
				return false;
			}
			written++;
		}
	}

	if (written != sect->dword_cnt) {
		LOG_ERR("Streamed %d DWORDs, expected %d", written, sect->dword_cnt);
		return false;
	}

	return true;
}

static bool xdpe1e_otp_config_store(uint8_t bus, uint8_t addr, uint32_t size_bytes)
{
	if (size_bytes > 0xFFFF) {
		LOG_ERR("Section size %d exceeds the 16-bit OTP_CONFIG_STORE field", size_bytes);
		return false;
	}

	// Faults on all loops must be cleared so faults raised by the upload are attributable
	if (!xdpe1e_set_page(bus, addr, XDPE1E_PAGE_ALL)) {
		return false;
	}

	I2C_MSG clr_msg = { 0 };
	clr_msg.bus = bus;
	clr_msg.target_addr = addr;
	clr_msg.tx_len = 1;
	clr_msg.data[0] = PMBUS_CLEAR_FAULTS;
	if (i2c_master_write(&clr_msg, 3)) {
		LOG_WRN("CLEAR_FAULTS failed before OTP store");
	}

	uint8_t tx[4] = { size_bytes & 0xFF, (size_bytes >> 8) & 0xFF, 0, 0 };

	return xdpe1e3g6a_fw_command(bus, addr, XDPE1E_FW_CMD_OTP_CONFIG_STORE, tx, NULL, 0);
}

/*
 * Polls CONFIGURATOR_STATUS_GET instead of sleeping the whole soak time blind.
 * 0 means the upload finished, 1 means still busy, anything else is an upload
 * error (chapter 6.5).
 */
static bool xdpe1e_wait_upload_done(uint8_t bus, uint8_t addr, uint32_t timeout_ms)
{
	const uint32_t poll_ms = 50;
	uint32_t waited = 0;
	uint8_t tx[4] = { 0, 0, 0, 0 };
	uint8_t rx[4] = { 0 };

	while (waited < timeout_ms) {
		k_msleep(poll_ms);
		waited += poll_ms;

		if (!xdpe1e3g6a_fw_command(bus, addr, XDPE1E_FW_CMD_CONFIGURATOR_STATUS_GET, tx, rx,
					   0)) {
			return false;
		}

		if (rx[0] == XDPE1E_UPLOAD_SUCCESS) {
			return true;
		}

		if (rx[0] != XDPE1E_UPLOAD_BUSY) {
			LOG_ERR("Upload reported error status 0x%02x", rx[0]);
			return false;
		}
	}

	LOG_ERR("Upload did not complete within %d ms", timeout_ms);
	return false;
}

static bool xdpe1e_check_upload_ok(uint8_t bus, uint8_t addr)
{
	uint8_t cml = 0;

	if (!xdpe1e3g6a_i2c_read(bus, addr, PMBUS_STATUS_CML, &cml, sizeof(cml))) {
		return false;
	}

	// Bit 0 is the CML "other memory fault" the controller raises on a failed upload
	if (cml & BIT(0)) {
		LOG_ERR("STATUS_CML 0x%02x indicates a failed upload", cml);
		return false;
	}

	return true;
}

static bool xdpe1e_fwupdate(uint8_t dev, uint8_t bus, uint8_t addr, uint8_t *img_buff,
			    uint32_t img_size)
{
	CHECK_NULL_ARG_WITH_RETURN(img_buff, false);

	const struct xdpe1e_dev_info *info = xdpe1e_get_dev_info(dev);
	if (!info) {
		return false;
	}

	bool ret = false;
	uint8_t revision = 0;
	uint32_t total_bytes = 0;
	uint32_t otp_remaining = 0;

	struct xdpe1e_config_file *file =
		(struct xdpe1e_config_file *)malloc(sizeof(struct xdpe1e_config_file));
	if (!file) {
		LOG_ERR("malloc fail");
		return false;
	}

	// Confirm the part before invalidating anything, not after
	if (!xdpe1e_verify_dev(dev, bus, addr, &revision)) {
		LOG_ERR("Device identity check fail, aborting %s update", info->name);
		goto exit;
	}

	if (info->has_security && !xdpe1e496a_check_update_allowed(bus, addr)) {
		LOG_ERR("%s is locked against configuration updates", info->name);
		goto exit;
	}

	if (xdpe1e_scan_config(file, img_buff, img_size) != 0) {
		LOG_ERR("Parse config file fail");
		goto exit;
	}

	if (file->part_number[0] != '\0' && !strstr(file->part_number, info->file_tag)) {
		LOG_ERR("Config file is for '%s', target is %s", file->part_number, info->name);
		goto exit;
	}

	if (!xdpe1e_validate_sections(dev, file, &total_bytes)) {
		goto exit;
	}

	/*
	 * Invalidation does not reclaim OTP; it only marks headers. If the
	 * remaining space cannot hold the new image, running the update anyway
	 * leaves the part with its old configuration invalidated and the new one
	 * only partly written, so check before the point of no return.
	 */
	if (!xdpe1e_get_otp_remaining(bus, addr, XDPE1E_OTP_CONFIG_PARTITION, &otp_remaining)) {
		LOG_ERR("Could not read remaining OTP size");
		goto exit;
	}

	if (otp_remaining < total_bytes) {
		LOG_ERR("Image needs %d bytes, OTP partition %d has %d left", total_bytes,
			XDPE1E_OTP_CONFIG_PARTITION, otp_remaining);
		goto exit;
	}

	LOG_INF("%s: %d sections, %d bytes, %d bytes of OTP free", info->name, file->sect_cnt,
		total_bytes, otp_remaining);

	// MFR_FW_COMMAND/MFR_AHB_ADDRESS/MFR_REG_WRITE/PAGE/CLEAR_FAULTS are blocked unless disabled
	if (!xdpe1e3g6a_set_write_protect(bus, addr, XDPE1E3G6A_DISABLE_WRITE_PROTECT)) {
		LOG_ERR("Disable write protect fail");
		goto exit;
	}

	// Reprogram entire configuration file: invalidate all non-trim OTP sections first
	if (!xdpe1e_invalidate_all(bus, addr)) {
		LOG_ERR("Invalidate existing OTP data fail");
		goto restore_protect;
	}

	for (uint8_t s = 0; s < file->sect_cnt; s++) {
		struct xdpe1e_sect_desc *sect = &file->sect[s];

		if (sect->header_code == XDPE1E_HC_TRIM) {
			continue; // never overwrite factory trim data
		}

		uint32_t size_bytes = xdpe1e_sect_size(sect);

		if (!xdpe1e_stream_section(bus, addr, img_buff, sect)) {
			LOG_ERR("Section %d (hc: 0x%x, xv: 0x%x, loop: %d): write scratchpad fail",
				s, sect->header_code, sect->xvcode, sect->loop);
			goto restore_protect;
		}

		if (!xdpe1e_otp_config_store(bus, addr, size_bytes)) {
			LOG_ERR("Section %d (hc: 0x%x, xv: 0x%x): OTP config store fail", s,
				sect->header_code, sect->xvcode);
			goto restore_protect;
		}

		// Soak is 2 ms per byte uploaded, plus margin before the poll gives up
		if (!xdpe1e_wait_upload_done(bus, addr, size_bytes * 2 + 1000)) {
			LOG_ERR("Section %d (hc: 0x%x, xv: 0x%x): upload did not complete", s,
				sect->header_code, sect->xvcode);
			goto restore_protect;
		}

		if (!xdpe1e_check_upload_ok(bus, addr)) {
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

	/*
	 * End-to-end verification: the total configuration checksum is the sum
	 * of every non-trim header and data CRC in OTP, and the config file
	 * carries the value it should produce.
	 */
	if (ret && file->has_checksum) {
		uint32_t device_crc = 0;

		if (!xdpe1e3g6a_get_config_checksum(bus, addr, &device_crc)) {
			LOG_ERR("Could not read back the configuration checksum");
			ret = false;
		} else if (device_crc != file->cfg_checksum) {
			LOG_ERR("Configuration checksum mismatch: device 0x%08x, file 0x%08x",
				device_crc, file->cfg_checksum);
			ret = false;
		} else {
			LOG_INF("Configuration checksum verified: 0x%08x", device_crc);
		}
	} else if (ret) {
		LOG_WRN("Config file carries no Configuration Checksum, skipping verification");
	}

exit:
	SAFE_FREE(file);
	return ret;
}

bool xdpe1e3g6a_fwupdate(uint8_t bus, uint8_t addr, uint8_t *img_buff, uint32_t img_size)
{
	return xdpe1e_fwupdate(XDPE1E_DEV_3G6A, bus, addr, img_buff, img_size);
}

bool xdpe1e496a_fwupdate(uint8_t bus, uint8_t addr, uint8_t *img_buff, uint32_t img_size)
{
	return xdpe1e_fwupdate(XDPE1E_DEV_496A, bus, addr, img_buff, img_size);
}
