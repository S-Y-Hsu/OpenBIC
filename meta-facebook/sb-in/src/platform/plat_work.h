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

#ifndef PLAT_WORK_H
#define PLAT_WORK_H

#include <zephyr.h>

/* platform work queue */
struct plat_work {
	const char *name;
	int (*fn)(void); // 0 on success, negative errno on failure
	uint32_t interval_ms;
	uint8_t max_fail; // consecutive failures before the work is suspended
	uint8_t fail_cnt;
	bool suspended;
	struct k_work_delayable work;
};

void plat_init_platform_queue(void);
const struct plat_work *plat_get_work_list(size_t *count);
void plat_vr_smbalert_trigger_scan(void);
void plat_sgpio_event_trigger_scan(void);

#endif
