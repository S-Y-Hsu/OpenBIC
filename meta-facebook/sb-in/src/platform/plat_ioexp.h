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

#ifndef PLAT_IOEXP_H
#define PLAT_IOEXP_H

#include <stdint.h>
#include <stdbool.h>
#include "plat_i2c.h"

/* pca6416a */
#define PCA6416A_INPUT_PORT_0 0x00
#define PCA6416A_INPUT_PORT_1 0x01
#define PCA6416A_OUTPUT_PORT_0 0x02
#define PCA6416A_OUTPUT_PORT_1 0x03
#define PCA6416A_CONFIG_0 0x06
#define PCA6416A_CONFIG_1 0x07

#define PCA6416A_U641_BUS I2C_BUS1
#define PCA6416A_U641_ADDR 0x20 // 7-bit
#define PCA6416A_U753_BUS I2C_BUS1
#define PCA6416A_U753_ADDR 0x21 // 7-bit

// TODO: fill in real initial values, 0xFF = power-on default
#define PCA6416A_U641_OUTPUT_0 0xFF
#define PCA6416A_U641_OUTPUT_1 0xFF
#define PCA6416A_U753_OUTPUT_0 0xFF // all input
#define PCA6416A_U753_OUTPUT_1 0xFF

// CONFIG: 1 = input, 0 = output
#define PCA6416A_U641_CONFIG_0 0x00 // all output
#define PCA6416A_U641_CONFIG_1 0x00 // all output
#define PCA6416A_U753_CONFIG_0 0xFF // bit0~6 input, bit7 not used (keep input)
#define PCA6416A_U753_CONFIG_1 0x07 // bit0~2 input, bit3~7 output

enum PCA6416A_IDX {
	PCA6416A_U641 = 0,
	PCA6416A_U753,
	PCA6416A_MAX,
};

/* pca6416a */
bool pca6416a_i2c_read(uint8_t idx, uint8_t offset, uint8_t *data, uint8_t len);
bool pca6416a_i2c_write(uint8_t idx, uint8_t offset, uint8_t *data, uint8_t len);
bool pca6416a_init(uint8_t idx);
/* other */
void ioexp_init(void);

#endif
