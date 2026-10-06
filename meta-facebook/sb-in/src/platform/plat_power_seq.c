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
#include <string.h>
#include <logging/log.h>
#include "libutil.h"
#include "plat_cpld.h"
#include "plat_log.h"
#include "plat_power_seq.h"

LOG_MODULE_REGISTER(plat_power_seq);

typedef struct _power_seq_pwrgd_ {
	uint8_t index;
	uint8_t cpld_offset;
	uint8_t bit;
	char *name;
} power_seq_pwrgd;

/* PWRGD event latch, in power sequence order.
 * Latch bit 0 means the rail never reached PWRGD.
 * 0xC4[1] Timeout Fault Status / [0] Power Fault Status are not PWRGD, not listed.
 */
// clang-format off
static const power_seq_pwrgd power_seq_pwrgd_table[] = {
	{ 0, 0xBE, 7, "P12V_UBC1" },
	{ 1, 0xBE, 6, "P12V_UBC2" },
	{ 2, 0xBE, 5, "P3V3" },
	{ 3, 0xBE, 4, "P1V8_RC38108" },
	{ 4, 0xBE, 3, "P4V2" },
	{ 5, 0xBE, 2, "P3V3_CLK" },
	{ 6, 0xBE, 1, "P3V3_CLK_1" },
	{ 7, 0xBE, 0, "LDO_IN_1V2" },
	{ 8, 0xBF, 7, "P5V" },
	{ 9, 0xBF, 6, "P1V8" },
	{ 10, 0xBF, 5, "P0V75_AVDD_HCSL" },
	{ 11, 0xBF, 4, "ZORA00_VDDH" },
	{ 12, 0xBF, 3, "ZORA01_VDDH" },
	{ 13, 0xBF, 2, "ZORA10_VDDH" },
	{ 14, 0xBF, 1, "ZORA11_VDDH" },
	{ 15, 0xBF, 0, "ZORA00_VDDL" },
	{ 16, 0xC0, 7, "ZORA01_VDDL" },
	{ 17, 0xC0, 6, "ZORA10_VDDL" },
	{ 18, 0xC0, 5, "ZORA11_VDDL" },
	{ 19, 0xC0, 4, "HAMSA_VDD" },
	{ 20, 0xC0, 3, "OWL_E_VDD" },
	{ 21, 0xC0, 2, "OWL_W_VDD" },
	{ 22, 0xC0, 1, "MAX_N_VDD" },
	{ 23, 0xC0, 0, "MAX_EW2_VDD" },
	{ 24, 0xC1, 7, "MAX_M_VDD" },
	{ 25, 0xC1, 6, "MAX_EW1_VDD" },
	{ 26, 0xC1, 5, "MAX_S_VDD" },
	{ 27, 0xC1, 4, "OWL_E_TRVDD0P75" },
	{ 28, 0xC1, 3, "OWL_W_TRVDD0P75" },
	{ 29, 0xC1, 2, "VDDPHY_HBM0145" },
	{ 30, 0xC1, 1, "VDDPHY_HBM2367" },
	{ 31, 0xC1, 0, "P1V5_PLL_VDDA_OWL_W" },
	{ 32, 0xC2, 7, "P1V5_PLL_VDDA_OWL_E" },
	{ 33, 0xC2, 6, "P1V5_PLL_VDDA_SOC" },
	{ 34, 0xC2, 5, "P1V2_PLL_VDDA_SOC" },
	{ 35, 0xC2, 4, "P1V2_PLL_VDDA_OWL_W" },
	{ 36, 0xC2, 3, "VPP_HBM0145" },
	{ 37, 0xC2, 2, "VPP_HBM2367" },
	{ 38, 0xC2, 1, "VDDC_HBM0145" },
	{ 39, 0xC2, 0, "VDDC_HBM2367" },
	{ 40, 0xC3, 7, "VDDQ_HBM0145" },
	{ 41, 0xC3, 6, "VDDQ_HBM2367" },
	{ 42, 0xC3, 5, "VDDQL_HBM0145" },
	{ 43, 0xC3, 4, "VDDQL_HBM2367" },
	{ 44, 0xC3, 3, "HAMSA_AVDD_PCIE" },
	{ 45, 0xC3, 2, "OWL_E_TRVDD0P9" },
	{ 46, 0xC3, 1, "OWL_W_TRVDD0P9" },
	{ 47, 0xC3, 0, "P0V9_OWL_E_PVDD" },
	{ 48, 0xC4, 7, "P0V9_OWL_W_PVDD" },
	{ 49, 0xC4, 6, "PVDD1P5" },
	{ 50, 0xC4, 5, "P1V5_E_RVDD" },
	{ 51, 0xC4, 4, "P1V5_W_RVDD" },
	{ 52, 0xC4, 3, "HAMSA_VDDHRXTX_PCIE" },
	{ 53, 0xC4, 2, "ASIC_POWER_ON_RESET" },
};
// clang-format on

static uint8_t power_seq_fail_id = POWER_SEQ_FAIL_ID_NONE;
static uint8_t power_seq_latch[PWRGD_EVENT_LATCH_NUM];

static void plat_find_power_seq_fail(void)
{
	power_seq_fail_id = POWER_SEQ_FAIL_ID_NONE;

	if (!plat_read_cpld(PWRGD_EVENT_LATCH_1_REG, power_seq_latch, PWRGD_EVENT_LATCH_NUM)) {
		LOG_ERR("Failed to read PWRGD event latch");
		memset(power_seq_latch, 0, sizeof(power_seq_latch));
		return;
	}

	for (size_t i = 0; i < ARRAY_SIZE(power_seq_pwrgd_table); i++) {
		const power_seq_pwrgd *entry = &power_seq_pwrgd_table[i];
		uint8_t data = power_seq_latch[entry->cpld_offset - PWRGD_EVENT_LATCH_1_REG];

		if (!(data & BIT(entry->bit))) {
			power_seq_fail_id = entry->index;
			LOG_ERR("Power sequence fail at [%d] %s", entry->index, entry->name);
			return;
		}
	}

	LOG_WRN("Power sequence fault but all PWRGD latched");
}

void plat_power_seq_fault_handler(void)
{
	// fail id and latch must be ready before error_log_event() calls get_error_data()
	plat_find_power_seq_fail();
	error_log_event(MAKE_ERR_CODE(POWER_ON_SEQUENCE_TRIGGER_CAUSE, 0), LOG_ASSERT);
}

void plat_clear_power_seq_fault(void)
{
	error_log_event(MAKE_ERR_CODE(POWER_ON_SEQUENCE_TRIGGER_CAUSE, 0), LOG_DEASSERT);
}

uint8_t plat_get_power_seq_fail_id(void)
{
	return power_seq_fail_id;
}

const char *plat_get_power_seq_name(uint8_t index)
{
	for (size_t i = 0; i < ARRAY_SIZE(power_seq_pwrgd_table); i++) {
		if (power_seq_pwrgd_table[i].index == index)
			return power_seq_pwrgd_table[i].name;
	}

	return "UNKNOWN";
}

void plat_get_power_seq_latch(uint8_t *data)
{
	CHECK_NULL_ARG(data);

	memcpy(data, power_seq_latch, sizeof(power_seq_latch));
}
