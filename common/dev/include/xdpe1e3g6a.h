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

/*
 * Infineon XDPE1Exxx digital multi-phase controller family.
 *
 *   XDPE1E3G6A - 3 loops, PMBus pages 0..2
 *   XDPE1E496A - 4 loops / 9 phases, PMBus pages 0..3
 *
 * Both parts share the PMBus + MFR command map and the MFR_FW_COMMAND (FEh)
 * driven OTP configuration flow described in the XDPE1Exxx Family Configuration
 * Programming Guide (ANxxx-XDPE1Exxx), so the implementation is shared.
 *
 * The device type is supplied by the caller (sensor table / PLDM component
 * mapping) rather than probed, so wiring the wrong part to a rail is a build
 * time mistake. The product ID read from IC_DEVICE_ID is used only to confirm
 * that choice before anything destructive happens.
 */
enum XDPE1E_DEV {
	XDPE1E_DEV_3G6A = 0,
	XDPE1E_DEV_496A,
	XDPE1E_DEV_MAX,
};

// IC_DEVICE_ID (ADh) product ID byte, programming guide Table 1
enum XDPE1E_PRODUCT_ID {
	XDPE1E3G6_PRODUCT_ID = 0xB0,
	XDPE1E496_PRODUCT_ID = 0xB1,
	XDPE1E2G5_PRODUCT_ID = 0xB3,
	XDPE1E2C6_PRODUCT_ID = 0xB4,
	XDPE1E2G6_PRODUCT_ID = 0xB6,
	XDPE1E2G3_PRODUCT_ID = 0xB9,
};

// IC_DEVICE_ID (ADh) revision byte, programming guide Table 6
enum XDPE1E_REVISION {
	XDPE1E_REV_A = 0x00,
	XDPE1E_REV_B = 0x01,
};

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

// STATUS_BYTE (78h) bit definitions. Identical on both parts.
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

/*
 * EXT_WRITE_PROTECT (D6h), XDPE1E496A datasheet Table 10.
 *
 * A second write-protect layer, independent of WRITE_PROTECT (10h). Clearing
 * 10h does not clear these bits, writes to D6h only take effect after a chip
 * reset, and bit<0> is a one-way latch.
 */
enum XDPE1E496A_EXT_WRITE_PROTECT_BIT {
	XDPE1E496A_EXT_WP_LOCK_BIT = BIT(0), // not removable once set
	XDPE1E496A_EXT_WP_MASK_LOCK_BIT = BIT(1),
	XDPE1E496A_EXT_WP_VOLTAGE_DYNAMICS_BIT = BIT(2),
	XDPE1E496A_EXT_WP_STORE_NVM_BIT = BIT(3), // blocks the OTP config store
	XDPE1E496A_EXT_WP_RELOAD_NVM_BIT = BIT(4),
	XDPE1E496A_EXT_WP_PART_ATTRIBUTES_BIT = BIT(5),
	XDPE1E496A_EXT_WP_FAULT_CONFIG_BIT = BIT(6),
	XDPE1E496A_EXT_WP_MFR_BIT = BIT(7), // blocks MFR_AHB_ADDRESS / MFR_REG_WRITE
};

// MFR_DISABLE_SECURITY_ONCE (CBh) read-back access state, XDPE1E496A only
enum XDPE1E496A_SECURITY_STATE {
	XDPE1E496A_SECURITY_UNLOCKED = 0,
	XDPE1E496A_SECURITY_LOCKED = 1,
	XDPE1E496A_SECURITY_DEADLOCKED = 2, // requires a reset to unlock
};

/*
 * PAGE (00h) loop select, and the loop field in the configuration section
 * header. Loop D exists on XDPE1E496A only. Loop 4 is the Psys pseudo-loop:
 * it is a valid section header / MFR_FW_COMMAND loop value on both parts, but
 * it is NOT a PMBus page (the Psys PMBus page is 0x0D).
 */
enum XDPE1E3G6A_LOOP {
	XDPE1E3G6A_LOOP_A = 0x00,
	XDPE1E3G6A_LOOP_B = 0x01,
	XDPE1E3G6A_LOOP_C = 0x02,
	XDPE1E496A_LOOP_D = 0x03,
	XDPE1E_LOOP_PSYS = 0x04,
};

#define XDPE1E_PAGE_ALL 0xFF
#define XDPE1E_PAGE_PSYS 0x0D

// CRC_CHECKSUM (B8h) block length. 8 bytes on every part in the family.
// Size caller buffers with this.
#define XDPE1E_CRC_CHECKSUM_LEN 8

// Retained so existing callers keep compiling
#define XDPE1E3G6A_CRC_CHECKSUM_LEN XDPE1E_CRC_CHECKSUM_LEN
#define XDPE1E496A_CRC_CHECKSUM_LEN XDPE1E_CRC_CHECKSUM_LEN

/* ---- device independent ---- */
bool xdpe1e3g6a_set_write_protect(uint8_t bus, uint8_t addr, uint8_t option);
void xdpe1e3g6a_set_write_protect_default_val(uint8_t val);
bool xdpe1e3g6a_get_status_byte(uint8_t bus, uint8_t addr, uint8_t *data);
bool xdpe1e3g6a_get_config_checksum(uint8_t bus, uint8_t addr, uint32_t *checksum);

// IC_DEVICE_ID (ADh): 2-byte block read, byte0 = revision, byte1 = product ID
bool xdpe1e_get_device_id(uint8_t bus, uint8_t addr, uint8_t *product_id, uint8_t *revision);

// OTP_PARTITION_SIZE_REMAINING (FEh/10h). Partition 0 holds the configuration (32 kB).
bool xdpe1e_get_otp_remaining(uint8_t bus, uint8_t addr, uint8_t partition, uint32_t *bytes);

/*
 * FW_RESET (FEh/0Eh). Reinitializes the controller so registers are re-read
 * from OTP. Per programming guide chapter 10.3 this may only be issued with the
 * 12 V input OFF and with the controller out of regulation, and if the new
 * configuration changes the address decode the device may come back at a
 * different PMBus address.
 */
bool xdpe1e3g6a_fw_reset(uint8_t bus, uint8_t addr);

/* ---- XDPE1E3G6A ---- */
// checksum buffer must be at least XDPE1E_CRC_CHECKSUM_LEN bytes
bool xdpe1e3g6a_get_checksum(uint8_t bus, uint8_t addr, uint8_t *checksum);
bool xdpe1e3g6a_get_vout_max(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt);
bool xdpe1e3g6a_get_vout_min(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt);
bool xdpe1e3g6a_set_vout_max(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt);
bool xdpe1e3g6a_set_vout_min(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt);
bool xdpe1e3g6a_fwupdate(uint8_t bus, uint8_t addr, uint8_t *img_buff, uint32_t img_size);
uint8_t xdpe1e3g6a_init(sensor_cfg *cfg);

/* ---- XDPE1E496A ---- */
// checksum buffer must be at least XDPE1E_CRC_CHECKSUM_LEN bytes
bool xdpe1e496a_get_checksum(uint8_t bus, uint8_t addr, uint8_t *checksum);
bool xdpe1e496a_get_vout_max(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt);
bool xdpe1e496a_get_vout_min(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt);
bool xdpe1e496a_set_vout_max(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt);
bool xdpe1e496a_set_vout_min(sensor_cfg *cfg, uint8_t rail, uint16_t *millivolt);
bool xdpe1e496a_fwupdate(uint8_t bus, uint8_t addr, uint8_t *img_buff, uint32_t img_size);
uint8_t xdpe1e496a_init(sensor_cfg *cfg);

// Pre-flight helpers, XDPE1E496A only. Exposed so platform code can surface a
// clear reason before starting a long PLDM transfer that would fail anyway.
bool xdpe1e496a_get_security_state(uint8_t bus, uint8_t addr, uint8_t *state);
bool xdpe1e496a_get_ext_write_protect(uint8_t bus, uint8_t addr, uint8_t *val);

#endif
