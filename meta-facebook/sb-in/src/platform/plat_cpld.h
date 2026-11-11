#ifndef PLAT_CPLD_H
#define PLAT_CPLD_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define CPLD_OFFSET_BOARD_REV_ID 0x14
#define CPLD_OFFSET_VR_VENDER_TYPE 0x15
#define CPLD_OFFSET_POWER_CLAMP 0x25
#define CPLD_OFFSET_USERCODE 0x32
#define CPLD_OFFSET_ASIC_BOARD_ID 0x37
#define CPLD_OFFSET_MMC_PWR_EN 0x38
#define CPLD_OFFSET_THERMALTRIP_SWITCH 0x3A
/* VR enable pin control (1-step only), 0x3B bit7 ~ 0x41 bit6 in power-up order.
 * 0x41 bit5~1 are ASIC power on reset, only touch bit7/6 there. */
#define CPLD_OFFSET_VR_STEP_ON_EN_BASE 0x3B
#define CPLD_OFFSET_VR_1STEP_FUNC_EN 0xA9
#define VR_1STEP_FUNC_EN_BIT 0
#define CPLD_OFFSET_ASIC_POWER_ON_RESET 0x41
#define CPLD_OFFSET_ASIC_SYS_RESET 0x42
#define CPLD_OFFSET_PCIE_PERST 0xA3
#define CPLD_OFFSET_MANUAL_CONTROL_STRAP 0xB2

/* VR Power Fault Registers */
#define VR_POWER_FAULT_1_REG 0x0D
#define VR_POWER_FAULT_2_REG 0x0E
#define VR_POWER_FAULT_3_REG 0x0F
#define VR_POWER_FAULT_4_REG 0x10
#define VR_POWER_FAULT_5_REG 0x11
#define VR_POWER_FAULT_6_REG 0x12
#define VR_POWER_FAULT_7_REG 0x13
#define POWER_FAULT_REG_NUM (VR_POWER_FAULT_7_REG - VR_POWER_FAULT_1_REG + 1)

/* PWRGD Event Latch Registers */
#define PWRGD_EVENT_LATCH_1_REG 0xBE
#define PWRGD_EVENT_LATCH_7_REG 0xC4
#define PWRGD_EVENT_LATCH_NUM (PWRGD_EVENT_LATCH_7_REG - PWRGD_EVENT_LATCH_1_REG + 1)

bool plat_read_cpld(uint8_t offset, uint8_t *data, uint8_t len);
bool plat_write_cpld(uint8_t offset, uint8_t *data);
bool plat_update_cpld(uint8_t offset, uint8_t mask, uint8_t val);

#endif