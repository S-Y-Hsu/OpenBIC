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

#ifndef SHELL_PLAT_TEMP_H
#define SHELL_PLAT_TEMP_H

#include <shell/shell.h>

/* The whole `temp` command tree is defined in shell_plat_temp.c; each subcommand group
 * implements its handlers in shell_plat_temp_<group>.c
 */

/* temp status, see shell_plat_temp_status.c */
int cmd_temp_status(const struct shell *shell, size_t argc, char **argv);

/* temp threshold, see shell_plat_temp_threshold.c */
int cmd_temp_threshold_get(const struct shell *shell, size_t argc, char **argv);
int cmd_temp_threshold_set(const struct shell *shell, size_t argc, char **argv);

/* temp thermaltrip, see shell_plat_temp_thermaltrip.c */
void temp_thermaltrip_name_entry_get(size_t idx, struct shell_static_entry *entry);
int cmd_temp_thermaltrip_get(const struct shell *shell, size_t argc, char **argv);
int cmd_temp_thermaltrip_set(const struct shell *shell, size_t argc, char **argv);

#endif
