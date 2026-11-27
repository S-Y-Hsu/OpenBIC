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

#include <zephyr.h>
#include <shell/shell.h>
#include <drivers/flash.h>
#include <stdlib.h>
#include <string.h>

#define SPIP_FLASH_DEV_NAME "spi_spip1_cs0"
#define SPIP_ERASE_UNIT 4096
#define SPIP_DEFAULT_READ_SZ 32
#define SPIP_MAX_READ_SZ 256
#define SPIP_MAX_WRITE_SZ 32

/* write_demo */
#define DEMO_ADDR 0x00000000U
#define DEMO_WRITE_SZ 16
#define DEMO_READ_SZ 64

static const char demo_str[] = "SPIP write test";

static uint8_t spip_buf[SPIP_MAX_READ_SZ];

static const struct device *get_spip_flash(const struct shell *sh)
{
	const struct device *dev = device_get_binding(SPIP_FLASH_DEV_NAME);

	if (!dev)
		shell_error(sh, "Cannot find device: %s", SPIP_FLASH_DEV_NAME);

	return dev;
}

static bool parse_ulong(const struct shell *sh, const char *str, const char *name,
			unsigned long *out)
{
	char *end;

	*out = strtoul(str, &end, 0);
	if (*str == '\0' || *end != '\0') {
		shell_error(sh, "Invalid %s: %s", name, str);
		return false;
	}

	return true;
}

static void print_hex_dump(const struct shell *sh, uint32_t base_addr, const uint8_t *buf,
			   size_t len)
{
	for (size_t i = 0; i < len; i += 16) {
		size_t row_len = MIN(16, len - i);

		shell_fprintf(sh, SHELL_NORMAL, "%08x:", base_addr + i);

		for (size_t j = 0; j < 16; j++) {
			if (j == 8)
				shell_fprintf(sh, SHELL_NORMAL, " ");
			if (j < row_len)
				shell_fprintf(sh, SHELL_NORMAL, " %02x", buf[i + j]);
			else
				shell_fprintf(sh, SHELL_NORMAL, "   ");
		}

		shell_fprintf(sh, SHELL_NORMAL, " |");
		for (size_t j = 0; j < 16; j++) {
			if (j == 8)
				shell_fprintf(sh, SHELL_NORMAL, " ");
			if (j < row_len) {
				uint8_t c = buf[i + j];
				shell_fprintf(sh, SHELL_NORMAL, "%c",
					      (c >= 0x20 && c < 0x7f) ? c : '.');
			} else {
				shell_fprintf(sh, SHELL_NORMAL, " ");
			}
		}
		shell_fprintf(sh, SHELL_NORMAL, "|\n");
	}
}

/* spip_flash read [addr] [len] */
static int cmd_read(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *flash_dev = get_spip_flash(sh);
	unsigned long addr = 0, len = SPIP_DEFAULT_READ_SZ;
	int ret;

	if (!flash_dev)
		return -ENODEV;

	if (argc > 1 && !parse_ulong(sh, argv[1], "addr", &addr))
		return -EINVAL;
	if (argc > 2 && !parse_ulong(sh, argv[2], "len", &len))
		return -EINVAL;

	if (len == 0 || len > SPIP_MAX_READ_SZ) {
		shell_error(sh, "len must be 1 ~ %d", SPIP_MAX_READ_SZ);
		return -EINVAL;
	}

	ret = flash_read(flash_dev, addr, spip_buf, len);
	if (ret) {
		shell_error(sh, "Read failed: %d", ret);
		return ret;
	}
	print_hex_dump(sh, addr, spip_buf, len);

	return 0;
}

/* spip_flash erase [addr] [size] */
static int cmd_erase(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *flash_dev = get_spip_flash(sh);
	unsigned long addr = 0, size = SPIP_ERASE_UNIT;
	int ret;

	if (!flash_dev)
		return -ENODEV;

	if (argc > 1 && !parse_ulong(sh, argv[1], "addr", &addr))
		return -EINVAL;
	if (argc > 2 && !parse_ulong(sh, argv[2], "size", &size))
		return -EINVAL;

	if ((addr % SPIP_ERASE_UNIT) || size == 0 || (size % SPIP_ERASE_UNIT)) {
		shell_error(sh, "addr/size must be aligned to %d bytes", SPIP_ERASE_UNIT);
		return -EINVAL;
	}

	shell_print(sh, "Erasing %lu bytes at addr 0x%08lx...", size, addr);
	ret = flash_erase(flash_dev, addr, size);
	if (ret) {
		shell_error(sh, "Erase failed: %d", ret);
		return ret;
	}
	shell_print(sh, "Erase done.");

	return 0;
}

/* spip_flash write <addr> <byte0> [byte1 ...]
 * Raw program only (no erase). NOR program can only clear bits (1 -> 0),
 * so erase the target sector first if it is not blank.
 */
static int cmd_write(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *flash_dev = get_spip_flash(sh);
	uint8_t write_buf[SPIP_MAX_WRITE_SZ];
	unsigned long addr, val;
	size_t len = argc - 2;
	int ret;

	if (!flash_dev)
		return -ENODEV;

	if (!parse_ulong(sh, argv[1], "addr", &addr))
		return -EINVAL;

	if (len > SPIP_MAX_WRITE_SZ) {
		shell_error(sh, "Too many bytes, max %d", SPIP_MAX_WRITE_SZ);
		return -EINVAL;
	}

	for (size_t i = 0; i < len; i++) {
		if (!parse_ulong(sh, argv[i + 2], "byte", &val))
			return -EINVAL;
		if (val > 0xff) {
			shell_error(sh, "Byte out of range: %s", argv[i + 2]);
			return -EINVAL;
		}
		write_buf[i] = (uint8_t)val;
	}

	shell_print(sh, "Writing %zu bytes to addr 0x%08lx...", len, addr);
	ret = flash_write(flash_dev, addr, write_buf, len);
	if (ret) {
		shell_error(sh, "Write failed: %d", ret);
		return ret;
	}

	ret = flash_read(flash_dev, addr, spip_buf, len);
	if (ret) {
		shell_error(sh, "Read back failed: %d", ret);
		return ret;
	}
	print_hex_dump(sh, addr, spip_buf, len);

	if (memcmp(write_buf, spip_buf, len)) {
		shell_warn(sh, "Verify mismatch (sector not erased?)");
		return -EIO;
	}
	shell_print(sh, "Write verify OK.");

	return 0;
}

/* spip_flash write_demo: erase 4KB @0x0, write test string, read back 64 bytes */
static int cmd_write_demo(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *flash_dev = get_spip_flash(sh);
	uint8_t write_buf[DEMO_WRITE_SZ];
	int ret;

	if (!flash_dev)
		return -ENODEV;

	shell_print(sh, "Erasing %d bytes at addr 0x%08x...", SPIP_ERASE_UNIT, DEMO_ADDR);
	ret = flash_erase(flash_dev, DEMO_ADDR, SPIP_ERASE_UNIT);
	if (ret) {
		shell_error(sh, "Erase failed: %d", ret);
		return ret;
	}

	memset(write_buf, 0xff, sizeof(write_buf));
	memcpy(write_buf, demo_str, strlen(demo_str));

	shell_print(sh, "Writing \"%s\" to addr 0x%08x...", demo_str, DEMO_ADDR);
	ret = flash_write(flash_dev, DEMO_ADDR, write_buf, sizeof(write_buf));
	if (ret) {
		shell_error(sh, "Write failed: %d", ret);
		return ret;
	}

	shell_print(sh, "Read after write:");
	ret = flash_read(flash_dev, DEMO_ADDR, spip_buf, DEMO_READ_SZ);
	if (ret) {
		shell_error(sh, "Read failed: %d", ret);
		return ret;
	}
	print_hex_dump(sh, DEMO_ADDR, spip_buf, DEMO_READ_SZ);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_spip_flash_cmds,
	SHELL_CMD_ARG(read, NULL, "[addr] [len]  Read flash (default addr 0x0, len 32, max 256)",
		      cmd_read, 1, 2),
	SHELL_CMD_ARG(erase, NULL,
		      "[addr] [size]  Erase flash, 4KB aligned (default addr 0x0, size 4KB)",
		      cmd_erase, 1, 2),
	SHELL_CMD_ARG(write, NULL,
		      "<addr> <byte0> [byte1 ...]  Program bytes (max 32, no auto erase) and verify",
		      cmd_write, 3, SPIP_MAX_WRITE_SZ - 1),
	SHELL_CMD_ARG(write_demo, NULL,
		      "Erase 4KB @0x0, write 'SPIP write test', read back 64 bytes",
		      cmd_write_demo, 1, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(crash_dump, &sub_spip_flash_cmds, "SPIP flash test commands", NULL);
