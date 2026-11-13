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
#include "hal_gpio.h"
#include "plat_gpio.h"
#include "plat_cpld.h"
#include "plat_hook.h"
#include "plat_ioexp.h"
#include "plat_user_setting.h"
#include "plat_bootstrap.h"

LOG_MODULE_REGISTER(plat_bootstrap);

/* OWL boot source straps: PCA6416A U641 (bus1, 0x20), P0 = OWL_E bit0~7, P1 = OWL_W bit0~7 */
#define BOOTSTRAP_PCA6416A PCA6416A_U641

/* Default values follow the "When Sampled" column of the Astrid chip functional strap table */
// clang-format off
static bootstrap_mapping_register bootstrap_table[STRAP_INDEX_MAX] = {
	{ STRAP_INDEX_HAMSA_TEST_STRAP_R, STRAP_TYPE_CPLD, HAMSA_STRAP, "HAMSA_TEST_STRAP_R", 4, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_HAMSA_LS_STRAP0, STRAP_TYPE_CPLD, HAMSA_STRAP, "HAMSA_LS_STRAP0", 3, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_HAMSA_LS_STRAP1, STRAP_TYPE_CPLD, HAMSA_STRAP, "HAMSA_LS_STRAP1", 2, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_HAMSA_CRM_STRAP0, STRAP_TYPE_CPLD, HAMSA_STRAP, "HAMSA_CRM_STRAP0", 1, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_HAMSA_CRM_STRAP1, STRAP_TYPE_CPLD, HAMSA_STRAP, "HAMSA_CRM_STRAP1", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_HAMSA_MFIO7, STRAP_TYPE_CPLD, HAMSA_MFIO_REG, "HAMSA_MFIO7", 4, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_HAMSA_MFIO9, STRAP_TYPE_CPLD, HAMSA_MFIO_REG, "HAMSA_MFIO9", 3, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_HAMSA_MFIO11, STRAP_TYPE_CPLD, HAMSA_MFIO_REG, "HAMSA_MFIO11", 2, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_HAMSA_MFIO17, STRAP_TYPE_CPLD, HAMSA_MFIO_REG, "HAMSA_MFIO17", 1, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_HAMSA_MFIO18, STRAP_TYPE_CPLD, HAMSA_MFIO_REG, "HAMSA_MFIO18", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_HAMSA_CORE_TAP_CTRL_L, STRAP_TYPE_CPLD, HAMSA_CONTROL_IO, "HAMSA_CORE_TAP_CTRL_L", 3, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_HAMSA_TRI_L, STRAP_TYPE_CPLD, HAMSA_CONTROL_IO, "HAMSA_TRI_L", 2, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_HAMSA_ATPG_MODE_L, STRAP_TYPE_CPLD, HAMSA_CONTROL_IO, "HAMSA_ATPG_MODE_L", 1, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_HAMSA_DFT_TAP_EN_L, STRAP_TYPE_CPLD, HAMSA_CONTROL_IO, "HAMSA_DFT_TAP_EN_L", 0, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_HAMSA_VQPS_EFUSE_USER_EN, STRAP_TYPE_CPLD, VQPS_CTRL_1, "HAMSA_VQPS_EFUSE_USER_EN", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_FM_JTAG_HAMSA_JTCE_0_3, STRAP_TYPE_CPLD, HAMSA_JTAG_JTCE, "FM_JTAG_HAMSA_JTCE_0_3", 0, 4, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA00_TEST_STRAP, STRAP_TYPE_CPLD, ZORA0X_STRAP, "ZORA00_TEST_STRAP", 5, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA00_CRM_STRAP0, STRAP_TYPE_CPLD, ZORA0X_STRAP, "ZORA00_CRM_STRAP0", 4, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA00_CRM_STRAP1, STRAP_TYPE_CPLD, ZORA0X_STRAP, "ZORA00_CRM_STRAP1", 3, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA00_CORE_TAP_CTRL_L, STRAP_TYPE_CPLD, ZORA0X_CONTROL_IO, "ZORA00_CORE_TAP_CTRL_L", 7, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA00_TRI_L, STRAP_TYPE_CPLD, ZORA0X_CONTROL_IO, "ZORA00_TRI_L", 6, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA00_ATPG_MODE_L, STRAP_TYPE_CPLD, ZORA0X_CONTROL_IO, "ZORA00_ATPG_MODE_L", 5, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA00_DFT_TAP_EN_L, STRAP_TYPE_CPLD, ZORA0X_CONTROL_IO, "ZORA00_DFT_TAP_EN_L", 4, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA00_CHIP_STRAP0, STRAP_TYPE_GPIO, ZORA00_CHIP_STRAP0_MMC, "ZORA00_CHIP_STRAP0", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA00_CHIP_STRAP1, STRAP_TYPE_GPIO, ZORA00_CHIP_STRAP1_MMC, "ZORA00_CHIP_STRAP1", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA00_STANDALONE_MODE, STRAP_TYPE_SGPIO, ZORA00_STANDALONE_MODE_R, "ZORA00_STANDALONE_MODE", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA00_VQPS_TOP, STRAP_TYPE_CPLD, VQPS_CTRL_1, "ZORA00_VQPS_TOP", 5, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA00_VQPS_U, STRAP_TYPE_CPLD, VQPS_CTRL_1, "ZORA00_VQPS_U", 1, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_FM_JTAG_ZORA00_JTCE_0_2, STRAP_TYPE_CPLD, ZORA0X_JTAG_JTCE, "FM_JTAG_ZORA00_JTCE_0_2", 3, 3, 0x01, 0x01, true },
	{ STRAP_INDEX_ZORA01_TEST_STRAP, STRAP_TYPE_CPLD, ZORA0X_STRAP, "ZORA01_TEST_STRAP", 2, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA01_CRM_STRAP0, STRAP_TYPE_CPLD, ZORA0X_STRAP, "ZORA01_CRM_STRAP0", 1, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA01_CRM_STRAP1, STRAP_TYPE_CPLD, ZORA0X_STRAP, "ZORA01_CRM_STRAP1", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA01_CORE_TAP_CTRL_PLD_L, STRAP_TYPE_CPLD, ZORA0X_CONTROL_IO, "ZORA01_CORE_TAP_CTRL_PLD_L", 3, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA01_TRI_L, STRAP_TYPE_CPLD, ZORA0X_CONTROL_IO, "ZORA01_TRI_L", 2, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA01_ATPG_MODE_L, STRAP_TYPE_CPLD, ZORA0X_CONTROL_IO, "ZORA01_ATPG_MODE_L", 1, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA01_DFT_TAP_EN_L, STRAP_TYPE_CPLD, ZORA0X_CONTROL_IO, "ZORA01_DFT_TAP_EN_L", 0, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA01_CHIP_STRAP0, STRAP_TYPE_GPIO, ZORA01_CHIP_STRAP0_MMC, "ZORA01_CHIP_STRAP0", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA01_CHIP_STRAP1, STRAP_TYPE_GPIO, ZORA01_CHIP_STRAP1_MMC, "ZORA01_CHIP_STRAP1", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA01_STANDALONE_MODE, STRAP_TYPE_SGPIO, ZORA01_STANDALONE_MODE_R, "ZORA01_STANDALONE_MODE", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA01_VQPS_TOP, STRAP_TYPE_CPLD, VQPS_CTRL_1, "ZORA01_VQPS_TOP", 6, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA01_VQPS_U, STRAP_TYPE_CPLD, VQPS_CTRL_1, "ZORA01_VQPS_U", 2, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_FM_JTAG_ZORA01_JTCE_0_2, STRAP_TYPE_CPLD, ZORA0X_JTAG_JTCE, "FM_JTAG_ZORA01_JTCE_0_2", 0, 3, 0x01, 0x01, true },
	{ STRAP_INDEX_ZORA10_TEST_STRAP, STRAP_TYPE_CPLD, ZORA1X_STRAP, "ZORA10_TEST_STRAP", 5, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA10_CRM_STRAP0, STRAP_TYPE_CPLD, ZORA1X_STRAP, "ZORA10_CRM_STRAP0", 4, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA10_CRM_STRAP1, STRAP_TYPE_CPLD, ZORA1X_STRAP, "ZORA10_CRM_STRAP1", 3, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA10_CORE_TAP_CTRL_PLD_L, STRAP_TYPE_CPLD, ZORA1X_CONTROL_IO, "ZORA10_CORE_TAP_CTRL_PLD_L", 7, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA10_TRI_L, STRAP_TYPE_CPLD, ZORA1X_CONTROL_IO, "ZORA10_TRI_L", 6, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA10_ATPG_MODE_L, STRAP_TYPE_CPLD, ZORA1X_CONTROL_IO, "ZORA10_ATPG_MODE_L", 5, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA10_DFT_TAP_EN_L, STRAP_TYPE_CPLD, ZORA1X_CONTROL_IO, "ZORA10_DFT_TAP_EN_L", 4, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA10_CHIP_STRAP0, STRAP_TYPE_GPIO, ZORA10_CHIP_STRAP0_MMC, "ZORA10_CHIP_STRAP0", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA10_CHIP_STRAP1, STRAP_TYPE_GPIO, ZORA10_CHIP_STRAP1_MMC, "ZORA10_CHIP_STRAP1", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA10_STANDALONE_MODE, STRAP_TYPE_SGPIO, ZORA10_STANDALONE_MODE_R, "ZORA10_STANDALONE_MODE", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA10_VQPS_TOP, STRAP_TYPE_CPLD, VQPS_CTRL_1, "ZORA10_VQPS_TOP", 7, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA10_VQPS_U, STRAP_TYPE_CPLD, VQPS_CTRL_1, "ZORA10_VQPS_U", 3, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_FM_JTAG_ZORA10_JTCE_0_2, STRAP_TYPE_CPLD, ZORA1X_JTAG_JTCE, "FM_JTAG_ZORA10_JTCE_0_2", 3, 3, 0x01, 0x01, true },
	{ STRAP_INDEX_ZORA11_TEST_STRAP, STRAP_TYPE_CPLD, ZORA1X_STRAP, "ZORA11_TEST_STRAP", 2, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA11_CRM_STRAP0, STRAP_TYPE_CPLD, ZORA1X_STRAP, "ZORA11_CRM_STRAP0", 1, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA11_CRM_STRAP1, STRAP_TYPE_CPLD, ZORA1X_STRAP, "ZORA11_CRM_STRAP1", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA11_CORE_TAP_CTRL_PLD_L, STRAP_TYPE_CPLD, ZORA1X_CONTROL_IO, "ZORA11_CORE_TAP_CTRL_PLD_L", 3, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA11_TRI_L, STRAP_TYPE_CPLD, ZORA1X_CONTROL_IO, "ZORA11_TRI_L", 2, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA11_ATPG_MODE_L, STRAP_TYPE_CPLD, ZORA1X_CONTROL_IO, "ZORA11_ATPG_MODE_L", 1, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA11_DFT_TAP_EN_L, STRAP_TYPE_CPLD, ZORA1X_CONTROL_IO, "ZORA11_DFT_TAP_EN_L", 0, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_ZORA11_CHIP_STRAP0, STRAP_TYPE_GPIO, ZORA11_CHIP_STRAP0_MMC, "ZORA11_CHIP_STRAP0", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA11_CHIP_STRAP1, STRAP_TYPE_GPIO, ZORA11_CHIP_STRAP1_MMC, "ZORA11_CHIP_STRAP1", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA11_STANDALONE_MODE, STRAP_TYPE_SGPIO, ZORA11_STANDALONE_MODE_R, "ZORA11_STANDALONE_MODE", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA11_VQPS_TOP, STRAP_TYPE_CPLD, VQPS_CTRL_0, "ZORA11_VQPS_TOP", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_ZORA11_VQPS_U, STRAP_TYPE_CPLD, VQPS_CTRL_1, "ZORA11_VQPS_U", 4, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_FM_JTAG_ZORA11_JTCE_0_2, STRAP_TYPE_CPLD, ZORA1X_JTAG_JTCE, "FM_JTAG_ZORA11_JTCE_0_2", 0, 3, 0x01, 0x01, true },
	{ STRAP_INDEX_PLD_OWL_E_DFT_TAP_EN_L, STRAP_TYPE_CPLD, OWL_CONTROL_IO, "PLD_OWL_E_DFT_TAP_EN_L", 7, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_PLD_OWL_E_CORE_TAP_CTRL_L, STRAP_TYPE_CPLD, OWL_CONTROL_IO, "PLD_OWL_E_CORE_TAP_CTRL_L", 6, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_PLD_OWL_E_PAD_TRI_L, STRAP_TYPE_CPLD, OWL_CONTROL_IO, "PLD_OWL_E_PAD_TRI_L", 5, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_PLD_OWL_E_ATPG_MODE_L, STRAP_TYPE_CPLD, OWL_CONTROL_IO, "PLD_OWL_E_ATPG_MODE_L", 4, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_OWL_E_JTAG_MUX_PLD_SEL_00, STRAP_TYPE_GPIO, FM_OWL_E_JTAG_MUX_SEL_00, "OWL_E_JTAG_MUX_PLD_SEL_00", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_E_JTAG_MUX_PLD_SEL_01, STRAP_TYPE_GPIO, FM_OWL_E_JTAG_MUX_SEL_01, "OWL_E_JTAG_MUX_PLD_SEL_01", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_E_JTAG_MUX_PLD_SEL_02, STRAP_TYPE_GPIO, FM_OWL_E_JTAG_MUX_SEL_02, "OWL_E_JTAG_MUX_PLD_SEL_02", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_E_JTAG_MUX_PLD_SEL_03, STRAP_TYPE_GPIO, FM_OWL_E_JTAG_MUX_SEL_03, "OWL_E_JTAG_MUX_PLD_SEL_03", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_E_UART_MUX_PLD_SEL_00, STRAP_TYPE_GPIO, FM_OWL_E_UART_MUX_SEL_00, "OWL_E_UART_MUX_PLD_SEL_00", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_E_UART_MUX_PLD_SEL_01, STRAP_TYPE_GPIO, FM_OWL_E_UART_MUX_SEL_01, "OWL_E_UART_MUX_PLD_SEL_01", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_E_UART_MUX_PLD_SEL_02, STRAP_TYPE_GPIO, FM_OWL_E_UART_MUX_SEL_02, "OWL_E_UART_MUX_PLD_SEL_02", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_E_BOOT_SOURCE_0_7, STRAP_TYPE_IOEXP_PCA6416A, PCA6416A_OUTPUT_PORT_0, "OWL_E_BOOT_SOURCE_0_7", 0, 8, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_E_DVT_ENABLE, STRAP_TYPE_CPLD, OWL_DVT_ENABLE, "OWL_E_DVT_ENABLE", 1, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_PLD_OWL_W_DFT_TAP_EN_L, STRAP_TYPE_CPLD, OWL_CONTROL_IO, "PLD_OWL_W_DFT_TAP_EN_L", 3, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_PLD_OWL_W_CORE_TAP_CTRL_L, STRAP_TYPE_CPLD, OWL_CONTROL_IO, "PLD_OWL_W_CORE_TAP_CTRL_L", 2, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_PLD_OWL_W_PAD_TRI_L, STRAP_TYPE_CPLD, OWL_CONTROL_IO, "PLD_OWL_W_PAD_TRI_L", 1, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_PLD_OWL_W_ATPG_MODE_L, STRAP_TYPE_CPLD, OWL_CONTROL_IO, "PLD_OWL_W_ATPG_MODE_L", 0, 1, 0x01, 0x01, false },
	{ STRAP_INDEX_OWL_W_JTAG_MUX_PLD_SEL_00, STRAP_TYPE_GPIO, FM_OWL_W_JTAG_MUX_SEL_00, "OWL_W_JTAG_MUX_PLD_SEL_00", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_W_JTAG_MUX_PLD_SEL_01, STRAP_TYPE_GPIO, FM_OWL_W_JTAG_MUX_SEL_01, "OWL_W_JTAG_MUX_PLD_SEL_01", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_W_JTAG_MUX_PLD_SEL_02, STRAP_TYPE_GPIO, FM_OWL_W_JTAG_MUX_SEL_02, "OWL_W_JTAG_MUX_PLD_SEL_02", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_W_JTAG_MUX_PLD_SEL_03, STRAP_TYPE_GPIO, FM_OWL_W_JTAG_MUX_SEL_03, "OWL_W_JTAG_MUX_PLD_SEL_03", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_W_UART_MUX_PLD_SEL_00, STRAP_TYPE_GPIO, FM_OWL_W_UART_MUX_SEL_00, "OWL_W_UART_MUX_PLD_SEL_00", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_W_UART_MUX_PLD_SEL_01, STRAP_TYPE_GPIO, FM_OWL_W_UART_MUX_SEL_01, "OWL_W_UART_MUX_PLD_SEL_01", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_W_UART_MUX_PLD_SEL_02, STRAP_TYPE_GPIO, FM_OWL_W_UART_MUX_SEL_02, "OWL_W_UART_MUX_PLD_SEL_02", 0, 1, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_W_BOOT_SOURCE_0_7, STRAP_TYPE_IOEXP_PCA6416A, PCA6416A_OUTPUT_PORT_1, "OWL_W_BOOT_SOURCE_0_7", 0, 8, 0x00, 0x00, false },
	{ STRAP_INDEX_OWL_W_DVT_ENABLE, STRAP_TYPE_CPLD, OWL_DVT_ENABLE, "OWL_W_DVT_ENABLE", 0, 1, 0x00, 0x00, false },
};
// clang-format on

/* perm values stored in EEPROM */
static bootstrap_perm_t bootstrap_perm;

uint8_t get_strap_index_max(void)
{
	return STRAP_INDEX_MAX;
}

static uint8_t reverse_bits(uint8_t byte, uint8_t bit_cnt)
{
	uint8_t reversed_byte = 0;

	for (int i = 0; i < bit_cnt; i++) {
		reversed_byte = (reversed_byte << 1) | (byte & 1);
		byte >>= 1;
	}

	return reversed_byte;
}

/* Mask of all table bits sharing the same register as the given strap */
static uint8_t get_reg_mask(uint8_t strap)
{
	uint8_t mask = 0;

	for (int i = 0; i < STRAP_INDEX_MAX; i++) {
		if ((bootstrap_table[i].type != bootstrap_table[strap].type) ||
		    (bootstrap_table[i].cpld_offsets != bootstrap_table[strap].cpld_offsets))
			continue;

		mask |= GENMASK(bootstrap_table[i].bit_offset + bootstrap_table[i].bit_count - 1,
				bootstrap_table[i].bit_offset);
	}

	return mask;
}

bool set_ioexp_val_to_bootstrap_table(void)
{
	uint8_t data[2] = { 0 };
	uint8_t direction[2] = { 0 };

	if (!is_mb_dc_on())
		return true;

	if (!pca6416a_i2c_read(BOOTSTRAP_PCA6416A, PCA6416A_OUTPUT_PORT_0, data, 2)) {
		LOG_ERR("Can't find bootstrap value from pca6416a");
		return false;
	}
	if (!pca6416a_i2c_read(BOOTSTRAP_PCA6416A, PCA6416A_CONFIG_0, direction, 2)) {
		LOG_ERR("Can't find bootstrap direction from pca6416a");
		return false;
	}

	// update output pins only, keep the table value of input pins
	uint8_t old_e = bootstrap_table[STRAP_INDEX_OWL_E_BOOT_SOURCE_0_7].change_setting_value;
	uint8_t old_w = bootstrap_table[STRAP_INDEX_OWL_W_BOOT_SOURCE_0_7].change_setting_value;

	bootstrap_table[STRAP_INDEX_OWL_E_BOOT_SOURCE_0_7].change_setting_value =
		(old_e & direction[0]) | (data[0] & (~direction[0]));
	bootstrap_table[STRAP_INDEX_OWL_W_BOOT_SOURCE_0_7].change_setting_value =
		(old_w & direction[1]) | (data[1] & (~direction[1]));

	return true;
}

bool set_bootstrap_table_val_to_ioexp(void)
{
	uint8_t data[2] = {
		bootstrap_table[STRAP_INDEX_OWL_E_BOOT_SOURCE_0_7].change_setting_value,
		bootstrap_table[STRAP_INDEX_OWL_W_BOOT_SOURCE_0_7].change_setting_value,
	};

	if (!pca6416a_i2c_write(BOOTSTRAP_PCA6416A, PCA6416A_OUTPUT_PORT_0, data, 2)) {
		LOG_ERR("Can't set pca6416a from bootstrap_table");
		return false;
	}

	return true;
}

bool bootstrap_default_settings_init(void)
{
	for (int i = 0; i < STRAP_INDEX_MAX; i++) {
		uint8_t data = 0;

		switch (bootstrap_table[i].type) {
		case STRAP_TYPE_CPLD: {
			if (!plat_read_cpld(bootstrap_table[i].cpld_offsets, &data, 1)) {
				LOG_ERR("Can't find bootstrap default from CPLD, index: %d", i);
				return false;
			}

			uint8_t mask = GENMASK(bootstrap_table[i].bit_offset +
						       bootstrap_table[i].bit_count - 1,
					       bootstrap_table[i].bit_offset);

			bootstrap_table[i].change_setting_value =
				(data & mask) >> bootstrap_table[i].bit_offset;
			break;
		}
		case STRAP_TYPE_GPIO:
			bootstrap_table[i].change_setting_value =
				gpio_get(bootstrap_table[i].cpld_offsets);
			break;
		default:
			continue;
		}

		if (bootstrap_table[i].reverse) {
			bootstrap_table[i].change_setting_value =
				reverse_bits(bootstrap_table[i].change_setting_value,
					     bootstrap_table[i].bit_count);
		}
	}

	return set_ioexp_val_to_bootstrap_table();
}

/*
 * SGPIO is initialized after the bootstrap table, so SGPIO straps are read on demand.
 * Keep the table value when the read fails (SGPIO not ready).
 */
static void bootstrap_sgpio_refresh(uint8_t rail)
{
	if (bootstrap_table[rail].type != STRAP_TYPE_SGPIO)
		return;

	int val = sgpio_get(bootstrap_table[rail].cpld_offsets);
	if (val >= 0)
		bootstrap_table[rail].change_setting_value = val;
}

static bool check_is_bootstrap_setting_value_valid(uint8_t rail, uint8_t value)
{
	int critical_value = 1 << bootstrap_table[rail].bit_count;

	return value < critical_value;
}

bool find_bootstrap_by_rail(uint8_t rail, bootstrap_mapping_register *result)
{
	CHECK_NULL_ARG_WITH_RETURN(result, false);

	if (rail >= STRAP_INDEX_MAX)
		return false;

	bootstrap_sgpio_refresh(rail);
	*result = bootstrap_table[rail];
	return true;
}

bool get_bootstrap_change_drive_level(int rail, int *drive_level)
{
	CHECK_NULL_ARG_WITH_RETURN(drive_level, false);

	if ((rail < 0) || (rail >= STRAP_INDEX_MAX)) {
		LOG_ERR("Can't find strap_item by rail index: %d", rail);
		return false;
	}

	bootstrap_sgpio_refresh(rail);
	*drive_level = bootstrap_table[rail].change_setting_value;
	return true;
}

bool strap_name_get(uint8_t rail, uint8_t **name)
{
	CHECK_NULL_ARG_WITH_RETURN(name, false);

	if (rail >= STRAP_INDEX_MAX) {
		*name = NULL;
		return false;
	}

	*name = (uint8_t *)bootstrap_table[rail].strap_name;
	return true;
}

bool strap_enum_get(const char *name, uint8_t *num)
{
	CHECK_NULL_ARG_WITH_RETURN(name, false);
	CHECK_NULL_ARG_WITH_RETURN(num, false);

	for (int i = 0; i < STRAP_INDEX_MAX; i++) {
		if (strcmp(name, bootstrap_table[i].strap_name) == 0) {
			*num = i;
			return true;
		}
	}

	LOG_ERR("invalid rail name %s", name);
	return false;
}

bool bootstrap_perm_get(uint8_t rail, uint8_t *value)
{
	CHECK_NULL_ARG_WITH_RETURN(value, false);

	if ((rail >= STRAP_INDEX_MAX) || (bootstrap_perm.unset[rail / 8] & BIT(rail % 8)))
		return false;

	*value = bootstrap_perm.value[rail];
	return true;
}

static void bootstrap_perm_load(void)
{
	if (plat_perm_get(PERM_ID_BOOTSTRAP, &bootstrap_perm, BOOTSTRAP_PERM_SIZE))
		memset(&bootstrap_perm, 0xFF, sizeof(bootstrap_perm));
}

bool set_bootstrap_table_and_user_settings(uint8_t rail, uint8_t *change_setting_value,
					   uint8_t drive_index_level, bool is_perm, bool is_default)
{
	CHECK_NULL_ARG_WITH_RETURN(change_setting_value, false);

	if (rail >= STRAP_INDEX_MAX)
		return false;

	bootstrap_mapping_register *item = &bootstrap_table[rail];

	if (is_default)
		drive_index_level = item->default_setting_value;

	if (!check_is_bootstrap_setting_value_valid(rail, drive_index_level)) {
		LOG_ERR("hex-value :0x%x is out of range", drive_index_level);
		return false;
	}

	item->change_setting_value = drive_index_level;
	*change_setting_value = 0;

	if ((item->type == STRAP_TYPE_GPIO) || (item->type == STRAP_TYPE_SGPIO)) {
		*change_setting_value = item->change_setting_value;
	} else {
		// merge every strap sharing the same register into one byte
		for (int j = 0; j < STRAP_INDEX_MAX; j++) {
			if ((bootstrap_table[j].cpld_offsets != item->cpld_offsets) ||
			    (bootstrap_table[j].type != item->type))
				continue;

			uint8_t value = bootstrap_table[j].change_setting_value;

			if (bootstrap_table[j].reverse)
				value = reverse_bits(value, bootstrap_table[j].bit_count);

			*change_setting_value |=
				(value & GENMASK(bootstrap_table[j].bit_count - 1, 0))
				<< bootstrap_table[j].bit_offset;
		}
	}

	if (is_perm) {
		bootstrap_perm.value[rail] = item->change_setting_value;
		bootstrap_perm.unset[rail / 8] &= ~BIT(rail % 8);
		if (!plat_perm_set(PERM_ID_BOOTSTRAP, &bootstrap_perm, BOOTSTRAP_PERM_SIZE)) {
			LOG_ERR("bootstrap failed to write perm");
			return false;
		}
	}

	return true;
}

bool set_bootstrap_val_to_device(uint8_t strap, uint8_t val)
{
	if (strap >= STRAP_INDEX_MAX)
		return false;

	uint8_t type = bootstrap_table[strap].type;

	switch (type) {
	case STRAP_TYPE_CPLD:
		// only touch the bits owned by the bootstrap table
		if (!plat_update_cpld(bootstrap_table[strap].cpld_offsets, get_reg_mask(strap),
				      val))
			return false;
		break;
	case STRAP_TYPE_GPIO:
		if (gpio_set(bootstrap_table[strap].cpld_offsets, val) < 0)
			return false;
		break;
	case STRAP_TYPE_SGPIO:
		// only SGPIO outputs (BIC --> CPLD) can be driven
		if (sgpio_cfg[bootstrap_table[strap].cpld_offsets].direction != GPIO_OUTPUT) {
			LOG_ERR("%s is a SGPIO input, can't be set",
				bootstrap_table[strap].strap_name);
			return false;
		}
		if (sgpio_set(bootstrap_table[strap].cpld_offsets, val) < 0)
			return false;
		break;
	case STRAP_TYPE_IOEXP_PCA6416A:
		if (is_mb_dc_on()) {
			if (!pca6416a_i2c_write(BOOTSTRAP_PCA6416A,
						bootstrap_table[strap].cpld_offsets, &val, 1))
				return false;
		}
		break;
	default:
		LOG_ERR("Invalid bootstrap_table[%d] type: %d", strap, type);
		return false;
	}

	return true;
}

bool bootstrap_user_settings_init(void)
{
	bootstrap_perm_load();

	for (int i = 0; i < STRAP_INDEX_MAX; i++) {
		uint8_t drive_index_level = 0;
		uint8_t change_setting_value = 0;

		if (!bootstrap_perm_get(i, &drive_index_level))
			continue;

		if (!set_bootstrap_table_and_user_settings(i, &change_setting_value,
							   drive_index_level, false, false)) {
			LOG_ERR("set bootstrap_table[%2d]:%d failed", i, drive_index_level);
			continue;
		}

		if (!set_bootstrap_val_to_device(i, change_setting_value)) {
			LOG_ERR("Can't set bootstrap[%2d]=%02x by user settings", i,
				change_setting_value);
			continue;
		}

		LOG_INF("set [%2d]%s: %02x", i, bootstrap_table[i].strap_name,
			change_setting_value);
	}

	return true;
}
