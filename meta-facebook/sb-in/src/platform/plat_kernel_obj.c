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

#include "plat_kernel_obj.h"
#include "plat_gpio.h"
#include "plat_util.h"
#include "plat_log.h"
#include "plat_hook.h"
#include "plat_ioexp.h"
#include <logging/log.h>

LOG_MODULE_REGISTER(plat_kernel_obj);

/* semaphore CPLD polling semaphore */
K_TIMER_DEFINE(ragular_cpld_polling_sem_timer, plat_ragular_cpld_polling_sem_handler, NULL);
static struct k_sem cpld_polling_sem;

void plat_ragular_cpld_polling_sem_handler(struct k_timer *timer)
{
	k_sem_give(&cpld_polling_sem);
}

void plat_activate_cpld_polling_semaphore_timer(void)
{
	k_sem_init(&cpld_polling_sem, 0, 1);
	k_timer_start(&ragular_cpld_polling_sem_timer, K_MSEC(1000), K_MSEC(1000));
}

void plat_wait_for_cpld_polling_trigger(void)
{
	k_sem_take(&cpld_polling_sem, K_FOREVER);
}

void plat_trigger_cpld_polling(void)
{
	LOG_WRN("triggering CPLD polling");
	k_sem_give(&cpld_polling_sem);
}

/* Timer for dc status checking
We expect UBC ON will trigger DC ON. */
bool ubc_status = false; // "ubc_enabled_delayed_status" in rainbow
void plat_check_ubc_delayed_timer_handler(struct k_timer *timer);
K_TIMER_DEFINE(check_ubc_delayed_timer, plat_check_ubc_delayed_timer_handler, NULL);

void plat_check_ubc_delayed_timer_handler(struct k_timer *timer)
{
	/* FM_PLD_UBC_EN_R
	 * 1 -> UBC is enabled
	 * 0 -> UBC is disabled
	 */
	bool is_ubc_enabled = (gpio_get(FM_PLD_UBC_EN_R) == GPIO_HIGH);
	ubc_status = is_ubc_enabled;
}

void plat_update_ubc_status(void)
{
	// delay for power sequence
	k_timer_start(&check_ubc_delayed_timer, K_MSEC(DC_ON_DELAY_TIMMING), K_NO_WAIT);
}

bool plat_get_ubc_status(void)
{
	return ubc_status;
}

/* platform work queue
 * Replaces the quick_sensor_poll_handler thread used in sb-el: each periodic job is a
 * delayable work on this queue instead of sharing one polling loop.
 */
#define PLAT_WORK_Q_STACK_SIZE 2048
#define SGPIO_BUFF_LOSB_POLL_INTERVAL_MS 100
#define SGPIO_BUFF_LOSB_MASK GENMASK(2, 0) // PCA6416A_U753 P1 bit0~2

K_THREAD_STACK_DEFINE(plat_work_q_stack, PLAT_WORK_Q_STACK_SIZE);
static struct k_work_q plat_work_q_obj;

static void sgpio_buff_losb_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(sgpio_buff_losb_work, sgpio_buff_losb_work_handler);

static void sgpio_buff_losb_work_handler(struct k_work *work)
{
	static bool synced = false;
	static uint8_t last_val = 0;
	uint8_t val = 0;

	if (!pca6416a_i2c_read(PCA6416A_U753, PCA6416A_INPUT_PORT_1, &val, 1)) {
		LOG_ERR("Failed to read PCA6416A_U753 input port 1");
		goto reschedule;
	}

	val &= SGPIO_BUFF_LOSB_MASK;
	if (synced && val == last_val)
		goto reschedule;

	sgpio_set(wMMC_SGPIO_BUFF0_100M_LOSB_N, (val & BIT(0)) ? GPIO_HIGH : GPIO_LOW);
	sgpio_set(wMMC_SGPIO_BUFF1_100M_LOSB_N, (val & BIT(1)) ? GPIO_HIGH : GPIO_LOW);
	sgpio_set(wMMC_SGPIO_BUFF2_100M_LOSB_N, (val & BIT(2)) ? GPIO_HIGH : GPIO_LOW);

	last_val = val;
	synced = true;

reschedule:
	k_work_schedule_for_queue(&plat_work_q_obj, &sgpio_buff_losb_work,
				  K_MSEC(SGPIO_BUFF_LOSB_POLL_INTERVAL_MS));
}

void plat_init_platform_queue(void)
{
	k_work_queue_start(&plat_work_q_obj, plat_work_q_stack,
			   K_THREAD_STACK_SIZEOF(plat_work_q_stack), CONFIG_MAIN_THREAD_PRIORITY,
			   NULL);
	k_thread_name_set(&plat_work_q_obj.thread, "plat_work_q");

	k_work_schedule_for_queue(&plat_work_q_obj, &sgpio_buff_losb_work, K_NO_WAIT);
}
