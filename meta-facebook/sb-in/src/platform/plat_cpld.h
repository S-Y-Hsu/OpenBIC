#ifndef PLAT_CPLD_H
#define PLAT_CPLD_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define CPLD_OFFSET_BOARD_REV_ID 0x14
#define CPLD_OFFSET_VR_VENDER_TYPE 0x15
#define CPLD_OFFSET_POWER_CLAMP 0x25
#define CPLD_OFFSET_USERCODE 0x32
#define CPLD_OFFSET_ASIC_BOARD_ID 0x3C

/* VR Power Fault Registers */
#define VR_POWER_FAULT_1_REG 0x0D
#define VR_POWER_FAULT_2_REG 0x0E
#define VR_POWER_FAULT_3_REG 0x0F
#define VR_POWER_FAULT_4_REG 0x10
#define VR_POWER_FAULT_5_REG 0x11

/* SMBus Alert Registers */
// TODO: placeholder offsets, not confirmed by CPLD team yet - do not treat as real addresses
#define SMBUS_ALERT_1_REG 0xF0
#define SMBUS_ALERT_2_REG 0xF1

typedef struct _cpld_info_ cpld_info;

typedef struct _cpld_info_ {
	uint8_t cpld_offset;
	uint8_t dc_off_defaut;
	uint8_t dc_on_defaut;
	bool is_fault_log; // if true, check the value is defaut or not
	uint8_t is_fault_bit_map; //flag for fault

	/* is_send_bmc in electra */
	bool send_to_bmc_flag; //flag for sending alert to bmc

	//temp data for last polling
	uint8_t last_polling_value;

	bool (*status_changed_cb)(cpld_info *, uint8_t *current_cpld_value, uint8_t expected_val,
				  uint8_t status_changed_bit);

	uint8_t bit_check_mask; //bit check mask

} cpld_info;

bool plat_read_cpld(uint8_t offset, uint8_t *data, uint8_t len);
bool plat_write_cpld(uint8_t offset, uint8_t *data);
void init_cpld_polling(void);
void get_cpld_polling_power_info(int *reading);
void set_cpld_polling_enable_flag(bool status);
bool get_cpld_polling_enable_flag(void);

#endif