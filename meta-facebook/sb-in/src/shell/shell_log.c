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

#include <shell/shell.h>
#include <stdlib.h>
#include <stdio.h>
#include "plat_log.h"
#include "plat_fru.h"
#include "plat_cpld.h"
#include "plat_hook.h"

typedef struct {
	uint8_t cpld_offset;
	const char *reg_name;
	const char *bit_name[8];
} cpld_bit_name_table_t;

const cpld_bit_name_table_t cpld_bit_name_table[] = {
	// bit_name[n] is bit n (bit0 first, bit7 last)
	{ VR_POWER_FAULT_1_REG,
	  "VR Power Fault   (1:Power Fault, 0=Normal)",
	  {
		  "PWRGD_P5V_R",
		  "PWRGD_LDO_IN_1V2_R",
		  "PWRGD_P3V3_CLK_1_R",
		  "PWRGD_P3V3_CLK_R",
		  "PWRGD_P4V2_R",
		  "PWRGD_P1V8_RC38108",
		  "PWRGD_P3V3_R",
		  "P12V_UBC_PWRGD",
	  } },
	{ VR_POWER_FAULT_2_REG,
	  "VR Power Fault   (1:Power Fault, 0=Normal)",
	  {
		  "PWRGD_ZORA01_VDDL",
		  "PWRGD_ZORA00_VDDL",
		  "PWRGD_ZORA11_VDDH",
		  "PWRGD_ZORA10_VDDH",
		  "PWRGD_ZORA01_VDDH",
		  "PWRGD_ZORA00_VDDH",
		  "PWRGD_P0V75_AVDD_HCSL_R",
		  "PWRGD_P1V8_R",
	  } },
	{ VR_POWER_FAULT_3_REG,
	  "VR Power Fault   (1:Power Fault, 0=Normal)",
	  {
		  "PWRGD_MAX_M_VDD_R",
		  "PWRGD_MAX_EW2_VDD_R",
		  "PWRGD_MAX_N_VDD_R",
		  "PWRGD_OWL_W_VDD_R",
		  "PWRGD_OWL_E_VDD_R",
		  "PWRGD_HAMSA_VDD_R",
		  "PWRGD_ZORA11_VDDL",
		  "PWRGD_ZORA10_VDDL",
	  } },
	{ VR_POWER_FAULT_4_REG,
	  "VR Power Fault   (1:Power Fault, 0=Normal)",
	  {
		  "PWRGD_P1V5_PLL_VDDA_SOC",
		  "PWRGD_P1V5_PLL_VDDA_OWL",
		  "PWRGD_VDDPHY_HBM2367_R",
		  "PWRGD_VDDPHY_HBM0145_R",
		  "PWRGD_OWL_W_TRVDD0P75_R",
		  "PWRGD_OWL_E_TRVDD0P75_R",
		  "PWRGD_MAX_S_VDD_R",
		  "PWRGD_MAX_EW1_VDD_R",
	  } },
	{ VR_POWER_FAULT_5_REG,
	  "VR Power Fault   (1:Power Fault, 0=Normal)",
	  {
		  "PWRGD_VDDQ_HBM0145_R",
		  "PWRGD_VDDQ_HBM2367_R",
		  "PWRGD_VDDC_HBM0145_R",
		  "PWRGD_VDDC_HBM2367_R",
		  "PWRGD_VPP_HBM2367_R",
		  "PWRGD_VPP_HBM0145_R",
		  "PWRGD_P1V2_PLL_VDDA_OWL_W",
		  "PWRGD_P1V2_PLL_VDDA_SOC",
	  } },
	{ VR_POWER_FAULT_6_REG,
	  "VR Power Fault   (1:Power Fault, 0=Normal)",
	  {
		  "PWRGD_PVDD1P5",
		  "PWRGD_P0V9_OWL_W_PVDD",
		  "PWRGD_P0V9_OWL_E_PVDD",
		  "PWRGD_OWL_W_TRVDD0P9_R",
		  "PWRGD_OWL_E_TRVDD0P9_R",
		  "PWRGD_HAMSA_AVDD_PCIE_R",
		  "PWRGD_VDDQL_HBM0145_R",
		  "PWRGD_VDDQL_HBM2367_R",
	  } },
	{ VR_POWER_FAULT_7_REG,
	  "VR Power Fault   (1:Power Fault, 0=Normal)",
	  {
		  "1'b1",
		  "1'b0",
		  "PWRGD_HAMSA_VDDHRXTX_PCIE3_R",
		  "PWRGD_HAMSA_VDDHRXTX_PCIE2_R",
		  "PWRGD_HAMSA_VDDHRXTX_PCIE1_R",
		  "PWRGD_HAMSA_VDDHRXTX_PCIE0_R",
		  "PWRGD_P1V5_W_RVDD",
		  "PWRGD_P1V5_E_RVDD",
	  } },
	{ SMBUS_ALERT_1_REG,
	  "SMBus Alert   (0:Alert, 1=Normal)",
	  {
		  "RSVD",
		  "RSVD",
		  "RSVD",
		  "RSVD",
		  "RSVD",
		  "RSVD",
		  "RSVD",
		  "RSVD",
	  } },
	{ SMBUS_ALERT_2_REG,
	  "SMBus Alert   (0:Alert, 1=Normal)",
	  {
		  "RSVD",
		  "RSVD",
		  "RSVD",
		  "RSVD",
		  "RSVD",
		  "RSVD",
		  "RSVD",
		  "RSVD",
	  } }
};

const char *get_cpld_reg_name(uint8_t cpld_offset)
{
	for (int i = 0; i < ARRAY_SIZE(cpld_bit_name_table); i++) {
		if (cpld_bit_name_table[i].cpld_offset == cpld_offset) {
			return cpld_bit_name_table[i].reg_name;
		}
	}
	return "NA";
}

const char *get_cpld_bit_name(uint8_t cpld_offset, uint8_t bit_pos)
{
	for (int i = 0; i < ARRAY_SIZE(cpld_bit_name_table); i++) {
		if (cpld_bit_name_table[i].cpld_offset == cpld_offset) {
			if (bit_pos < 8 && cpld_bit_name_table[i].bit_name[bit_pos]) {
				return cpld_bit_name_table[i].bit_name[bit_pos];
			}
		}
	}
	return "NA";
}

void cmd_set_event(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 3) {
		shell_warn(shell, "Help: test log set_event <error_code_1> <error_code_2> ");
		shell_warn(shell, "error_code_1: high byte, error_code_2: low byte");
		return;
	}

	uint16_t error_code = ((strtol(argv[1], NULL, 16)) << 8) | (strtol(argv[2], NULL, 16));
	shell_print(shell, "Generate error code: 0x%x", error_code);

	error_log_event(error_code, LOG_ASSERT);

	return;
}

void cmd_log_dump(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 1) {
		shell_warn(shell, "Help: test log log_dump");
		return;
	}

	shell_print(shell, "Log number: %d", plat_log_get_num());

	shell_print(
		shell,
		"=============================      LOG DUMP START      =============================");
	for (int i = 0; i < plat_log_get_num(); i++) {
		plat_err_log_mapping log = { 0 };
		plat_log_read((uint8_t *)&log, FRU_LOG_SIZE, i + 1);

		uint8_t err_type = (log.err_code >> 13) & 0x07;

		shell_print(shell, "index %d:", log.index);
		shell_print(shell, "error_code: 0x%x", log.err_code);

		uint8_t cpld_offset = log.err_code & 0xFF;
		uint8_t bit_position = (log.err_code >> 8) & 0x07;

		const char *reg_name = get_cpld_reg_name(cpld_offset);
		const char *bit_name = get_cpld_bit_name(cpld_offset, bit_position);

		switch (err_type) {
		case CPLD_UNEXPECTED_VAL_TRIGGER_CAUSE:
			if (cpld_offset == SMBUS_ALERT_1_REG || cpld_offset == SMBUS_ALERT_2_REG) {
				shell_print(shell, "\t%s", reg_name);
				shell_print(shell, "\t\t%s", bit_name);

				uint8_t vr_index;
				if (!get_smb_alert_vr_index(cpld_offset, bit_position, &vr_index)) {
					shell_print(shell, "no VR index mapped for this bit yet");
					break;
				}

				const uint8_t *rails;
				uint8_t rail_count;
				if (!vr_index_get_rails(vr_index, &rails, &rail_count)) {
					shell_print(shell, "invalid VR index %d", vr_index);
					break;
				}

				for (int j = 0; j < rail_count; j++) {
					uint8_t *rail_name;
					uint8_t idx = j * 2;

					if (!vr_rail_name_get(rails[j], &rail_name)) {
						shell_print(
							shell,
							"Unknown VR rail(%u) status word(0x79):",
							rails[j]);
					} else {
						shell_print(shell, "[0x%02x] %s status word(0x79):",
							    rails[j], rail_name);
					}
					shell_print(shell, "\tlow  byte: 0x%02x",
						    log.error_data[idx]);
					shell_print(shell, "\thigh byte: 0x%02x",
						    log.error_data[idx + 1]);
				}
			} else {
				shell_print(shell, "\t%s", reg_name);
				shell_print(shell, "\t\t%s", bit_name);
				shell_print(shell, "read vr sensor status word(0x79):");
				shell_print(shell, "\tlow  byte: 0x%02x", log.error_data[0]);
				shell_print(shell, "\thigh byte: 0x%02x", log.error_data[1]);
				shell_print(shell, "read vr sensor status vout(0x20): 0x%02x",
					    log.error_data[2]);
				shell_print(shell, "read vr sensor status iout(0x21): 0x%02x",
					    log.error_data[3]);
				shell_print(shell, "read vr sensor status input(0x22): 0x%02x",
					    log.error_data[4]);
				shell_print(shell,
					    "read vr sensor status temperature(0x24): 0x%02x",
					    log.error_data[5]);
				shell_print(shell, "read vr sensor status CML(0x7e): 0x%02x",
					    log.error_data[6]);
			}
			break;
		case POWER_ON_SEQUENCE_TRIGGER_CAUSE:
			shell_print(shell, "\tPOWER_ON_SEQUENCE_TRIGGER");
			break;
		case AC_ON_TRIGGER_CAUSE:
			shell_print(shell, "\tAC_ON");
			break;
		case DC_ON_TRIGGER_CAUSE:
			shell_print(shell, "\tDC_ON_DETECTED");
			break;
		default:
			shell_print(shell, "Unknown error type: %d", err_type);
			break;
		}

		shell_print(shell, "sys_time: %lld ms", log.sys_time);
		shell_print(shell, "error_data:");
		shell_hexdump(shell, log.error_data, sizeof(log.error_data));
		shell_print(shell, "cpld register: start offset 0x%02x",
			    CPLD_REGISTER_1ST_PART_START_OFFSET);
		shell_hexdump(shell, log.cpld_dump, CPLD_REGISTER_1ST_PART_NUM);
		shell_print(
			shell,
			"====================================================================================");
	}

	shell_print(
		shell,
		"=============================       LOG DUMP END       =============================");

	return;
}

void cmd_test_read(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 4) {
		shell_warn(shell, "Help: test log test_read <offset_1> <offset_2> <length>");
		return;
	}

	uint16_t offset = ((strtol(argv[1], NULL, 16)) << 8) | (strtol(argv[2], NULL, 16));
	printf("offset = 0x%04X\n", offset);

	int length = strtol(argv[3], NULL, 16);

	uint8_t log_data[128] = { 0 };
	plat_eeprom_read(offset, log_data, length);
	printf("FRU_LOG_SIZE = %d\n", FRU_LOG_SIZE);

	shell_hexdump(shell, log_data, sizeof(uint8_t) * FRU_LOG_SIZE);

	return;
}

void cmd_log_clear(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 1) {
		shell_warn(shell, "Help: test log log_clear");
		return;
	}

	k_msleep(1000);

	plat_clear_log();
	shell_print(shell, "plat_clear_log finished!");

	return;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_blackbox_cmd, SHELL_CMD(get, NULL, "log_dump", cmd_log_dump),
			       SHELL_CMD(clear, NULL, "log_clear", cmd_log_clear),
			       SHELL_CMD(set_event, NULL, "set_event", cmd_set_event),
			       SHELL_CMD(test_read, NULL, "test_read", cmd_test_read),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(blackbox, &sub_blackbox_cmd, "blackbox get/clear commands", NULL);
