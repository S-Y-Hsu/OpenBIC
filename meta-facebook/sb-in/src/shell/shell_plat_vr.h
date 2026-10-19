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

#ifndef SHELL_PLAT_VR_H
#define SHELL_PLAT_VR_H

#include <shell/shell.h>

/* The whole `vr` command tree is defined in shell_plat_vr.c; each subcommand group
 * implements its handlers in shell_plat_vr_<group>.c
 */

/* Parse a rail name argument, print an error and return false if it is invalid */
bool vr_rail_arg_get(const struct shell *shell, char *arg, uint8_t *rail);

/* vr pmbus, see shell_plat_vr_pmbus.c */
int cmd_vr_pmbus_read(const struct shell *shell, size_t argc, char **argv);
int cmd_vr_pmbus_write(const struct shell *shell, size_t argc, char **argv);

/* vr status, see shell_plat_vr_status.c */
int cmd_vr_status_get(const struct shell *shell, size_t argc, char **argv);
int cmd_vr_status_clear(const struct shell *shell, size_t argc, char **argv);

/* vr peak, see shell_plat_vr_peak.c */
int cmd_vr_peak_get(const struct shell *shell, size_t argc, char **argv);
int cmd_vr_peak_clear(const struct shell *shell, size_t argc, char **argv);

#endif
