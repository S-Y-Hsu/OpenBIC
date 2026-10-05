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

#ifndef PLAT_GPIO_H
#define PLAT_GPIO_H

#include "hal_gpio.h"

void gpio_int_default();
bool plat_sgpio_init(void);

// clang-format off

// gpio_cfg(chip, number, is_init, direction, status, int_type, int_callback)
// dedicate gpio A0~A7, B0~B7, C0~C7, D0~D7, E0~E7, total 40 gpios
// Default name: Reserve_GPIOH0
#define name_gpio0	\
	gpio_name_to_num(FM_ASIC_0_THERMTRIP_R_N) \
	gpio_name_to_num(RST_ASTRID_PWR_ON_PLD_R1_N) \
	gpio_name_to_num(BUFF3_100M_LOSB_MMC) \
	gpio_name_to_num(Reserve_GPIO03) \
	gpio_name_to_num(ZORAVDD_I2C_LV_EN) \
	gpio_name_to_num(ALL_VR_PM_ALERT_R_N) \
	gpio_name_to_num(SMB_HAMSA_MMC_LVC33_ALERT_N) \
	gpio_name_to_num(FM_PLD_UBC_EN_R)
#define name_gpio1	\
	gpio_name_to_num(ZORA01_HBM_CATTRIP_MMC_LVC33_ALARM) \
	gpio_name_to_num(Reserve_GPIO11) \
	gpio_name_to_num(Reserve_GPIO12) \
	gpio_name_to_num(Reserve_GPIO13) \
	gpio_name_to_num(Reserve_GPIO14) \
	gpio_name_to_num(Reserve_GPIO15) \
	gpio_name_to_num(Reserve_GPIO16) \
	gpio_name_to_num(Reserve_GPIO17)
#define name_gpio2	\
	gpio_name_to_num(HAMSA_UART_MUX_SEL_R) \
	gpio_name_to_num(Reserve_GPIO21) \
	gpio_name_to_num(ZORA10_CHIP_STRAP0_MMC) \
	gpio_name_to_num(Reserve_GPIO23) \
	gpio_name_to_num(Reserve_GPIO24) \
	gpio_name_to_num(MMC_U684_OE) \
	gpio_name_to_num(MMC_U626_OE) \
	gpio_name_to_num(Reserve_GPIO27)
#define name_gpio3	\
	gpio_name_to_num(Reserve_GPIO30) \
	gpio_name_to_num(Reserve_GPIO31) \
	gpio_name_to_num(Reserve_GPIO32) \
	gpio_name_to_num(Reserve_GPIO33) \
	gpio_name_to_num(Reserve_GPIO34) \
	gpio_name_to_num(Reserve_GPIO35) \
	gpio_name_to_num(Reserve_GPIO36) \
	gpio_name_to_num(Reserve_GPIO37)
#define name_gpio4	\
	gpio_name_to_num(FM_OWL_W_JTAG_MUX_SEL_00) \
	gpio_name_to_num(FM_OWL_W_JTAG_MUX_SEL_01) \
	gpio_name_to_num(FM_OWL_W_JTAG_MUX_SEL_02) \
	gpio_name_to_num(FM_OWL_W_JTAG_MUX_SEL_03) \
	gpio_name_to_num(Reserve_GPIO44) \
	gpio_name_to_num(Reserve_GPIO45) \
	gpio_name_to_num(Reserve_GPIO46) \
	gpio_name_to_num(Reserve_GPIO47)
#define name_gpio5	\
	gpio_name_to_num(SPI_HAMSA_MUX_IN1) \
	gpio_name_to_num(SMB_ZORA00_CRM_ALERT_N_LS_LVC33_MMC) \
	gpio_name_to_num(SMB_ZORA01_CRM_ALERT_N_LS_LVC33_MMC) \
	gpio_name_to_num(SMB_ZORA10_CRM_ALERT_N_LS_LVC33_MMC) \
	gpio_name_to_num(FM_OWL_W_UART_MUX_SEL_00) \
	gpio_name_to_num(Reserve_GPIO55) \
	gpio_name_to_num(Reserve_GPIO56) \
	gpio_name_to_num(ZORA11_HBM_CATTRIP_MMC_LVC33_ALARM)
#define name_gpio6	\
	gpio_name_to_num(Reserve_GPIO60) \
	gpio_name_to_num(Reserve_GPIO61) \
	gpio_name_to_num(SMB_ZORA11_CRM_ALERT_N_LS_LVC33_MMC) \
	gpio_name_to_num(FM_OWL_E_JTAG_MUX_SEL_00) \
	gpio_name_to_num(FM_OWL_E_JTAG_MUX_SEL_01) \
	gpio_name_to_num(FM_OWL_W_UART_MUX_SEL_01) \
	gpio_name_to_num(Reserve_GPIO66) \
	gpio_name_to_num(Reserve_GPIO67)
#define name_gpio7	\
	gpio_name_to_num(Reserve_GPIO70) \
	gpio_name_to_num(FM_OWL_E_JTAG_MUX_SEL_02) \
	gpio_name_to_num(FM_OWL_E_JTAG_MUX_SEL_03) \
	gpio_name_to_num(Reserve_GPIO73) \
	gpio_name_to_num(Reserve_GPIO74) \
	gpio_name_to_num(Reserve_GPIO75) \
	gpio_name_to_num(Reserve_GPIO76) \
	gpio_name_to_num(Reserve_GPIO77)
#define name_gpio8	\
	gpio_name_to_num(FM_OWL_W_UART_MUX_SEL_02) \
	gpio_name_to_num(NC_FAN_TACH1) \
	gpio_name_to_num(LED_MMC_HEARTBEAT_R) \
	gpio_name_to_num(ZORA10_HBM_CATTRIP_MMC_ALARM) \
	gpio_name_to_num(Reserve_GPIO84) \
	gpio_name_to_num(Reserve_GPIO85) \
	gpio_name_to_num(ZORA01_CHIP_STRAP1_MMC) \
	gpio_name_to_num(Reserve_GPIO87)
#define name_gpio9	\
	gpio_name_to_num(ZORA00_CHIP_STRAP0_MMC) \
	gpio_name_to_num(ZORA00_CHIP_STRAP1_MMC) \
	gpio_name_to_num(ZORA01_CHIP_STRAP0_MMC) \
	gpio_name_to_num(ZORA11_CHIP_STRAP1_MMC) \
	gpio_name_to_num(ZORA11_CHIP_STRAP0_MMC) \
	gpio_name_to_num(ZORA10_CHIP_STRAP1_MMC) \
	gpio_name_to_num(Reserve_GPIO96) \
	gpio_name_to_num(Reserve_GPIO97)
#define name_gpioA	\
	gpio_name_to_num(Reserve_GPIOA0) \
	gpio_name_to_num(Reserve_GPIOA1) \
	gpio_name_to_num(Reserve_GPIOA2) \
	gpio_name_to_num(Reserve_GPIOA3) \
	gpio_name_to_num(Reserve_GPIOA4) \
	gpio_name_to_num(Reserve_GPIOA5) \
	gpio_name_to_num(Reserve_GPIOA6) \
	gpio_name_to_num(Reserve_GPIOA7)
#define name_gpioB	\
	gpio_name_to_num(ZORA00_HBM_CATTRIP_MMC_LVC33_ALARM) \
	gpio_name_to_num(FM_OWL_E_UART_MUX_SEL_02) \
	gpio_name_to_num(FM_OWL_E_UART_MUX_SEL_00) \
	gpio_name_to_num(FM_OWL_E_UART_MUX_SEL_01) \
	gpio_name_to_num(Reserve_GPIOB4) \
	gpio_name_to_num(Reserve_GPIOB5) \
	gpio_name_to_num(FM_PDB_INA238_I2C_BUF_EN) \
	gpio_name_to_num(Reserve_GPIOB7)
#define name_gpioC	\
	gpio_name_to_num(Reserve_GPIOC0) \
	gpio_name_to_num(Reserve_GPIOC1) \
	gpio_name_to_num(Reserve_GPIOC2) \
	gpio_name_to_num(Reserve_GPIOC3) \
	gpio_name_to_num(Reserve_GPIOC4) \
	gpio_name_to_num(Reserve_GPIOC5) \
	gpio_name_to_num(Reserve_GPIOC6) \
	gpio_name_to_num(Reserve_GPIOC7)
#define name_gpioD	\
	gpio_name_to_num(Reserve_GPIOD0) \
	gpio_name_to_num(Reserve_GPIOD1) \
	gpio_name_to_num(Reserve_GPIOD2) \
	gpio_name_to_num(Reserve_GPIOD3) \
	gpio_name_to_num(Reserve_GPIOD4) \
	gpio_name_to_num(Reserve_GPIOD5) \
	gpio_name_to_num(Reserve_GPIOD6) \
	gpio_name_to_num(Reserve_GPIOD7)
#define name_gpioE	\
	gpio_name_to_num(PRSNT_PDB_SENSOR_N_R) \
	gpio_name_to_num(I3C_INGRID_ALERT_N) \
	gpio_name_to_num(Reserve_GPIOE2) \
	gpio_name_to_num(Reserve_GPIOE3) \
	gpio_name_to_num(Reserve_GPIOE4) \
	gpio_name_to_num(EN_MMC_CLKGEN_SW_R) \
	gpio_name_to_num(Reserve_GPIOE6) \
	gpio_name_to_num(Reserve_GPIOE7)
#define name_gpioF	\
	gpio_name_to_num(Reserve_GPIOF0) \
	gpio_name_to_num(Reserve_GPIOF1) \
	gpio_name_to_num(Reserve_GPIOF2) \
	gpio_name_to_num(Reserve_GPIOF3)

// clang-format on

#define gpio_name_to_num(x) x,
enum _GPIO_NUMS_ {
	name_gpio0 name_gpio1 name_gpio2 name_gpio3 name_gpio4 name_gpio5 name_gpio6 name_gpio7
		name_gpio8 name_gpio9 name_gpioA name_gpioB name_gpioC name_gpioD name_gpioE
			name_gpioF
};

extern enum _GPIO_NUMS_ GPIO_NUMS;
#undef gpio_name_to_num

extern char *gpio_name[];

/*
 * SGPIO0/SGPIO1 real pin mapping, from the sb-in "CPLD SGPIO Slave" table.
 *
 * The table's columns are named from the CPLD's point of view: its "SGPIO0
 * Input" (MMC --> CPLD) is data the BIC drives OUT, i.e. this board's
 * SGPIO0_OUT(); its "SGPIO0 Output" (CPLD --> MMC) is data the BIC reads IN,
 * i.e. this board's SGPIO0_IN(). Confirmed against the schematic - do not
 * flip this again without re-checking.
 *
 * plat_sgpio_cfg[] in plat_gpio.c writes each entry's real number as a
 * literal SGPIOx_OUT()/SGPIOx_IN(n) call (mirrors plat_gpio_cfg[] using a
 * literal gpio number instead of a name). The name_sgpio0_out/in and
 * name_sgpio1_out/in lists below are the only place a bit's name lives, and
 * (like name_gpio0~F above) they double as the enum _SGPIO_NUMS_ definition
 * further down, so wZORA00_VQPS_TOP_EN etc. are usable anywhere an int is,
 * e.g. sgpio_set(wZORA00_VQPS_TOP_EN, 1). Both the name lists and
 * plat_sgpio_cfg[] must stay in the same bit order (position N here ==
 * plat_sgpio_cfg[N]) - nothing enforces that at compile time, same as
 * name_gpio0~F vs. plat_gpio_cfg[].
 */

// clang-format off
// name_sgpioX_OUT/IN(APPLY): dense per-bit name lists, one entry per flat
// sgpio number, in the same order as plat_sgpio_cfg[] in plat_gpio.c. Feeds
// both enum _SGPIO_NUMS_ (below) and sgpio_name[] (in plat_gpio.c), same as
// name_gpio0~F above feeds _GPIO_NUMS_ and gpio_name[].
#define name_sgpio0_out	\
	/* bit 0-7 */ \
	gpio_name_to_num(Reserve_SGPIO0_OUT0) \
	gpio_name_to_num(Reserve_SGPIO0_OUT1) \
	gpio_name_to_num(Reserve_SGPIO0_OUT2) \
	gpio_name_to_num(Reserve_SGPIO0_OUT3) \
	gpio_name_to_num(Reserve_SGPIO0_OUT4) \
	gpio_name_to_num(Reserve_SGPIO0_OUT5) \
	gpio_name_to_num(Reserve_SGPIO0_OUT6) \
	gpio_name_to_num(Reserve_SGPIO0_OUT7) \
	/* bit 8-15 */ \
	gpio_name_to_num(Reserve_SGPIO0_OUT8) \
	gpio_name_to_num(Reserve_SGPIO0_OUT9) \
	gpio_name_to_num(Reserve_SGPIO0_OUT10) \
	gpio_name_to_num(Reserve_SGPIO0_OUT11) \
	gpio_name_to_num(Reserve_SGPIO0_OUT12) \
	gpio_name_to_num(Reserve_SGPIO0_OUT13) \
	gpio_name_to_num(Reserve_SGPIO0_OUT14) \
	gpio_name_to_num(Reserve_SGPIO0_OUT15) \
	/* bit 16-23 */ \
	gpio_name_to_num(Reserve_SGPIO0_OUT16) \
	gpio_name_to_num(Reserve_SGPIO0_OUT17) \
	gpio_name_to_num(Reserve_SGPIO0_OUT18) \
	gpio_name_to_num(Reserve_SGPIO0_OUT19) \
	gpio_name_to_num(Reserve_SGPIO0_OUT20) \
	gpio_name_to_num(Reserve_SGPIO0_OUT21) \
	gpio_name_to_num(Reserve_SGPIO0_OUT22) \
	gpio_name_to_num(Reserve_SGPIO0_OUT23) \
	/* bit 24-31 */ \
	gpio_name_to_num(Reserve_SGPIO0_OUT24) \
	gpio_name_to_num(Reserve_SGPIO0_OUT25) \
	gpio_name_to_num(Reserve_SGPIO0_OUT26) \
	gpio_name_to_num(Reserve_SGPIO0_OUT27) \
	gpio_name_to_num(Reserve_SGPIO0_OUT28) \
	gpio_name_to_num(Reserve_SGPIO0_OUT29) \
	gpio_name_to_num(Reserve_SGPIO0_OUT30) \
	gpio_name_to_num(Reserve_SGPIO0_OUT31) \
	/* bit 32-39 */ \
	gpio_name_to_num(Reserve_SGPIO0_OUT32) \
	gpio_name_to_num(Reserve_SGPIO0_OUT33) \
	gpio_name_to_num(Reserve_SGPIO0_OUT34) \
	gpio_name_to_num(Reserve_SGPIO0_OUT35) \
	gpio_name_to_num(Reserve_SGPIO0_OUT36) \
	gpio_name_to_num(Reserve_SGPIO0_OUT37) \
	gpio_name_to_num(Reserve_SGPIO0_OUT38) \
	gpio_name_to_num(Reserve_SGPIO0_OUT39) \
	/* bit 40-47 */ \
	gpio_name_to_num(Reserve_SGPIO0_OUT40) \
	gpio_name_to_num(Reserve_SGPIO0_OUT41) \
	gpio_name_to_num(Reserve_SGPIO0_OUT42) \
	gpio_name_to_num(Reserve_SGPIO0_OUT43) \
	gpio_name_to_num(Reserve_SGPIO0_OUT44) \
	gpio_name_to_num(Reserve_SGPIO0_OUT45) \
	gpio_name_to_num(Reserve_SGPIO0_OUT46) \
	gpio_name_to_num(Reserve_SGPIO0_OUT47) \
	/* bit 48-55 */ \
	gpio_name_to_num(Reserve_SGPIO0_OUT48) \
	gpio_name_to_num(Reserve_SGPIO0_OUT49) \
	gpio_name_to_num(Reserve_SGPIO0_OUT50) \
	gpio_name_to_num(Reserve_SGPIO0_OUT51) \
	gpio_name_to_num(Reserve_SGPIO0_OUT52) \
	gpio_name_to_num(Reserve_SGPIO0_OUT53) \
	gpio_name_to_num(Reserve_SGPIO0_OUT54) \
	gpio_name_to_num(Reserve_SGPIO0_OUT55) \
	/* bit 56-63 */ \
	gpio_name_to_num(Reserve_SGPIO0_OUT56) \
	gpio_name_to_num(Reserve_SGPIO0_OUT57) \
	gpio_name_to_num(Reserve_SGPIO0_OUT58) \
	gpio_name_to_num(Reserve_SGPIO0_OUT59) \
	gpio_name_to_num(Reserve_SGPIO0_OUT60) \
	gpio_name_to_num(Reserve_SGPIO0_OUT61) \
	gpio_name_to_num(wMMC_SGPIO0_RDY) \
	gpio_name_to_num(Reserve_SGPIO0_OUT63)
#define name_sgpio0_in	\
	/* bit 0-7 */ \
	gpio_name_to_num(ZORA00_VDD_SMBALERT_N) \
	gpio_name_to_num(ZORA01_VDD_SMBALERT_N) \
	gpio_name_to_num(ZORA10_VDD_SMBALERT_N) \
	gpio_name_to_num(ZORA11_VDD_SMBALERT_N) \
	gpio_name_to_num(MAX_EW2_VDD_SMBALERT_N) \
	gpio_name_to_num(MAX_EW1_VDD_SMBALERT_N) \
	gpio_name_to_num(OWL_E_SMBALRT_N) \
	gpio_name_to_num(OWL_W_SMBALRT_N) \
	/* bit 8-15 */ \
	gpio_name_to_num(VDDPHY_HBM0145_SMBALERT_N) \
	gpio_name_to_num(VDDPHY_HBM2367_SMBALERT_N) \
	gpio_name_to_num(VDDC_HBM0145_SMBALERT_N) \
	gpio_name_to_num(VDDC_HBM2367_SMBALERT_N) \
	gpio_name_to_num(VPP_HBM_P1V8_SMBALERT_N) \
	gpio_name_to_num(CHASSIS1_LEAK_Q_N_FF) \
	gpio_name_to_num(LEAK1_DETECT_ALERT_CPLD_N) \
	gpio_name_to_num(LEAK1_DETECT_FF) \
	/* bit 16-23 */ \
	gpio_name_to_num(PRSNT_CHASSIS1_LEAK_CABLE_R_N) \
	gpio_name_to_num(ZORA00_HBM_CATTRIP_ALARM) \
	gpio_name_to_num(ZORA01_HBM_CATTRIP_ALARM) \
	gpio_name_to_num(ZORA10_HBM_CATTRIP_ALARM) \
	gpio_name_to_num(ZORA11_HBM_CATTRIP_ALARM) \
	gpio_name_to_num(OWL_E_SOC_CATTRIP_ALARM) \
	gpio_name_to_num(OWL_W_SOC_CATTRIP_ALARM) \
	gpio_name_to_num(HAMSA_CATTRIP_ALARM) \
	/* bit 24-31 */ \
	gpio_name_to_num(SMB_ZORA00_CRM_LS_LVC33_ALERT_N) \
	gpio_name_to_num(SMB_ZORA01_CRM_LS_LVC33_ALERT_N) \
	gpio_name_to_num(SMB_ZORA10_CRM_LS_LVC33_ALERT_N) \
	gpio_name_to_num(SMB_ZORA11_CRM_LS_LVC33_ALERT_N) \
	gpio_name_to_num(SMB_HAMSA_LS_LVC33_ALERT_N) \
	gpio_name_to_num(FM_MODULE_PWRBRK_R_N) \
	gpio_name_to_num(IRQ_TMP75_1_ALERT_R_N) \
	gpio_name_to_num(IRQ_TMP75_2_ALERT_R_N) \
	/* bit 32-39 */ \
	gpio_name_to_num(IRQ_TMP75_3_ALERT_R_N) \
	gpio_name_to_num(TEMP_MON2_OVERT_N) \
	gpio_name_to_num(TEMP_MON3_OVERT_N) \
	gpio_name_to_num(TEMP_MON4_OVERT_N) \
	gpio_name_to_num(TEMP_MON5_OVERT_N) \
	gpio_name_to_num(RADSOK_CONN_PRSNT) \
	gpio_name_to_num(VRHOT_ZORA00_VDD_N) \
	gpio_name_to_num(VRHOT_ZORA01_VDD_N) \
	/* bit 40-47 */ \
	gpio_name_to_num(VRHOT_ZORA10_VDD_N) \
	gpio_name_to_num(VRHOT_ZORA11_VDD_N) \
	gpio_name_to_num(ZORA00_STANDALONE_MODE_R) \
	gpio_name_to_num(ZORA01_STANDALONE_MODE_R) \
	gpio_name_to_num(ZORA10_STANDALONE_MODE_R) \
	gpio_name_to_num(ZORA11_STANDALONE_MODE_R) \
	gpio_name_to_num(FM_ASIC_0_THERMTRIP_N_LATCH) \
	gpio_name_to_num(Reserve_SGPIO0_IN47) \
	/* bit 48-55 */ \
	gpio_name_to_num(Reserve_SGPIO0_IN48) \
	gpio_name_to_num(Reserve_SGPIO0_IN49) \
	gpio_name_to_num(Reserve_SGPIO0_IN50) \
	gpio_name_to_num(Reserve_SGPIO0_IN51) \
	gpio_name_to_num(Reserve_SGPIO0_IN52) \
	gpio_name_to_num(Reserve_SGPIO0_IN53) \
	gpio_name_to_num(Reserve_SGPIO0_IN54) \
	gpio_name_to_num(Reserve_SGPIO0_IN55) \
	/* bit 56-63 */ \
	gpio_name_to_num(Reserve_SGPIO0_IN56) \
	gpio_name_to_num(Reserve_SGPIO0_IN57) \
	gpio_name_to_num(Reserve_SGPIO0_IN58) \
	gpio_name_to_num(Reserve_SGPIO0_IN59) \
	gpio_name_to_num(Reserve_SGPIO0_IN60) \
	gpio_name_to_num(Reserve_SGPIO0_IN61) \
	gpio_name_to_num(HOST_PWRGD) \
	gpio_name_to_num(Reserve_SGPIO0_IN63)
#define name_sgpio1_out	\
	/* bit 0-7 */ \
	gpio_name_to_num(wMMC_SGPIO_BUFF0_100M_LOSB_N) \
	gpio_name_to_num(wMMC_SGPIO_BUFF1_100M_LOSB_N) \
	gpio_name_to_num(wMMC_SGPIO_BUFF2_100M_LOSB_N) \
	gpio_name_to_num(wMMC_SGPIO_BUFF3_100M_LOSB_N) \
	gpio_name_to_num(wMMC_SGPIO_ZORA00_PWRCAP_LV1) \
	gpio_name_to_num(wMMC_SGPIO_ZORA00_PWRCAP_LV2) \
	gpio_name_to_num(wMMC_SGPIO_ZORA00_PWRCAP_LV3) \
	gpio_name_to_num(wMMC_SGPIO_ZORA_PWRCAP_LV1) \
	/* bit 8-15 */ \
	gpio_name_to_num(wMMC_SGPIO_ZORA_PWRCAP_LV2) \
	gpio_name_to_num(wMMC_SGPIO_ZORA_PWRCAP_LV3) \
	gpio_name_to_num(wMMC_SGPIO_LV1_SW_EN) \
	gpio_name_to_num(wMMC_SGPIO_LV2_SW_EN) \
	gpio_name_to_num(wMMC_SGPIO_LV3_SW_EN) \
	gpio_name_to_num(wMMC_SGPIO_HAMSA_VRHOT) \
	gpio_name_to_num(Reserve_SGPIO1_OUT14) \
	gpio_name_to_num(Reserve_SGPIO1_OUT15) \
	/* bit 16-23 */ \
	gpio_name_to_num(Reserve_SGPIO1_OUT16) \
	gpio_name_to_num(Reserve_SGPIO1_OUT17) \
	gpio_name_to_num(Reserve_SGPIO1_OUT18) \
	gpio_name_to_num(Reserve_SGPIO1_OUT19) \
	gpio_name_to_num(Reserve_SGPIO1_OUT20) \
	gpio_name_to_num(Reserve_SGPIO1_OUT21) \
	gpio_name_to_num(Reserve_SGPIO1_OUT22) \
	gpio_name_to_num(Reserve_SGPIO1_OUT23) \
	/* bit 24-31 */ \
	gpio_name_to_num(Reserve_SGPIO1_OUT24) \
	gpio_name_to_num(Reserve_SGPIO1_OUT25) \
	gpio_name_to_num(Reserve_SGPIO1_OUT26) \
	gpio_name_to_num(Reserve_SGPIO1_OUT27) \
	gpio_name_to_num(Reserve_SGPIO1_OUT28) \
	gpio_name_to_num(Reserve_SGPIO1_OUT29) \
	gpio_name_to_num(Reserve_SGPIO1_OUT30) \
	gpio_name_to_num(Reserve_SGPIO1_OUT31) \
	/* bit 32-39 */ \
	gpio_name_to_num(Reserve_SGPIO1_OUT32) \
	gpio_name_to_num(Reserve_SGPIO1_OUT33) \
	gpio_name_to_num(Reserve_SGPIO1_OUT34) \
	gpio_name_to_num(Reserve_SGPIO1_OUT35) \
	gpio_name_to_num(Reserve_SGPIO1_OUT36) \
	gpio_name_to_num(Reserve_SGPIO1_OUT37) \
	gpio_name_to_num(Reserve_SGPIO1_OUT38) \
	gpio_name_to_num(Reserve_SGPIO1_OUT39) \
	/* bit 40-47 */ \
	gpio_name_to_num(Reserve_SGPIO1_OUT40) \
	gpio_name_to_num(Reserve_SGPIO1_OUT41) \
	gpio_name_to_num(Reserve_SGPIO1_OUT42) \
	gpio_name_to_num(Reserve_SGPIO1_OUT43) \
	gpio_name_to_num(Reserve_SGPIO1_OUT44) \
	gpio_name_to_num(Reserve_SGPIO1_OUT45) \
	gpio_name_to_num(Reserve_SGPIO1_OUT46) \
	gpio_name_to_num(Reserve_SGPIO1_OUT47) \
	/* bit 48-55 */ \
	gpio_name_to_num(Reserve_SGPIO1_OUT48) \
	gpio_name_to_num(Reserve_SGPIO1_OUT49) \
	gpio_name_to_num(Reserve_SGPIO1_OUT50) \
	gpio_name_to_num(Reserve_SGPIO1_OUT51) \
	gpio_name_to_num(Reserve_SGPIO1_OUT52) \
	gpio_name_to_num(Reserve_SGPIO1_OUT53) \
	gpio_name_to_num(Reserve_SGPIO1_OUT54) \
	gpio_name_to_num(Reserve_SGPIO1_OUT55) \
	/* bit 56-63 */ \
	gpio_name_to_num(Reserve_SGPIO1_OUT56) \
	gpio_name_to_num(Reserve_SGPIO1_OUT57) \
	gpio_name_to_num(Reserve_SGPIO1_OUT58) \
	gpio_name_to_num(Reserve_SGPIO1_OUT59) \
	gpio_name_to_num(Reserve_SGPIO1_OUT60) \
	gpio_name_to_num(Reserve_SGPIO1_OUT61) \
	gpio_name_to_num(wMMC_SGPIO1_RDY) \
	gpio_name_to_num(Reserve_SGPIO1_OUT63)
#define name_sgpio1_in	\
	/* bit 0-7 */ \
	gpio_name_to_num(FM_52V_PWROFF_SENSE_PLD) \
	gpio_name_to_num(FM_3V3_PWROFF_SENSE_PLD) \
	gpio_name_to_num(PWRGD_P1V8_AUX) \
	gpio_name_to_num(PWRGD_P1V2_AUX) \
	gpio_name_to_num(PWR_EN) \
	gpio_name_to_num(P12V_UBC1_PWRGD) \
	gpio_name_to_num(P12V_UBC2_PWRGD) \
	gpio_name_to_num(PWRGD_P3V3_R) \
	/* bit 8-15 */ \
	gpio_name_to_num(PWRGD_P1V8_RC38108) \
	gpio_name_to_num(PWRGD_P4V2_R) \
	gpio_name_to_num(PWRGD_P3V3_CLK) \
	gpio_name_to_num(PWRGD_P3V3_CLK_1) \
	gpio_name_to_num(PWRGD_LDO_IN_1V2_R) \
	gpio_name_to_num(PWRGD_P5V_R) \
	gpio_name_to_num(PWRGD_P1V8_R) \
	gpio_name_to_num(PWRGD_P0V75_AVDD_HCSL_R) \
	/* bit 16-23 */ \
	gpio_name_to_num(PWRGD_ZORA00_VDDH_PLD) \
	gpio_name_to_num(PWRGD_ZORA01_VDDH_PLD) \
	gpio_name_to_num(PWRGD_ZORA10_VDDH_PLD) \
	gpio_name_to_num(PWRGD_ZORA11_VDDH_PLD) \
	gpio_name_to_num(PWRGD_ZORA00_VDDL_PLD) \
	gpio_name_to_num(PWRGD_ZORA01_VDDL_PLD) \
	gpio_name_to_num(PWRGD_ZORA10_VDDL_PLD) \
	gpio_name_to_num(PWRGD_ZORA11_VDDL_PLD) \
	/* bit 24-31 */ \
	gpio_name_to_num(PWRGD_HAMSA_VDD_R) \
	gpio_name_to_num(PWRGD_OWL_E_VDD_PLD) \
	gpio_name_to_num(PWRGD_OWL_W_VDD_PLD) \
	gpio_name_to_num(PWRGD_MAX_N_VDD_PLD) \
	gpio_name_to_num(PWRGD_MAX_EW2_VDD_PLD) \
	gpio_name_to_num(PWRGD_MAX_M_VDD_PLD) \
	gpio_name_to_num(PWRGD_MAX_EW1_VDD_PLD) \
	gpio_name_to_num(PWRGD_MAX_S_VDD_PLD) \
	/* bit 32-39 */ \
	gpio_name_to_num(PWRGD_OWL_E_TRVDD0P75_PLD) \
	gpio_name_to_num(PWRGD_OWL_W_TRVDD0P75_PLD) \
	gpio_name_to_num(PWRGD_VDDPHY_HBM0145_PLD) \
	gpio_name_to_num(PWRGD_VDDPHY_HBM2367_PLD) \
	gpio_name_to_num(PWRGD_P1V5_PLL_VDDA_OWL_E) \
	gpio_name_to_num(PWRGD_P1V5_PLL_VDDA_OWL_W) \
	gpio_name_to_num(PWRGD_P1V5_PLL_VDDA_SOC) \
	gpio_name_to_num(PWRGD_P1V2_PLL_VDDA_SOC) \
	/* bit 40-47 */ \
	gpio_name_to_num(PWRGD_P1V2_PLL_VDDA_OWL_W) \
	gpio_name_to_num(PWRGD_VPP_HBM0145_R) \
	gpio_name_to_num(PWRGD_VPP_HBM2367_R) \
	gpio_name_to_num(PWRGD_VDDC_HBM0145) \
	gpio_name_to_num(PWRGD_VDDC_HBM2367_PLD) \
	gpio_name_to_num(PWRGD_VDDQ_HBM0145_PLD) \
	gpio_name_to_num(PWRGD_VDDQ_HBM2367_PLD) \
	gpio_name_to_num(PWRGD_VDDQL_HBM0145_PLD) \
	/* bit 48-55 */ \
	gpio_name_to_num(PWRGD_VDDQL_HBM2367_PLD) \
	gpio_name_to_num(PWRGD_HAMSA_AVDD_PCIE_PLD) \
	gpio_name_to_num(PWRGD_OWL_E_TRVDD0P9_PLD) \
	gpio_name_to_num(PWRGD_OWL_W_TRVDD0P9_PLD) \
	gpio_name_to_num(PWRGD_P0V9_OWL_E_PVDD) \
	gpio_name_to_num(PWRGD_P0V9_OWL_W_PVDD) \
	gpio_name_to_num(PWRGD_PVDD1P5) \
	gpio_name_to_num(PWRGD_P1V5_E_RVDD) \
	/* bit 56-63 */ \
	gpio_name_to_num(PWRGD_P1V5_W_RVDD) \
	gpio_name_to_num(PWRGD_HAMSA_VDDHRXTX_PCIE3) \
	gpio_name_to_num(PWRGD_HAMSA_VDDHRXTX_PCIE2) \
	gpio_name_to_num(PWRGD_HAMSA_VDDHRXTX_PCIE1) \
	gpio_name_to_num(PWRGD_HAMSA_VDDHRXTX_PCIE0) \
	gpio_name_to_num(MODULE_PWRGD) \
	gpio_name_to_num(PWRSEQ_TO_FAULT) \
	gpio_name_to_num(PWER_FAULT)
// clang-format on

#define gpio_name_to_num(x) x,
enum _SGPIO_NUMS_ { name_sgpio0_out name_sgpio0_in name_sgpio1_out name_sgpio1_in };

extern enum _SGPIO_NUMS_ SGPIO_NUMS;
#undef gpio_name_to_num

#endif
