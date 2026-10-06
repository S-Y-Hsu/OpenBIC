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

#ifndef PLAT_POWER_SEQ_H
#define PLAT_POWER_SEQ_H

#include <stdint.h>

#define POWER_SEQ_FAIL_ID_NONE 0xFF

void plat_power_seq_fault_handler(void);
void plat_clear_power_seq_fault(void);
uint8_t plat_get_power_seq_fail_id(void);
const char *plat_get_power_seq_name(uint8_t index);
void plat_get_power_seq_latch(uint8_t *data); // PWRGD_EVENT_LATCH_NUM bytes

#endif
