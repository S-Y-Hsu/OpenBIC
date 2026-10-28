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

#include "libutil.h"
#include "plat_util.h"
#include "plat_i2c.h"
#include "plat_pldm_sensor.h"
#include "plat_hook.h"
#include <logging/log.h>

LOG_MODULE_REGISTER(plat_util);

bool plat_i2c_read(uint8_t bus, uint8_t addr, uint8_t offset, uint8_t *data, uint8_t len)
{
	CHECK_NULL_ARG_WITH_RETURN(data, false);

	memset(data, 0, len);

	I2C_MSG i2c_msg = { 0 };
	uint8_t retry = 5;
	i2c_msg.bus = bus;
	i2c_msg.target_addr = addr;
	i2c_msg.tx_len = 1;
	i2c_msg.rx_len = len;
	i2c_msg.data[0] = offset;

	if (i2c_master_read(&i2c_msg, retry)) {
		LOG_ERR("Failed to read reg, bus: %d, addr: 0x%x, reg: 0x%x", bus, addr, offset);
		return false;
	}

	memcpy(data, i2c_msg.data, len);
	return true;
}

bool plat_i2c_write(uint8_t bus, uint8_t addr, uint8_t offset, uint8_t *data, uint8_t len)
{
	CHECK_NULL_ARG_WITH_RETURN(data, false);

	I2C_MSG i2c_msg = { 0 };
	uint8_t retry = 5;
	i2c_msg.bus = bus;
	i2c_msg.target_addr = addr;
	i2c_msg.tx_len = len + 1;
	i2c_msg.data[0] = offset;

	if (len > 0)
		memcpy(&i2c_msg.data[1], data, len);

	if (i2c_master_write(&i2c_msg, retry)) {
		LOG_ERR("Failed to write reg, bus: %d, addr: 0x%x, reg: 0x%x", bus, addr, offset);
		return false;
	}
	return true;
}

/* PU7/PU8/PU9 VR sit behind this I2C mux, other VR are connected directly */
#define VR_I2C_MUX_BUS I2C_BUS9
#define VR_I2C_MUX_ADDR 0x73
#define VR_I2C_MUX_CH_PU7 0x04 // channel 3
#define VR_I2C_MUX_CH_PU8 0x02 // channel 2
#define VR_I2C_MUX_CH_PU9 0x01 // channel 1

/* Return the mux channel of the sensor, 0 if the sensor is not behind the mux */
static uint8_t vr_mux_channel_get(uint8_t sensor_num)
{
	if (sensor_num >= SENSOR_NUM_ASIC_P0V75_ZORA11_VDDL_TEMP_C &&
	    sensor_num <= SENSOR_NUM_ASIC_P0V75_ZORA11_VDDH_PWR_W)
		return VR_I2C_MUX_CH_PU7;

	if (sensor_num >= SENSOR_NUM_ASIC_P0V75_ZORA10_VDDL_TEMP_C &&
	    sensor_num <= SENSOR_NUM_ASIC_P0V75_ZORA10_VDDH_PWR_W)
		return VR_I2C_MUX_CH_PU8;

	if (sensor_num >= SENSOR_NUM_ASIC_P0V75_ZORA01_VDDL_TEMP_C &&
	    sensor_num <= SENSOR_NUM_ASIC_P0V75_ZORA01_VDDH_PWR_W)
		return VR_I2C_MUX_CH_PU9;

	return 0;
}

/* The VRs behind the mux each have their own VR mutex, but they share the mux channel.
 * This mutex keeps the channel from being switched by another thread during a VR access. */
static K_MUTEX_DEFINE(vr_mux_mutex);

static bool vr_mux_write_channel(uint8_t channel)
{
	I2C_MSG i2c_msg = { 0 };
	uint8_t retry = 5;
	i2c_msg.bus = VR_I2C_MUX_BUS;
	i2c_msg.target_addr = VR_I2C_MUX_ADDR;
	i2c_msg.tx_len = 1;
	i2c_msg.data[0] = channel;

	return i2c_master_write(&i2c_msg, retry) == 0;
}

bool vr_mux_select(uint8_t sensor_num)
{
	uint8_t channel = vr_mux_channel_get(sensor_num);
	if (!channel)
		return true;

	if (k_mutex_lock(&vr_mux_mutex, K_MSEC(VR_MUTEX_LOCK_TIMEOUT_MS))) {
		LOG_ERR("Failed to lock vr mux, sensor: 0x%x", sensor_num);
		return false;
	}

	if (!vr_mux_write_channel(channel)) {
		LOG_ERR("Failed to switch mux, sensor: 0x%x, channel: 0x%x", sensor_num, channel);
		k_mutex_unlock(&vr_mux_mutex);
		return false;
	}
	return true;
}

/* Release the mux channel held by vr_mux_select(). Does nothing if not behind the mux. */
void vr_mux_release(uint8_t sensor_num)
{
	if (!vr_mux_channel_get(sensor_num))
		return;

	if (k_mutex_unlock(&vr_mux_mutex))
		LOG_ERR("Failed to release vr mux, sensor: 0x%x", sensor_num);
}
