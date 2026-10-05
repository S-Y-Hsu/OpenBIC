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

#include <logging/log.h>
#include "plat_util.h"
#include "plat_ioexp.h"

LOG_MODULE_REGISTER(plat_ioexp);

/* pca6416a */
typedef struct {
	uint8_t bus;
	uint8_t addr;
	uint8_t output[2]; // OUTPUT_0, OUTPUT_1
	uint8_t config[2]; // CONFIG_0, CONFIG_1
} ioexp_cfg_t;

static const ioexp_cfg_t pca6416a_cfg[PCA6416A_MAX] = {
	[PCA6416A_U641] = { PCA6416A_U641_BUS,
			    PCA6416A_U641_ADDR,
			    { PCA6416A_U641_OUTPUT_0, PCA6416A_U641_OUTPUT_1 },
			    { PCA6416A_U641_CONFIG_0, PCA6416A_U641_CONFIG_1 } },
	[PCA6416A_U753] = { PCA6416A_U753_BUS,
			    PCA6416A_U753_ADDR,
			    { PCA6416A_U753_OUTPUT_0, PCA6416A_U753_OUTPUT_1 },
			    { PCA6416A_U753_CONFIG_0, PCA6416A_U753_CONFIG_1 } },
};

bool pca6416a_i2c_read(uint8_t idx, uint8_t offset, uint8_t *data, uint8_t len)
{
	if (idx >= PCA6416A_MAX)
		return false;

	return plat_i2c_read(pca6416a_cfg[idx].bus, pca6416a_cfg[idx].addr, offset, data, len);
}

bool pca6416a_i2c_write(uint8_t idx, uint8_t offset, uint8_t *data, uint8_t len)
{
	if (idx >= PCA6416A_MAX)
		return false;

	return plat_i2c_write(pca6416a_cfg[idx].bus, pca6416a_cfg[idx].addr, offset, data, len);
}

bool pca6416a_init(uint8_t idx)
{
	if (idx >= PCA6416A_MAX)
		return false;

	uint8_t out[2] = { pca6416a_cfg[idx].output[0], pca6416a_cfg[idx].output[1] };
	uint8_t cfg[2] = { pca6416a_cfg[idx].config[0], pca6416a_cfg[idx].config[1] };

	// write output first so pins switch to the right level when config changes
	if (!pca6416a_i2c_write(idx, PCA6416A_OUTPUT_PORT_0, out, 2))
		return false;
	if (!pca6416a_i2c_write(idx, PCA6416A_CONFIG_0, cfg, 2))
		return false;

	return true;
}

/* other */
void ioexp_init(void)
{
	for (uint8_t i = 0; i < PCA6416A_MAX; i++) {
		if (!pca6416a_init(i))
			LOG_ERR("pca6416a %d init fail", i);
	}
}
