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
#include "hal_gpio.h"

/* Debug only: disable every SGPIO interrupt registered in sgpio_cfg[].
 * Takes effect until the next BIC reboot.
 */
static int cmd_sgpio_disable(const struct shell *shell, size_t argc, char **argv)
{
	uint16_t count = 0;

	for (uint16_t i = 0; i < SGPIO_CFG_SIZE; i++) {
		if (sgpio_cfg[i].is_init != ENABLE)
			continue;

		switch (sgpio_cfg[i].int_type) {
		case GPIO_INT_EDGE_RISING:
		case GPIO_INT_EDGE_FALLING:
		case GPIO_INT_EDGE_BOTH:
			break;
		default:
			continue;
		}

		if (sgpio_interrupt_conf(sgpio_cfg[i].number, GPIO_INT_DISABLE) != 0) {
			shell_error(shell, "Failed to disable sgpio %d interrupt",
				    sgpio_cfg[i].number);
			continue;
		}
		count++;
	}

	shell_print(shell, "Disabled %d sgpio interrupts", count);
	return 0;
}

SHELL_CMD_REGISTER(sgpio_disable, NULL, "Disable all sgpio interrupts (debug)", cmd_sgpio_disable);
