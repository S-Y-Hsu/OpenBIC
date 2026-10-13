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
#include "plat_power_seq.h"

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

		uint8_t err_type = ERR_CODE_GET_CAUSE(log.err_code);

		shell_print(shell, "index %d:", log.index);
		shell_print(shell, "error_code: 0x%x", log.err_code);

		uint8_t err_data_len = sizeof(log.error_data);

		switch (err_type) {
		case POWER_FAULT_TRIGGER_CAUSE: {
			uint8_t cpld_offset = log.err_code & 0xFF;
			uint8_t bit_position = (log.err_code >> 8) & 0x07;

			shell_print(shell, "\tVR Power Fault   (1:Power Fault, 0=Normal)");
			shell_print(shell, "\t\t%s", power_fault_get_bit_name(cpld_offset, bit_position));

			uint8_t rail = NOT_VR_RAIL;
			uint8_t event_source = 0;
			if (!power_fault_get_bit(cpld_offset, bit_position, &rail, &event_source) ||
			    rail == NOT_VR_RAIL) {
				shell_print(shell, "not a VR rail, no VR status");
				err_data_len = 0;
				break;
			}

			shell_print(shell, "read vr sensor status word(0x79):");
			shell_print(shell, "\tlow  byte: 0x%02x", log.error_data[0]);
			shell_print(shell, "\thigh byte: 0x%02x", log.error_data[1]);
			shell_print(shell, "read vr sensor status vout(0x7a): 0x%02x",
				    log.error_data[2]);
			shell_print(shell, "read vr sensor status iout(0x7b): 0x%02x",
				    log.error_data[3]);
			shell_print(shell, "read vr sensor status input(0x7c): 0x%02x",
				    log.error_data[4]);
			shell_print(shell, "read vr sensor status temperature(0x7d): 0x%02x",
				    log.error_data[5]);
			shell_print(shell, "read vr sensor status CML(0x7e): 0x%02x",
				    log.error_data[6]);
			err_data_len = 7;
			break;
		}
		case VR_SMB_ALERT_TRIGGER_CAUSE: {
			uint8_t vr_index = ERR_CODE_GET_PAYLOAD(log.err_code);
			shell_print(shell, "\tVR_SMB_ALERT, VR index %d", vr_index);

			const uint8_t *rails;
			uint8_t rail_count;
			if (!vr_index_get_rails(vr_index, &rails, &rail_count)) {
				shell_print(shell, "invalid VR index %d", vr_index);
				err_data_len = 0;
				break;
			}

			err_data_len = rail_count * 2;

			for (int j = 0; j < rail_count; j++) {
				uint8_t *rail_name;
				uint8_t idx = j * 2;

				if (!vr_rail_name_get(rails[j], &rail_name)) {
					shell_print(shell, "Unknown VR rail(%u) status word(0x79):",
						    rails[j]);
				} else {
					shell_print(shell,
						    "[0x%02x] %s status word(0x79):", rails[j],
						    rail_name);
				}
				shell_print(shell, "\tlow  byte: 0x%02x", log.error_data[idx]);
				shell_print(shell, "\thigh byte: 0x%02x", log.error_data[idx + 1]);
			}
			break;
		}
		case POWER_ON_SEQUENCE_TRIGGER_CAUSE: {
			shell_print(shell, "\tPOWER_ON_SEQUENCE_TRIGGER");
			shell_print(shell, "\tRAIL: [%d] %s", log.error_data[0],
				    plat_get_power_seq_name(log.error_data[0]));
			shell_print(
				shell,
				"\tPWRGD latch(0x%02x~0x%02x): %02x %02x %02x %02x %02x %02x %02x",
				PWRGD_EVENT_LATCH_1_REG, PWRGD_EVENT_LATCH_7_REG, log.error_data[1],
				log.error_data[2], log.error_data[3], log.error_data[4],
				log.error_data[5], log.error_data[6], log.error_data[7]);
			err_data_len = 1 + PWRGD_EVENT_LATCH_NUM;
			break;
		}
		case SGPIO_EVENT_TRIGGER_CAUSE:
			shell_print(shell, "\tSGPIO_EVENT");
			shell_print(shell, "\t\t%s",
				    sgpio_event_get_name(ERR_CODE_GET_PAYLOAD(log.err_code)));
			err_data_len = 0;
			break;
		case AC_ON_TRIGGER_CAUSE:
			shell_print(shell, "\tAC_ON");
			err_data_len = 1;
			break;
		case DC_ON_TRIGGER_CAUSE:
			shell_print(shell, "\tDC_ON_DETECTED");
			err_data_len = 1;
			break;
		default:
			shell_print(shell, "Unknown error type: %d", err_type);
			break;
		}

		shell_print(shell, "sys_time: %lld ms", log.sys_time);
		shell_print(shell, "error_data:");
		shell_hexdump(shell, log.error_data, err_data_len);
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
