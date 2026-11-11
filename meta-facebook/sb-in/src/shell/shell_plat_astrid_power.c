#include <stdlib.h>
#include <string.h>
#include <shell/shell.h>
#include "plat_cpld.h"
#include <logging/log.h>
#include "plat_gpio.h"
#include "plat_isr.h"
#include "plat_i2c.h"
#include "plat_pldm_sensor.h"
#include "plat_class.h"
#include "plat_util.h"

#define POWER_CYCLE_OFF_DELAY_MS 5000

#define STEP_ON_MAX_PWRGD 4
#define STEP_ON_PWRGD_TIMEOUT_MS 1500
#define STEP_ON_PWRGD_POLL_MS 10

typedef struct {
	const char *name;
	bool active_low; // _EN_N: write 0 to enable
	uint8_t pwrgd_cnt; // 0: no PWRGD to check (clock enable)
	uint8_t pwrgd[STEP_ON_MAX_PWRGD]; // SGPIO number
} astrid_step_on_t;

#define PWRGD_LIST(...) .pwrgd_cnt = sizeof((uint8_t[]){ __VA_ARGS__ }), .pwrgd = { __VA_ARGS__ }
#define STEP_ON(_name, ...)                                                                        \
	{                                                                                          \
		.name = _name, .active_low = false, PWRGD_LIST(__VA_ARGS__)                        \
	}
#define STEP_ON_NO_PWRGD(_name, _active_low)                                                       \
	{                                                                                          \
		.name = _name, .active_low = _active_low, .pwrgd_cnt = 0                           \
	}

/* Power-up order. Step i is CPLD reg (0x3B + i / 8) bit (7 - i % 8), 0x3B bit7 ~ 0x41 bit6. */
// clang-format off
static const astrid_step_on_t step_on_table[] = {
	/* 0x3B */
	STEP_ON("FM_ASTRID_EN", P12V_UBC1_PWRGD, P12V_UBC2_PWRGD),
	STEP_ON("FM_P3V3_EN", PWRGD_P3V3_R, PWRGD_P1V8_RC38108),
	STEP_ON("FM_P4V2_EN", PWRGD_P4V2_R),
	STEP_ON("FM_P3V3_CLK_EN", PWRGD_P3V3_CLK, PWRGD_P3V3_CLK_1),
	STEP_ON("FM_P5V_EN", PWRGD_P5V_R),
	STEP_ON("LDO_IN_1V2_EN", PWRGD_LDO_IN_1V2_R),
	STEP_ON("FM_P1V80_EN", PWRGD_P1V8_R),
	STEP_ON("FM_AVDD_HCSL_EN", PWRGD_P0V75_AVDD_HCSL_R),
	/* 0x3C */
	STEP_ON("FM_ZORA00_VDDH_EN", PWRGD_ZORA00_VDDH_PLD),
	STEP_ON("FM_ZORA01_VDDH_EN", PWRGD_ZORA01_VDDH_PLD),
	STEP_ON("FM_ZORA10_VDDH_EN", PWRGD_ZORA10_VDDH_PLD),
	STEP_ON("FM_ZORA11_VDDH_EN", PWRGD_ZORA11_VDDH_PLD),
	STEP_ON("FM_ZORA00_VDDL_EN", PWRGD_ZORA00_VDDL_PLD),
	STEP_ON("FM_ZORA01_VDDL_EN", PWRGD_ZORA01_VDDL_PLD),
	STEP_ON("FM_ZORA10_VDDL_EN", PWRGD_ZORA10_VDDL_PLD),
	STEP_ON("FM_ZORA11_VDDL_EN", PWRGD_ZORA11_VDDL_PLD),
	/* 0x3D */
	STEP_ON("FM_HAMSA_VDD_EN", PWRGD_HAMSA_VDD_R),
	STEP_ON("FM_OWL_E_VDD_EN", PWRGD_OWL_E_VDD_PLD),
	STEP_ON("FM_OWL_W_VDD_EN", PWRGD_OWL_W_VDD_PLD),
	STEP_ON("FM_MAX_N_VDD_EN", PWRGD_MAX_N_VDD_PLD),
	STEP_ON("FM_MAX_EW2_VDD_EN", PWRGD_MAX_EW2_VDD_PLD),
	STEP_ON("FM_MAX_M_VDD_EN", PWRGD_MAX_M_VDD_PLD),
	STEP_ON("FM_MAX_EW1_VDD_EN", PWRGD_MAX_EW1_VDD_PLD),
	STEP_ON("FM_MAX_S_VDD_EN", PWRGD_MAX_S_VDD_PLD),
	/* 0x3E */
	STEP_ON("FM_OWL_E_TRVDD0P75_EN", PWRGD_OWL_E_TRVDD0P75_PLD),
	STEP_ON("FM_OWL_W_TRVDD0P75_EN", PWRGD_OWL_W_TRVDD0P75_PLD),
	STEP_ON("FM_VDDPHY_HBM0145_EN", PWRGD_VDDPHY_HBM0145_PLD),
	STEP_ON("FM_VDDPHY_HBM2367_EN", PWRGD_VDDPHY_HBM2367_PLD),
	STEP_ON("FM_P1V5_PLL_VDDA_OWL_EN", PWRGD_P1V5_PLL_VDDA_OWL_E, PWRGD_P1V5_PLL_VDDA_OWL_W),
	STEP_ON("FM_P1V5_PLL_VDDA_SOC_EN", PWRGD_P1V5_PLL_VDDA_SOC),
	STEP_ON("FM_P1V2_PLL_VDDA_SOC_EN", PWRGD_P1V2_PLL_VDDA_SOC),
	STEP_ON("FM_P1V2_PLL_VDDA_OWL_EN", PWRGD_P1V2_PLL_VDDA_OWL_W),
	/* 0x3F */
	STEP_ON_NO_PWRGD("FM_AEGIS_CLK_48MHZ_EN", false),
	STEP_ON_NO_PWRGD("FM_AEGIS_CLK_100MHZ_EN_N", true),
	STEP_ON_NO_PWRGD("FM_ASTRID_CLK_312MHZ_EN_N", true),
	STEP_ON("FM_VPP_HBM0145_EN", PWRGD_VPP_HBM0145_R),
	STEP_ON("FM_VPP_HBM2367_EN", PWRGD_VPP_HBM2367_R),
	STEP_ON("FM_VDDC_HBM0145_EN", PWRGD_VDDC_HBM0145),
	STEP_ON("FM_VDDC_HBM2367_EN", PWRGD_VDDC_HBM2367_PLD),
	STEP_ON("FM_VDDQ_HBM0145_EN", PWRGD_VDDQ_HBM0145_PLD),
	/* 0x40 */
	STEP_ON("FM_VDDQ_HBM2367_EN", PWRGD_VDDQ_HBM2367_PLD),
	STEP_ON("FM_VDDQL_HBM0145_EN", PWRGD_VDDQL_HBM0145_PLD),
	STEP_ON("FM_VDDQL_HBM2367_EN", PWRGD_VDDQL_HBM2367_PLD),
	STEP_ON("FM_HAMSA_AVDD_PCIE_EN", PWRGD_HAMSA_AVDD_PCIE_PLD),
	STEP_ON("FM_OWL_E_TRVDD0P9_EN", PWRGD_OWL_E_TRVDD0P9_PLD),
	STEP_ON("FM_OWL_W_TRVDD0P9_EN", PWRGD_OWL_W_TRVDD0P9_PLD),
	STEP_ON("FM_PVDD0P9_EN", PWRGD_P0V9_OWL_E_PVDD, PWRGD_P0V9_OWL_W_PVDD),
	STEP_ON("FM_PVDD1P5_EN", PWRGD_PVDD1P5),
	/* 0x41, bit5~1 are ASIC power on reset */
	STEP_ON("FM_P1V5_RVDD_EN", PWRGD_P1V5_E_RVDD, PWRGD_P1V5_W_RVDD),
	STEP_ON("FM_HAMSA_VDDHRXTX_PCIE_EN", PWRGD_HAMSA_VDDHRXTX_PCIE0, PWRGD_HAMSA_VDDHRXTX_PCIE1,
		PWRGD_HAMSA_VDDHRXTX_PCIE2, PWRGD_HAMSA_VDDHRXTX_PCIE3),
};
// clang-format on

#define STEP_ON_COUNT ARRAY_SIZE(step_on_table)
BUILD_ASSERT(STEP_ON_COUNT == 50, "step on table must cover 0x3B bit7 ~ 0x41 bit6");

// next step to power on, 0 ~ STEP_ON_COUNT
static uint8_t power_steps = 0;

static bool astrid_power_control(uint8_t onoff)
{
	uint8_t tmp = onoff ? 0x80 : 0x00;
	return plat_write_cpld(CPLD_OFFSET_MMC_PWR_EN, &tmp);
}

static bool is_1step_mode(bool *enable)
{
	uint8_t data = 0;

	if (!plat_read_cpld(CPLD_OFFSET_VR_1STEP_FUNC_EN, &data, 1)) {
		return false;
	}

	*enable = !!(data & BIT(VR_1STEP_FUNC_EN_BIT));
	return true;
}

static bool set_1step_mode(bool enable)
{
	return plat_update_cpld(CPLD_OFFSET_VR_1STEP_FUNC_EN, BIT(VR_1STEP_FUNC_EN_BIT),
				enable ? BIT(VR_1STEP_FUNC_EN_BIT) : 0);
}

/* MMC_PWR_EN has no effect while the CPLD is in 1-step mode */
static bool check_not_in_1step_mode(const struct shell *shell)
{
	bool enable = false;

	if (!is_1step_mode(&enable)) {
		shell_error(shell, "Read CPLD offset 0x%x failed", CPLD_OFFSET_VR_1STEP_FUNC_EN);
		return false;
	}

	if (enable) {
		shell_warn(shell, "in steps on mode, run \"astrid_power disable_steps_on\" first");
		return false;
	}

	return true;
}

/* Set MMC power enable, wait for the sequence, then check MODULE_PWRGD */
static bool astrid_power_set_and_check(const struct shell *shell, uint8_t onoff)
{
	const char *action = onoff ? "on" : "off";

	if (!astrid_power_control(onoff)) {
		shell_warn(shell, "astrid power %s set cpld fail!", action);
		return false;
	}

	k_msleep(DC_ON_DELAY_TIMMING);

	if (sgpio_get(MODULE_PWRGD) != (onoff ? GPIO_HIGH : GPIO_LOW)) {
		shell_warn(shell, "astrid power %s fail!", action);
		return false;
	}

	shell_print(shell, "astrid power %s success!", action);
	return true;
}

void cmd_astrid_power_on(const struct shell *shell, size_t argc, char **argv)
{
	if (!check_not_in_1step_mode(shell)) {
		return;
	}

	astrid_power_set_and_check(shell, 1);
}

void cmd_astrid_power_off(const struct shell *shell, size_t argc, char **argv)
{
	if (!check_not_in_1step_mode(shell)) {
		return;
	}

	astrid_power_set_and_check(shell, 0);
}

void cmd_astrid_power_cycle(const struct shell *shell, size_t argc, char **argv)
{
	if (!check_not_in_1step_mode(shell)) {
		return;
	}

	if (!astrid_power_set_and_check(shell, 0)) {
		shell_warn(shell, "astrid power cycle abort!");
		return;
	}

	k_msleep(POWER_CYCLE_OFF_DELAY_MS);

	astrid_power_set_and_check(shell, 1);
}

/*
 * Turn one step on/off without a per-step reg/bit table.
 *
 * The CPLD "VR Enable pin control (1-step only)" registers are packed in power-up
 * order: step 0 is 0x3B bit7, step 1 is 0x3B bit6, ..., step 7 is 0x3B bit0, then
 * step 8 continues at 0x3C bit7, up to step 49 at 0x41 bit6. So for step i:
 *   offset = 0x3B + i / 8
 *   bit    = 7 - i % 8   (MSB first)
 * step_on_table[] must stay in exactly this order, one entry per bit, no gaps.
 *
 * active_low entries (_EN_N) are enabled by writing 0, so "on" is inverted for them.
 */
static bool step_on_set(uint8_t step, bool on)
{
	uint8_t offset = CPLD_OFFSET_VR_STEP_ON_EN_BASE + step / 8;
	uint8_t mask = BIT(7 - step % 8);
	bool high = on ^ step_on_table[step].active_low;

	// read-modify-write: only touch this step's bit, 0x41 bit5~1 are ASIC power on reset
	return plat_update_cpld(offset, mask, high ? mask : 0);
}

static bool step_on_all_pwrgd(const astrid_step_on_t *entry)
{
	for (uint8_t i = 0; i < entry->pwrgd_cnt; i++) {
		if (sgpio_get(entry->pwrgd[i]) != GPIO_HIGH) {
			return false;
		}
	}

	return true;
}

static bool step_on_wait_pwrgd(const struct shell *shell, const astrid_step_on_t *entry)
{
	bool ok = false;

	for (int t = 0; t <= STEP_ON_PWRGD_TIMEOUT_MS; t += STEP_ON_PWRGD_POLL_MS) {
		ok = step_on_all_pwrgd(entry);
		if (ok) {
			break;
		}
		k_msleep(STEP_ON_PWRGD_POLL_MS);
	}

	for (uint8_t i = 0; i < entry->pwrgd_cnt; i++) {
		shell_print(shell, "    %-35s %d", sgpio_name[entry->pwrgd[i]],
			    sgpio_get(entry->pwrgd[i]));
	}

	return ok;
}

void cmd_astrid_steps_on(const struct shell *shell, size_t argc, char **argv)
{
	if (power_steps >= STEP_ON_COUNT) {
		shell_warn(shell, "all %d steps are on, no more steps", STEP_ON_COUNT);
		return;
	}

	if (power_steps == 0) {
		if (sgpio_get(MODULE_PWRGD) == GPIO_HIGH) {
			shell_warn(shell, "astrid power is already on, skip steps on operation.");
			return;
		}

		// set before entering 1-step mode, so no DC on / sequence timeout event is logged
		set_pwr_steps_on_flag(true);
		if (!set_1step_mode(true)) {
			shell_error(shell, "enable 1-step mode fail!");
			set_pwr_steps_on_flag(false);
			return;
		}
	}

	const astrid_step_on_t *entry = &step_on_table[power_steps];

	/* Always move to the next step even if this one fails, so the lab can still
	 * power and measure the following rails. */
	if (!step_on_set(power_steps, true)) {
		shell_error(shell, "[%2d/%d] set %-35s fail", power_steps + 1, STEP_ON_COUNT,
			    entry->name);
	} else {
		shell_print(shell, "[%2d/%d] set %-35s success", power_steps + 1, STEP_ON_COUNT,
			    entry->name);
	}

	if (entry->pwrgd_cnt && !step_on_wait_pwrgd(shell, entry)) {
		shell_warn(shell, "%s PWRGD not ready", entry->name);
	}

	power_steps++;

	// VR PMBus needs P3V3 and P5V, start VR / UBC polling before MODULE_PWRGD is up
	if (get_plat_sensor_one_step_enable_flag() != ONE_STEP_POWER_MAGIC_NUMBER &&
	    sgpio_get(PWRGD_P3V3_R) == GPIO_HIGH && sgpio_get(PWRGD_P5V_R) == GPIO_HIGH) {
		set_plat_sensor_one_step_enable_flag(ONE_STEP_POWER_MAGIC_NUMBER);
	}

	if (power_steps == STEP_ON_COUNT) {
		shell_print(shell, "all steps on, MODULE_PWRGD %d", sgpio_get(MODULE_PWRGD));
	}
}

void cmd_astrid_disable_steps_on(const struct shell *shell, size_t argc, char **argv)
{
	// power off in reverse order of power on
	for (int i = STEP_ON_COUNT - 1; i >= 0; i--) {
		if (!step_on_set(i, false)) {
			shell_error(shell, "clear %-35s fail", step_on_table[i].name);
		}
	}

	if (!set_1step_mode(false)) {
		shell_error(shell, "disable 1-step mode fail!");
		return;
	}

	power_steps = 0;
	set_plat_sensor_one_step_enable_flag(0);
	set_pwr_steps_on_flag(false);
	shell_print(shell, "steps on disabled");
}

/*
 * List every PWRGD SGPIO input, no table needed: any SGPIO whose name contains
 * "PWRGD" is a power good input (no SGPIO output is named PWRGD). sgpio1_in is
 * laid out in power-up order, so the list comes out in power-up order too.
 */
void cmd_astrid_power_good_status(const struct shell *shell, size_t argc, char **argv)
{
	uint8_t total = 0;
	uint8_t not_good = 0;

	// PWER_FAULT is the last SGPIO in enum _SGPIO_NUMS_
	for (int i = 0; i <= PWER_FAULT; i++) {
		if (!sgpio_name[i] || !strstr(sgpio_name[i], "PWRGD")) {
			continue;
		}

		int value = sgpio_get(i);
		shell_print(shell, "%-35s %d", sgpio_name[i], value);

		total++;
		if (value != GPIO_HIGH) {
			not_good++;
		}
	}

	shell_print(shell, "total %d, not good %d", total, not_good);
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_astrid_power_cmd, SHELL_CMD(on, NULL, "astrid power on", cmd_astrid_power_on),
	SHELL_CMD(off, NULL, "astrid power off", cmd_astrid_power_off),
	SHELL_CMD(cycle, NULL, "astrid power cycle", cmd_astrid_power_cycle),
	SHELL_CMD(steps_on, NULL, "astrid power on one rail per call", cmd_astrid_steps_on),
	SHELL_CMD(disable_steps_on, NULL, "astrid power off all rails and leave steps on mode",
		  cmd_astrid_disable_steps_on),
	SHELL_CMD(power_good_status, NULL, "show all PWRGD SGPIO status",
		  cmd_astrid_power_good_status),
	SHELL_SUBCMD_SET_END);

/* Root of command echo */
SHELL_CMD_REGISTER(astrid_power, &sub_astrid_power_cmd, "astrid power commands", NULL);
