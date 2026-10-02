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

#include "plat_work.h"
#include "plat_gpio.h"
#include "plat_ioexp.h"
#include <errno.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(plat_work);

/* platform work queue
 * Replaces the quick_sensor_poll_handler thread used in sb-el: each periodic job is a
 * delayable work on this queue instead of sharing one polling loop. A work that fails
 * max_fail times in a row is suspended until reboot, so a broken board does not flood the log.
 */
#define PLAT_WORK_Q_STACK_SIZE 2048
#define SGPIO_BUFF_LOSB_MASK GENMASK(2, 0) // PCA6416A_U753 P1 bit0~2

K_THREAD_STACK_DEFINE(plat_work_q_stack, PLAT_WORK_Q_STACK_SIZE);
static struct k_work_q plat_work_q_obj;

static int sgpio_buff_losb_poll(void)
{
	static bool synced = false;
	static uint8_t last_val = 0;
	uint8_t val = 0;

	if (!pca6416a_i2c_read(PCA6416A_U753, PCA6416A_INPUT_PORT_1, &val, 1))
		return -EIO;

	val &= SGPIO_BUFF_LOSB_MASK;
	if (synced && val == last_val)
		return 0;

	sgpio_set(wMMC_SGPIO_BUFF0_100M_LOSB_N, (val & BIT(0)) ? GPIO_HIGH : GPIO_LOW);
	sgpio_set(wMMC_SGPIO_BUFF1_100M_LOSB_N, (val & BIT(1)) ? GPIO_HIGH : GPIO_LOW);
	sgpio_set(wMMC_SGPIO_BUFF2_100M_LOSB_N, (val & BIT(2)) ? GPIO_HIGH : GPIO_LOW);

	last_val = val;
	synced = true;
	return 0;
}

static struct plat_work plat_work_list[] = {
	{ .name = "sgpio_buff_losb", .fn = sgpio_buff_losb_poll, .interval_ms = 100, .max_fail = 10 },
};

static void plat_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct plat_work *pw = CONTAINER_OF(dwork, struct plat_work, work);

	if (pw->fn() < 0) {
		if (++pw->fail_cnt >= pw->max_fail) {
			pw->suspended = true;
			LOG_ERR("plat work %s suspended after %u consecutive failures", pw->name,
				pw->fail_cnt);
			return;
		}
	} else {
		pw->fail_cnt = 0;
	}

	k_work_schedule_for_queue(&plat_work_q_obj, &pw->work, K_MSEC(pw->interval_ms));
}

const struct plat_work *plat_get_work_list(size_t *count)
{
	*count = ARRAY_SIZE(plat_work_list);
	return plat_work_list;
}

void plat_init_platform_queue(void)
{
	k_work_queue_start(&plat_work_q_obj, plat_work_q_stack,
			   K_THREAD_STACK_SIZEOF(plat_work_q_stack), CONFIG_MAIN_THREAD_PRIORITY,
			   NULL);
	k_thread_name_set(&plat_work_q_obj.thread, "plat_work_q");

	for (size_t i = 0; i < ARRAY_SIZE(plat_work_list); i++) {
		k_work_init_delayable(&plat_work_list[i].work, plat_work_handler);
		k_work_schedule_for_queue(&plat_work_q_obj, &plat_work_list[i].work, K_NO_WAIT);
	}
}
