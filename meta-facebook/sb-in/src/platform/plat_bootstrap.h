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

#ifndef PLAT_BOOTSTRAP_H
#define PLAT_BOOTSTRAP_H

#include <stdbool.h>
#include <stdint.h>

enum STRAP_TYPE_E {
	STRAP_TYPE_CPLD = 0,
	STRAP_TYPE_GPIO,
	STRAP_TYPE_IOEXP_PCA6416A,
	STRAP_TYPE_SGPIO,
	STRAP_TYPE_MAX,
};

enum PLAT_STRAP_INDEX_E {
	STRAP_INDEX_HAMSA_TEST_STRAP_R = 0,
	STRAP_INDEX_HAMSA_LS_STRAP0,
	STRAP_INDEX_HAMSA_LS_STRAP1,
	STRAP_INDEX_HAMSA_CRM_STRAP0,
	STRAP_INDEX_HAMSA_CRM_STRAP1,
	STRAP_INDEX_HAMSA_MFIO7,
	STRAP_INDEX_HAMSA_MFIO9,
	STRAP_INDEX_HAMSA_MFIO11,
	STRAP_INDEX_HAMSA_MFIO17,
	STRAP_INDEX_HAMSA_MFIO18,
	STRAP_INDEX_HAMSA_CORE_TAP_CTRL_L,
	STRAP_INDEX_HAMSA_TRI_L,
	STRAP_INDEX_HAMSA_ATPG_MODE_L,
	STRAP_INDEX_HAMSA_DFT_TAP_EN_L,
	STRAP_INDEX_HAMSA_VQPS_EFUSE_USER_EN,
	STRAP_INDEX_FM_JTAG_HAMSA_JTCE_0_3,
	STRAP_INDEX_ZORA00_TEST_STRAP,
	STRAP_INDEX_ZORA00_CRM_STRAP0,
	STRAP_INDEX_ZORA00_CRM_STRAP1,
	STRAP_INDEX_ZORA00_CORE_TAP_CTRL_L,
	STRAP_INDEX_ZORA00_TRI_L,
	STRAP_INDEX_ZORA00_ATPG_MODE_L,
	STRAP_INDEX_ZORA00_DFT_TAP_EN_L,
	STRAP_INDEX_ZORA00_CHIP_STRAP0,
	STRAP_INDEX_ZORA00_CHIP_STRAP1,
	STRAP_INDEX_ZORA00_STANDALONE_MODE,
	STRAP_INDEX_ZORA00_VQPS_TOP,
	STRAP_INDEX_ZORA00_VQPS_U,
	STRAP_INDEX_FM_JTAG_ZORA00_JTCE_0_2,
	STRAP_INDEX_ZORA01_TEST_STRAP,
	STRAP_INDEX_ZORA01_CRM_STRAP0,
	STRAP_INDEX_ZORA01_CRM_STRAP1,
	STRAP_INDEX_ZORA01_CORE_TAP_CTRL_PLD_L,
	STRAP_INDEX_ZORA01_TRI_L,
	STRAP_INDEX_ZORA01_ATPG_MODE_L,
	STRAP_INDEX_ZORA01_DFT_TAP_EN_L,
	STRAP_INDEX_ZORA01_CHIP_STRAP0,
	STRAP_INDEX_ZORA01_CHIP_STRAP1,
	STRAP_INDEX_ZORA01_STANDALONE_MODE,
	STRAP_INDEX_ZORA01_VQPS_TOP,
	STRAP_INDEX_ZORA01_VQPS_U,
	STRAP_INDEX_FM_JTAG_ZORA01_JTCE_0_2,
	STRAP_INDEX_ZORA10_TEST_STRAP,
	STRAP_INDEX_ZORA10_CRM_STRAP0,
	STRAP_INDEX_ZORA10_CRM_STRAP1,
	STRAP_INDEX_ZORA10_CORE_TAP_CTRL_PLD_L,
	STRAP_INDEX_ZORA10_TRI_L,
	STRAP_INDEX_ZORA10_ATPG_MODE_L,
	STRAP_INDEX_ZORA10_DFT_TAP_EN_L,
	STRAP_INDEX_ZORA10_CHIP_STRAP0,
	STRAP_INDEX_ZORA10_CHIP_STRAP1,
	STRAP_INDEX_ZORA10_STANDALONE_MODE,
	STRAP_INDEX_ZORA10_VQPS_TOP,
	STRAP_INDEX_ZORA10_VQPS_U,
	STRAP_INDEX_FM_JTAG_ZORA10_JTCE_0_2,
	STRAP_INDEX_ZORA11_TEST_STRAP,
	STRAP_INDEX_ZORA11_CRM_STRAP0,
	STRAP_INDEX_ZORA11_CRM_STRAP1,
	STRAP_INDEX_ZORA11_CORE_TAP_CTRL_PLD_L,
	STRAP_INDEX_ZORA11_TRI_L,
	STRAP_INDEX_ZORA11_ATPG_MODE_L,
	STRAP_INDEX_ZORA11_DFT_TAP_EN_L,
	STRAP_INDEX_ZORA11_CHIP_STRAP0,
	STRAP_INDEX_ZORA11_CHIP_STRAP1,
	STRAP_INDEX_ZORA11_STANDALONE_MODE,
	STRAP_INDEX_ZORA11_VQPS_TOP,
	STRAP_INDEX_ZORA11_VQPS_U,
	STRAP_INDEX_FM_JTAG_ZORA11_JTCE_0_2,
	STRAP_INDEX_PLD_OWL_E_DFT_TAP_EN_L,
	STRAP_INDEX_PLD_OWL_E_CORE_TAP_CTRL_L,
	STRAP_INDEX_PLD_OWL_E_PAD_TRI_L,
	STRAP_INDEX_PLD_OWL_E_ATPG_MODE_L,
	STRAP_INDEX_OWL_E_JTAG_MUX_PLD_SEL_00,
	STRAP_INDEX_OWL_E_JTAG_MUX_PLD_SEL_01,
	STRAP_INDEX_OWL_E_JTAG_MUX_PLD_SEL_02,
	STRAP_INDEX_OWL_E_JTAG_MUX_PLD_SEL_03,
	STRAP_INDEX_OWL_E_UART_MUX_PLD_SEL_00,
	STRAP_INDEX_OWL_E_UART_MUX_PLD_SEL_01,
	STRAP_INDEX_OWL_E_UART_MUX_PLD_SEL_02,
	STRAP_INDEX_OWL_E_BOOT_SOURCE_0_7,
	STRAP_INDEX_OWL_E_DVT_ENABLE,
	STRAP_INDEX_PLD_OWL_W_DFT_TAP_EN_L,
	STRAP_INDEX_PLD_OWL_W_CORE_TAP_CTRL_L,
	STRAP_INDEX_PLD_OWL_W_PAD_TRI_L,
	STRAP_INDEX_PLD_OWL_W_ATPG_MODE_L,
	STRAP_INDEX_OWL_W_JTAG_MUX_PLD_SEL_00,
	STRAP_INDEX_OWL_W_JTAG_MUX_PLD_SEL_01,
	STRAP_INDEX_OWL_W_JTAG_MUX_PLD_SEL_02,
	STRAP_INDEX_OWL_W_JTAG_MUX_PLD_SEL_03,
	STRAP_INDEX_OWL_W_UART_MUX_PLD_SEL_00,
	STRAP_INDEX_OWL_W_UART_MUX_PLD_SEL_01,
	STRAP_INDEX_OWL_W_UART_MUX_PLD_SEL_02,
	STRAP_INDEX_OWL_W_BOOT_SOURCE_0_7,
	STRAP_INDEX_OWL_W_DVT_ENABLE,
	STRAP_INDEX_MAX,
};

typedef struct bootstrap_mapping_register {
	uint8_t index;
	uint8_t type;
	uint8_t cpld_offsets; /* CPLD offset, GPIO/SGPIO number or IO expander register */
	const char *strap_name;
	uint8_t bit_offset;
	uint8_t bit_count;
	uint8_t default_setting_value;
	uint8_t change_setting_value;
	bool reverse;
} bootstrap_mapping_register;

/*
 * Perm storage of all straps, written in one EEPROM write.
 * unset bit = 1 means the strap has no perm value, erased EEPROM (all 0xFF) = nothing set.
 */
typedef struct __attribute__((__packed__)) {
	uint8_t unset[(STRAP_INDEX_MAX + 7) / 8];
	uint8_t value[STRAP_INDEX_MAX];
} bootstrap_perm_t;

#define BOOTSTRAP_PERM_SIZE sizeof(bootstrap_perm_t)

uint8_t get_strap_index_max(void);
bool strap_name_get(uint8_t rail, uint8_t **name);
bool strap_enum_get(const char *name, uint8_t *num);
bool find_bootstrap_by_rail(uint8_t rail, bootstrap_mapping_register *result);
bool get_bootstrap_change_drive_level(int rail, int *drive_level);
bool set_bootstrap_table_and_user_settings(uint8_t rail, uint8_t *change_setting_value,
					   uint8_t drive_index_level, bool is_perm,
					   bool is_default);
bool set_bootstrap_val_to_device(uint8_t strap, uint8_t val);
bool set_ioexp_val_to_bootstrap_table(void);
bool set_bootstrap_table_val_to_ioexp(void);
bool bootstrap_perm_get(uint8_t rail, uint8_t *value);

/* Read the current strap values from CPLD/GPIO/IO expander into the table, called at boot */
bool bootstrap_default_settings_init(void);
/* Apply the perm values stored in EEPROM, called at boot */
bool bootstrap_user_settings_init(void);

#endif
