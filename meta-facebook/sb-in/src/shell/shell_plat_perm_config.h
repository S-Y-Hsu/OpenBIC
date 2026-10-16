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

#ifndef SHELL_PLAT_PERM_CONFIG_H
#define SHELL_PLAT_PERM_CONFIG_H

/* Perm restore of each shell command that supports [perm] */
void thermaltrip_switch_perm_restore(void);

/* Restore all shell command perm settings, called at boot */
void plat_shell_perm_restore(void);

#endif
