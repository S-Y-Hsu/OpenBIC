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
#include <shell/shell.h>
#include <stdlib.h>
#include "libutil.h"
#include "plat_cpld.h"

/* CPLD records each rail's sequence timing, unit: 2ms */
#define POWER_SEQUENCE_TIME_UNIT_MS 2

typedef struct _power_sequence_timing_ {
	uint8_t cpld_offset;
	char *name;
} power_sequence_timing;

typedef struct _power_sequence_record_ {
	uint8_t index;
	uint8_t value;
} power_sequence_record;

// clang-format off
/* Power-up timing, from FM_ASTRID_EN to each PWRGD */
static const power_sequence_timing power_up_table[] = {
	{ 0x43, "P4V2" },
	{ 0x44, "P3V3_CLK" },
	{ 0x45, "P3V3_CLK_1" },
	{ 0x46, "LDO_IN_1V2" },
	{ 0x47, "P5V" },
	{ 0x48, "P1V8" },
	{ 0x49, "P0V75_AVDD_HCSL" },
	{ 0x4A, "ZORA00_VDDH" },
	{ 0x4B, "ZORA01_VDDH" },
	{ 0x4C, "ZORA10_VDDH" },
	{ 0x4D, "ZORA11_VDDH" },
	{ 0x4E, "ZORA00_VDDL" },
	{ 0x4F, "ZORA01_VDDL" },
	{ 0x50, "ZORA10_VDDL" },
	{ 0x51, "ZORA11_VDDL" },
	{ 0x52, "HAMSA_VDD" },
	{ 0x53, "OWL_E_VDD" },
	{ 0x54, "OWL_W_VDD" },
	{ 0x55, "MAX_N_VDD" },
	{ 0x56, "MAX_EW2_VDD" },
	{ 0x57, "MAX_M_VDD" },
	{ 0x58, "MAX_EW1_VDD" },
	{ 0x59, "MAX_S_VDD" },
	{ 0x5A, "OWL_E_TRVDD0P75" },
	{ 0x5B, "OWL_W_TRVDD0P75" },
	{ 0x5C, "VDDPHY_HBM0145" },
	{ 0x5D, "VDDPHY_HBM2367" },
	{ 0x5E, "P1V5_PLL_VDDA_OWL_E" },
	{ 0x5F, "P1V5_PLL_VDDA_OWL_W" },
	{ 0x60, "P1V5_PLL_VDDA_SOC" },
	{ 0x61, "P1V2_PLL_VDDA_OWL_W" },
	{ 0x62, "P1V2_PLL_VDDA_SOC" },
	{ 0x63, "ASTRID_CLK_48MHZ" },
	{ 0x64, "ASTRID_CLK_100MHZ" },
	{ 0x65, "ASTRID_CLK_312.5MHZ" },
	{ 0x66, "VPP_HBM0145" },
	{ 0x67, "VPP_HBM2367" },
	{ 0x68, "VDDC_HBM0145" },
	{ 0x69, "VDDC_HBM2367" },
	{ 0x6A, "VDDQ_HBM0145" },
	{ 0x6B, "VDDQ_HBM2367" },
	{ 0x6C, "VDDQL_HBM0145" },
	{ 0x6D, "VDDQL_HBM2367" },
	{ 0x6E, "HAMSA_AVDD_PCIE" },
	{ 0x6F, "OWL_E_TRVDD0P9" },
	{ 0x70, "OWL_W_TRVDD0P9" },
	{ 0x71, "OWL_E_PVDD0P9" },
	{ 0x72, "OWL_W_PVDD0P9" },
	{ 0xA1, "PVDD1P5" },
	{ 0xA2, "OWL_E_RVDD1P5" },
	{ 0xA6, "OWL_W_RVDD1P5" },
	{ 0xA7, "HAMSA_VDDHRXTX_PCIE" },
	{ 0xA8, "HAMSA_POWER_ON_RESET_PLD_L" },
	{ 0xAA, "ZORA00_POWER_ON_RESET_PLD_L" },
	{ 0xAB, "ZORA01_POWER_ON_RESET_PLD_L" },
	{ 0xAC, "ZORA10_POWER_ON_RESET_PLD_L" },
	{ 0xAD, "ZORA11_POWER_ON_RESET_PLD_L" },
	{ 0xAE, "HAMSA_SYS_RST_PLD_L" },
	{ 0xAF, "ZORA00_SYS_RST_PLD_L" },
	{ 0xB0, "ZORA01_SYS_RST_PLD_L" },
	{ 0xB1, "ZORA10_SYS_RST_PLD_L" },
	{ 0xBC, "ZORA11_SYS_RST_PLD_L" },
};

/* Power-down timing, from RST_ASTRID_PWR_ON_PLD_N to each PWRGD */
static const power_sequence_timing power_down_table[] = {
	{ 0x73, "HAMSA_VDDHRXTX_PCIE" },
	{ 0x74, "P1V5_E_RVDD" },
	{ 0x75, "PVDD1P5" },
	{ 0x76, "P0V9_OWL_W_PVDD" },
	{ 0x77, "OWL_E_TRVDD0P9" },
	{ 0x78, "OWL_W_TRVDD0P9" },
	{ 0x79, "HAMSA_AVDD_PCIE" },
	{ 0x7A, "VDDQL_HBM2367" },
	{ 0x7B, "VDDQL_HBM0145" },
	{ 0x7C, "VDDQ_HBM2367" },
	{ 0x7D, "VDDQ_HBM0145" },
	{ 0x7E, "VDDC_HBM2367" },
	{ 0x7F, "VDDC_HBM0145" },
	{ 0x80, "VPP_HBM2367" },
	{ 0x81, "VPP_HBM0145" },
	{ 0x82, "P1V5_PLL_VDDA_OWL_E" },
	{ 0x83, "VDDPHY_HBM2367" },
	{ 0x84, "VDDPHY_HBM0145" },
	{ 0x85, "OWL_E_TRVDD0P75" },
	{ 0x86, "OWL_W_TRVDD0P75" },
	{ 0x87, "MAX_S_VDD" },
	{ 0x88, "MAX_EW1_VDD" },
	{ 0x89, "MAX_M_VDD" },
	{ 0x8A, "MAX_EW2_VDD" },
	{ 0x8B, "MAX_N_VDD" },
	{ 0x8C, "OWL_E_VDD" },
	{ 0x8D, "OWL_W_VDD" },
	{ 0x8E, "HAMSA_VDD" },
	{ 0x8F, "ZORA11_VDDL" },
	{ 0x90, "ZORA10_VDDL" },
	{ 0x91, "ZORA01_VDDL" },
	{ 0x92, "ZORA00_VDDL" },
	{ 0x93, "ZORA11_VDDH" },
	{ 0x94, "ZORA10_VDDH" },
	{ 0x95, "ZORA01_VDDH" },
	{ 0x96, "ZORA00_VDDH" },
	{ 0x97, "P0V75_AVDD_HCSL" },
	{ 0x98, "P1V8" },
	{ 0x99, "P5V" },
	{ 0x9A, "LDO_IN_1V2" },
	{ 0x9B, "P3V3_CLK_1" },
	{ 0x9C, "P4V2" },
};
// clang-format on

#define POWER_SEQUENCE_RECORD_MAX MAX(ARRAY_SIZE(power_up_table), ARRAY_SIZE(power_down_table))

static void show_power_sequence(const struct shell *shell, const power_sequence_timing *table,
				size_t size)
{
	power_sequence_record record[POWER_SEQUENCE_RECORD_MAX];

	for (size_t i = 0; i < size; i++) {
		record[i].index = i;
		if (!plat_read_cpld(table[i].cpld_offset, &record[i].value, 1)) {
			shell_error(shell, "Failed to read CPLD offset 0x%02x",
				    table[i].cpld_offset);
			record[i].value = 0;
		}
	}

	// sort by time, keep table order for the same time
	for (size_t i = 1; i < size; i++) {
		power_sequence_record key = record[i];
		size_t j = i;
		while (j > 0 && record[j - 1].value > key.value) {
			record[j] = record[j - 1];
			j--;
		}
		record[j] = key;
	}

	for (size_t i = 0; i < size; i++) {
		const power_sequence_timing *entry = &table[record[i].index];
		shell_print(shell, "    [%2d] 0x%02x %-30s %d ms", i, entry->cpld_offset,
			    entry->name, record[i].value * POWER_SEQUENCE_TIME_UNIT_MS);
	}
}

static int cmd_power_sequence_power_up(const struct shell *shell, size_t argc, char **argv)
{
	shell_print(shell, "FM_ASTRID_EN ->");
	show_power_sequence(shell, power_up_table, ARRAY_SIZE(power_up_table));
	return 0;
}

static int cmd_power_sequence_power_down(const struct shell *shell, size_t argc, char **argv)
{
	shell_print(shell, "RST_ASTRID_PWR_ON_PLD_N ->");
	show_power_sequence(shell, power_down_table, ARRAY_SIZE(power_down_table));
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_power_sequence_cmds,
			       SHELL_CMD(power_up, NULL, "show power-up sequence timing",
					 cmd_power_sequence_power_up),
			       SHELL_CMD(power_down, NULL, "show power-down sequence timing",
					 cmd_power_sequence_power_down),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(power_sequence, &sub_power_sequence_cmds, "power_sequence <power_up|power_down>",
		   NULL);
