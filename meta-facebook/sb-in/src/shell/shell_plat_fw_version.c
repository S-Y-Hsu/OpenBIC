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

#include <zephyr.h>
#include <stdlib.h>
#include <shell/shell.h>
#include "plat_cpld.h"
#include "mp2971.h"
#include "mp29526.h"
#include "xdpe1e3g6a.h"
#include "plat_class.h"
#include "plat_pldm_fw_update.h"
#include "plat_pldm_sensor.h"
#include "plat_hook.h"
#include "plat_gpio.h"
#include "hal_gpio.h"
#include "pldm_firmware_update.h"

LOG_MODULE_REGISTER(shell_plat_fw_version);

void cmd_get_fw_version_vr(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 1) {
		shell_warn(shell, "Help: test get_fw_version vr");
		return;
	}

	/* VR PMBus is accessible once P3V3 is up, no need to wait for DC on */
	if (sgpio_get(PWRGD_P3V3_R) != GPIO_HIGH) {
		shell_warn(shell, "PWRGD_P3V3_R is not on, skip get VR version");
		return;
	}

	if (is_update_state_idle_or_learn_comp() == false) {
		shell_warn(shell, "FW update in progress, skip get VR version");
		return;
	}

	/* save the polling flag, stop sensor polling */
	bool polling_flag = get_plat_sensor_polling_enable_flag();
	set_plat_sensor_polling_enable_flag(false);

	shell_print(shell, "comp_id |                rail name               |version |remain");
	for (int i = COMPNT_VR_1; i <= COMPNT_VR_3V3; i++) {
		uint8_t sensor_id = 0;
		char sensor_name[MAX_AUX_SENSOR_NAME_LEN] = { 0 };

		/* same as PLDM get_vr_fw_version: only EVB has the 3V3 VR */
		if (get_asic_board_id() != ASIC_BOARD_ID_EVB && i == COMPNT_VR_3V3)
			continue;

		if (!find_sensor_id_and_name_by_firmware_comp_id(i, &sensor_id, sensor_name)) {
			LOG_ERR("Can't find sensor id and name by comp id: 0x%x", i);
			continue;
		}

		sensor_cfg *cfg = get_sensor_cfg_by_sensor_id(sensor_id);
		if (cfg == NULL)
			continue;

		/* take the VR mutex and switch the mux (if any), like sensor polling does */
		if (cfg->pre_sensor_read_hook &&
		    !cfg->pre_sensor_read_hook(cfg, cfg->pre_sensor_read_args)) {
			shell_print(shell, "comp id 0x%x pre hook fail", i);
			continue;
		}

		uint32_t version = 0;
		uint32_t remain = 0;
		bool has_remain = false;
		bool read_ok = false;
		switch (cfg->type) {
		case sensor_dev_mp29526:
			if (!mp29526_get_fw_version(cfg->port, cfg->target_addr, &version)) {
				shell_print(shell, "comp id 0x%x MP29526 version reading failed",
					    i);
				break;
			}
			read_ok = true;
			break;
		case sensor_dev_xdpe1e3g6a:
			if (!xdpe1e3g6a_get_config_checksum(cfg->port, cfg->target_addr,
							    &version)) {
				shell_print(shell, "comp id 0x%x XDPE1E3G6A version reading failed",
					    i);
				break;
			}
			/* partition 0 = configuration, unit: bytes */
			if (!xdpe1e_get_otp_remaining(cfg->port, cfg->target_addr, 0, &remain)) {
				shell_print(shell,
					    "comp id 0x%x XDPE1E3G6A remaining reading failed", i);
				break;
			}
			has_remain = true;
			read_ok = true;
			break;
		case sensor_dev_mp2971:
			if (!mp2971_get_checksum(cfg->port, cfg->target_addr, &version)) {
				shell_print(shell, "comp id 0x%x MP2971 version reading failed", i);
				break;
			}
			read_ok = true;
			break;
		default:
			shell_print(shell, "comp id 0x%x unsupport sensor_dev: %d", i, cfg->type);
			break;
		}

		if (cfg->post_sensor_read_hook &&
		    !cfg->post_sensor_read_hook(cfg, cfg->post_sensor_read_args, NULL))
			shell_print(shell, "comp id 0x%x post hook fail", i);

		if (!read_ok)
			continue;

		if (has_remain)
			shell_print(shell, "%-8x|%-40s|%08x|%u bytes", i, sensor_name, version,
				    remain);
		else
			shell_print(shell, "%-8x|%-40s|%08x|-", i, sensor_name, version);
	}

	/* set back to the original polling flag */
	set_plat_sensor_polling_enable_flag(polling_flag);

	return;
}

void cmd_get_fw_version_cpld(const struct shell *shell, size_t argc, char **argv)
{
	uint8_t data[4] = { 0 };
	uint32_t version = 0;

	if (!plat_read_cpld(CPLD_OFFSET_USERCODE, data, 4)) {
		shell_warn(shell, "cpld read 0x%02x fail", CPLD_OFFSET_USERCODE);
		return;
	}

	version = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
	shell_print(shell, "The cpld version: %08x", version);
	return;
}

void cmd_get_fw_version_asic(const struct shell *shell, size_t argc, char **argv)
{
	get_fw_version_boot0_from_asic();

	uint32_t version = plat_get_image_version();
	shell_print(shell, "HAMSA boot0 VER from asic: %02d.%02d.%02d.%02d", (version >> 24) & 0xFF,
		    (version >> 16) & 0xFF, (version >> 8) & 0xFF, version & 0xFF);
	return;
}

void cmd_get_fw_version_asic_from_flash(const struct shell *shell, size_t argc, char **argv)
{
	// want to show like VER : 01.06.00 | CRC32 : e9e2b0ba
	uint32_t version = 0;
	uint32_t crc32 = 0;

	if (plat_get_image_crc_checksum_from_flash(&version, &crc32)) {
		shell_print(shell, "HAMSA boot0 VER : %08x | CRC32 : %08x", version, crc32);
	} else {
		shell_warn(shell, "HAMSA boot0 VER and CRC reading failed");
	}

	return;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_get_fw_version_cmd, SHELL_CMD(vr, NULL, "get fw version vr", cmd_get_fw_version_vr),
	SHELL_CMD(cpld, NULL, "get fw version cpld", cmd_get_fw_version_cpld),
	SHELL_CMD(asic, NULL, "get fw version asic", cmd_get_fw_version_asic),
	SHELL_CMD(force_read_asic, NULL, "get fw version asic from flash",
		  cmd_get_fw_version_asic_from_flash),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(get_fw_version, &sub_get_fw_version_cmd, "get fw version command", NULL);