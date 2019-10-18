/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"


#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#endif

#include <linux/gpio.h>
#include <linux/of_gpio.h>



#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_info("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID_ (0xf5)

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;


#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
	lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE									0
#define FRAME_WIDTH										(480)
#define FRAME_HEIGHT									(960)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH									(56000)
#define LCM_PHYSICAL_HEIGHT									(112000)
#define LCM_DENSITY											(240)

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

static struct LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 60, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 60, {} },
};
/*
static struct LCM_setting_table lcm_suspend_deepslp_setting[] = {
	{REGFLAG_DELAY, 10, {} },
	{0x28, 0, {} },
	{REGFLAG_DELAY, 60, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 60, {} },
	{0x00, 1, { 0x00 } },
	{0xF7, 4, { 0x5A, 0xA5, 0x95, 0x27 } },
	{REGFLAG_DELAY, 20, {} }
};
*/
static struct LCM_setting_table init_setting[] = {
	{0x00, 1, {0x00} },
	{0xff, 3, {0x80, 0x19, 0x01} },
	{0x00, 1, {0x80} },
	{0xff, 2, {0x80, 0x19} },
	{0x00, 1, {0x90} },
	{0xb3, 1, {0x02} },
	{0x00, 1, {0x92} },
	{0xb3, 1, {0x40} },
	{0x00, 1, {0xa2} },
	{0xb3, 4, {0x01, 0xe0, 0x03, 0xc0} },
	{0x00, 1, {0xa7} },
	{0xb3, 1, {0x04} },
	{0x00, 1, {0x80} },
	{0xc0, 6, {0x00, 0x5f, 0x00, 0x1a, 0x1a, 0x00} },
	{0x00, 1, {0x90} },
	{0xc0, 6, {0x00, 0x15, 0x00, 0x00, 0x00, 0x03} },
	{0x00, 1, {0xa0} },
	{0xc1, 1, {0xe8} },
	{0x00, 1, {0xb4} },
	{0xc0, 1, {0x00} },
	{0x00, 1, {0x80} },
	{0xce, 6, {0x8b, 0x03, 0x00, 0x8a, 0x03, 0x00} },
	{0x00, 1, {0xa0} },
	{0xce, 14, {0x38, 0x05, 0x03, 0xc7,
				0x00, 0x22, 0x00, 0x38,
				0x04, 0x03, 0xc8, 0x00, 0x22, 0x00} },
	{0x00, 1, {0xb0} },
	{0xce, 14, {0x38, 0x03, 0x03, 0xc9,
				0x00, 0x22, 0x00, 0x38,
				0x02, 0x03, 0xcb, 0x00, 0x22, 0x00} },
	{0x00, 1, {0xc0} },
	{0xce, 14, {0x38, 0x09, 0x03, 0xc3,
				0x00, 0x22, 0x00, 0x38,
				0x08, 0x03, 0xc4, 0x00, 0x22, 0x00} },
	{0x00, 1, {0xd0} },
	{0xce, 14, {0x38, 0x07, 0x03, 0xc5,
				0x00, 0x22, 0x00, 0x38,
				0x06, 0x03, 0xc6, 0x00, 0x22, 0x00} },
	{0x00, 1, {0xe0} },
	{0xce, 8, {0x02, 0x02, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00} },

	{0x00, 1, {0xf0} },
	{0xce, 2, {0xa0, 0x00} },
	{0x00, 1, {0xc0} },
	{0xcf, 10, {0x09, 0x00, 0x01, 0x00, 0x00,
				0x00, 0x01, 0x00, 0x20, 0x00} },
	{0x00, 1, {0xc0} },
	{0xcb, 15, {0x15, 0x15, 0x15, 0x15,
				0x15, 0x15, 0x00, 0x00,
				0x15, 0x00, 0x15, 0x00,
				0x00, 0x00, 0x00} },
	{0x00, 1, {0xd0} },
	{0xcb, 15, {0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x15, 0x00,
				0x15, 0x00, 0x00} },
	{0x00, 1, {0xe0} },
	{0xcb, 10, {0x15, 0x15, 0x15, 0x15,
				0x15, 0x15, 0x00, 0x00, 0x00, 0x00} },

	{0x00, 1, {0xf0} },
	{0xcb, 10, {0xff, 0x0f, 0x33, 0x00,
				0x00, 0x00, 0xcc, 0xc0, 0xff, 0x00} },
	{0x00, 1, {0x80} },
	{0xcc, 10, {0x02, 0x0c, 0x0a, 0x10,
				0x0e, 0x00, 0x00, 0x00, 0x21, 0x00} },
	{0x00, 1, {0x90} },
	{0xcc, 15, {0x22, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00} },
	{0x00, 1, {0xa0} },
	{0xcc, 15, {0x22, 0x00, 0x21, 0x00,
				0x00, 0x00, 0x0d, 0x0f,
				0x09, 0x0b, 0x01, 0x00,
				0x00, 0x00, 0x00} },
	{0x00, 1, {0xb0} },
	{0xcc, 10, {0x00, 0x09, 0x0b, 0x0d,
				0x0f, 0x01, 0x00, 0x00, 0x21, 0x00} },
	{0x00, 1, {0xc0} },
	{0xcc, 15, {0x22, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00} },
	{0x00, 1, {0xd0} },
	{0xcc, 15, {0x22, 0x00, 0x21, 0x00,
				0x00, 0x02, 0x10, 0x0e,
				0x0c, 0x0a, 0x00, 0x00,
				0x00, 0x00, 0x00} },
	{0x00, 1, {0xd0} },
	{0xcf, 2, {0x00, 0x87} },

	{0x00, 1, {0x00} },
	{0xe1, 20, {0x12, 0x1C, 0x2a, 0x38,
				0x46, 0x66, 0x72, 0xa1,
				0x8b, 0x9d, 0x6d, 0x5e,
				0x79, 0x68, 0x6f, 0x69,
				0x62, 0x5d, 0x50, 0x2b} },
	{0x00, 1, {0x00} },
	{0xe2, 20, {0x12, 0x1C, 0x2a, 0x38,
				0x46, 0x66, 0x72, 0xa1,
				0x8b, 0x9d, 0x6d, 0x5e,
				0x79, 0x68, 0x6f, 0x69,
				0x62, 0x5d, 0x50, 0x2b} },
	{0x00, 1, {0x80} },
	{0xc1, 2, {0x03, 0x44} },

	{0x00, 1, {0x90} },
	{0xc5, 2, {0x8e, 0x77} },

	{0x00, 1, {0x82} },
	{0xc5, 2, {0xc5, 0xb0} },

	{0x00, 1, {0x00} },
	{0xd8, 2, {0x97, 0x97} },

	{0x00, 1, {0x00} },
	{0xd9, 1, {0x60} },
	{0x00, 1, {0xb1} },
	{0xc5, 1, {0xb9} },
	{0x00, 1, {0x92} },
	{0xc5, 1, {0x01} },
	{0x00, 1, {0x94} },
	{0xC5, 2, {0x66, 0x66} },

	{0x00, 1, {0x89} },
	{0xc4, 1, {0x08} },
	{0x00, 1, {0xa2} },
	{0xc0, 3, {0x00, 0x00, 0x00} },
	{0x00, 1, {0x90} },
	{0xc0, 6, {0x00, 0x13, 0x00, 0x08,
				0x00, 0x0c} },

	{0x00, 1, {0x80} },
	{0xc4, 1, {0x30} },

	{0x00, 1, {0x80} },
	{0xc1, 3, {0x03, 0x44, 0x84} },
	{0x00, 1, {0x98} },
	{0xc0, 1, {0x00} },
	{0x00, 1, {0xa9} },
	{0xc0, 1, {0x0a} },

	{0x00, 1, {0xb0} },
	{0xc1, 6, {0x20, 0x00, 0x00, 0x00,
			0x02, 0x01} },
	{0x00, 1, {0xe1} },
	{0xC0, 2, {0x40, 0x30} },

	{0x00, 1, {0xa0} },
	{0xc1, 1, {0xe8} },
	{0x00, 1, {0x90} },
	{0xb6, 2, {0xb4, 0x5a} },
	{0x00, 1, {0x83} },
	{0xc5, 1, {0x01} },
	{0x00, 1, {0xb0} },
	{0xca, 6, {0x0a, 0x20, 0x5f, 0x50} },
	{0x00, 1, {0xc4} },
	{0xc5, 2, {0x00, 0x00} },
	{0x00, 1, {0xba} },
	{0xf5, 2, {0x00, 0x00} },
	{0x00, 1, {0x00} },
	{0xfb, 1, {0x01} },
	{0x00, 1, {0x00} },
	{0xff, 3, {0xff, 0xff, 0xff} },
	{0x35, 1, {0x00} },
	{0x51, 1, {0xff} },
	{0x53, 1, {0x24} },
	{0x55, 1, {0x01} },
	{0x5e, 1, {0x03} },
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },

	{0x29, 0, {} },
	{REGFLAG_DELAY, 5, {} },

	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}


static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density            = LCM_DENSITY;

	params->dbi.te_mode				= LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
#else
	params->dsi.mode   = SYNC_PULSE_VDO_MODE;
#endif

	/* Command mode setting */
	params->dsi.LANE_NUM				= LCM_TWO_LANE;
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	params->dsi.packet_size = 256;

	params->dsi.intermediat_buffer_num = 2;

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.vertical_sync_active                = 4;
	params->dsi.vertical_backporch                  = 18;
	params->dsi.vertical_frontporch                 = 18;
	params->dsi.vertical_active_line                = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active              = 10;
	params->dsi.horizontal_backporch                = 60;
	params->dsi.horizontal_frontporch               = 60;
	params->dsi.horizontal_active_pixel             = FRAME_WIDTH;

	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
	params->dsi.lcm_backlight_curve_mode = 350;


	params->dsi.ssc_disable = 1;
	params->dsi.PLL_CLOCK = 226;
}

static void lcm_init_power(void)
{
}


static void lcm_suspend_power(void)
{
}

static void lcm_resume_power(void)
{

}

static void lcm_init(void)
{
	SET_RESET_PIN(0);
	MDELAY(15);
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(20);

	push_table(NULL, init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
	LCM_LOGI("-------lcm mode = vdo mode :%d----\n", lcm_dsi_mode);

}

static void lcm_suspend(void)
{
	LCM_LOGI("%s: kernel ----otm8019a_480p_dsi_vdo_ctc_lcetron----\n", __func__);
	push_table(NULL, lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(10);
}

static void lcm_resume(void)
{
	LCM_LOGI("%s: kernel----otm8019a_480p_dsi_vdo_ctc_lcetron----\n", __func__);
	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

static unsigned int lcm_compare_id(void)
{
	return 1;
}


/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x53, buffer, 1);

	if (buffer[0] != 0x24) {
		LCM_LOGI("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];

	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00043700;	/* read id return two byte,version and id */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	    && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s,backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
/* customization: 1. V2C config 2 values, C2V config 1 value; 2. config mode control register */
	if (mode == 0) {	/* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;	/* mode control addr */
		lcm_switch_mode_cmd.val[0] = 0x13;	/* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[1] = 0x10;	/* disable video mode secondly */
	} else {		/* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		lcm_switch_mode_cmd.val[0] = 0x03;	/* disable GRAM and enable video mode */
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}


struct LCM_DRIVER otm8019a_480p_dsi_vdo_ctc_lcetron_6771_lcm_drv = {
	.name = "otm8019a_480p_dsi_vdo_ctc_lcetron_6771",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.switch_mode = lcm_switch_mode,
};
