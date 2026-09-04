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

#include <stdlib.h>
#include <shell/shell.h>
#include "plat_cpld.h"
#include "plat_gpio.h"

#define ARKE_BOARD_POWER_ENABLE 0xFF

#define HAMSA_POWER_ON_RESET_PLD_L_BIT 5
#define HAMSA_SYS_RST_PLD_L_BIT 2

void cmd_spi_disable_hamsa(const struct shell *shell, size_t argc, char **argv)
{
	shell_print(shell, "set SPI_MUX to ASIC");
	gpio_set(SPI_HAMSA_MUX_IN1, 0);
	// waiting for CPLD team to confirm reset reg location
	// uint8_t reset_release = ARKE_BOARD_POWER_ENABLE;
	// if (!plat_write_cpld(CPLD_OFFSET_ASIC_RESET, &reset_release)) {
	// 	shell_error(shell, "failed to write 0xFF to CPLD reset register");
	// 	return;
	// }
	shell_print(shell, "all reset signals are inactive");
}

void cmd_spi_enable_hamsa(const struct shell *shell, size_t argc, char **argv)
{
	shell_print(shell, "set SPI_MUX_HAMSA to MMC");
	gpio_set(SPI_HAMSA_MUX_IN1, 1);
	// if (!set_cpld_bit(CPLD_OFFSET_ASIC_RESET, HAMSA_POWER_ON_RESET_PLD_L_BIT, 0) ||
	//     !set_cpld_bit(CPLD_OFFSET_ASIC_RESET, HAMSA_SYS_RST_PLD_L_BIT, 0)) {
	// 	shell_error(shell, "failed to drive HAMSA reset bits low");
	// 	return;
	// }
	shell_print(shell, "HAMSA is held in reset");
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_set_spimux_oob_cmds,
			       SHELL_CMD(enable, NULL, "Hamsa spi enable",
					 cmd_spi_enable_hamsa),
			       SHELL_CMD(disable, NULL, "Hamsa spi disable",
					 cmd_spi_disable_hamsa),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(set_spimux_oob, &sub_set_spimux_oob_cmds, "enable/disable Hamsa SPI MUX", NULL);