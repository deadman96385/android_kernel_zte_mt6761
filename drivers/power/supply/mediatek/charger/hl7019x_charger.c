/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/types.h>
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/gpio/consumer.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
#include <mt-plat/mtk_boot.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/charger_type.h>
#include "hl7019x_charger.h"
#include "mtk_charger_intf.h"

#ifdef CONFIG_OF
#else

#define HL7019X_SLAVE_ADDR_WRITE   0xD6
#define HL7019X_SLAVE_ADDR_Read    0xD7

#ifdef I2C_SWITHING_CHARGER_CHANNEL
#define hl7019x_BUSNUM I2C_SWITHING_CHARGER_CHANNEL
#else
#define hl7019x_BUSNUM 0
#endif
#endif

struct hl7019x_info {
	struct charger_device *chg_dev;
	struct power_supply *psy;
	struct charger_properties chg_props;
	struct device *dev;
	const char *chg_dev_name;

	struct i2c_client *client;
	struct delayed_work otg_feed_watchdog_work;
};
static struct hl7019x_info *hl7019x_data;

static DEFINE_MUTEX(hl7019x_i2c_access);
static DEFINE_MUTEX(hl7019x_access_lock);
static int hl7019x_write_reg(u8 reg, u8 val)
{
	int ret = 0;

	mutex_lock(&hl7019x_i2c_access);
	dev_dbg(&hl7019x_data->client->dev, "writer reg=0x%x, val=0x%x\n",
			 reg, val);
	ret = i2c_smbus_write_byte_data(hl7019x_data->client, reg, val);
	if (ret < 0)
		dev_err(&hl7019x_data->client->dev, "reg=0x%x, error=%d\n", reg, ret);
	mutex_unlock(&hl7019x_i2c_access);

	return ret;
}

int hl7019x_read_reg(u8 reg, u8 *dest)
{
	int ret = 0;

	mutex_lock(&hl7019x_i2c_access);
	ret = i2c_smbus_read_byte_data(hl7019x_data->client, reg);
	if (ret < 0)
		dev_err(&hl7019x_data->client->dev, "reg=0x%x, ret=%d\n", reg, ret);

	*dest = ret & 0xff;
	mutex_unlock(&hl7019x_i2c_access);

	return ret;
}

static void hl7019x_set_value(u8 reg, u8 reg_bit,
			      u8 reg_shift, u8 val)
{
	u8 tmp = 0;
	int ret = 0;

	mutex_lock(&hl7019x_access_lock);
	ret = hl7019x_read_reg(reg, &tmp);
	if (ret < 0) {
		dev_err(&hl7019x_data->client->dev, "ignore I2C write process\n");
	} else {
		tmp = (tmp & (~reg_bit)) | (val << reg_shift);
		hl7019x_write_reg(reg, tmp);
	}
	mutex_unlock(&hl7019x_access_lock);
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
static int hl7019x_set_vindpm_voltage(struct charger_device *chg_dev, u32 vin);

static int hl7019x_get_enable_status(struct charger_device *chg_dev, bool *en)
{
	u8 reg_value = 0;

	hl7019x_read_reg(HL7019X_REG_01, &reg_value);
	*en = (reg_value & HL7019X_REG01_CHG_CONFIG_MASK) >> HL7019X_REG01_CHG_CONFIG_SHIFT;

	return 0;
}

static int hl7019x_enable_charging(struct charger_device *chg_dev, bool en)
{
	u8 reg_value = 0;
	if (en) {
		reg_value = HL7019X_REG01_CHG_ENABLE << HL7019X_REG01_OTG_CHG_CONFIG_SHIFT;
		reg_value |= (HL7019X_REG01_SYS_MINV_3P5V << HL7019X_REG01_SYS_MINV_SHIFT);
		hl7019x_write_reg(HL7019X_REG_01, reg_value);

	} else {
		reg_value = HL7019X_REG01_CHG_DISABLE << HL7019X_REG01_OTG_CHG_CONFIG_SHIFT;
		reg_value |= (HL7019X_REG01_SYS_MINV_3P5V << HL7019X_REG01_SYS_MINV_SHIFT);
		hl7019x_write_reg(HL7019X_REG_01, reg_value);
	}

	return 0;
}

static int hl7019x_get_chg_current(struct charger_device *chg_dev, u32 *cur)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = hl7019x_read_reg(HL7019X_REG_02, &reg_value);
	reg_value = (reg_value & HL7019X_REG02_ICHG_MASK) >> HL7019X_REG02_ICHG_SHIFT;
	*cur = (reg_value * HL7019X_REG02_ICHG_LSB + HL7019X_REG02_ICHG_BASE) * 1000;

	return ret;
}

static int hl7019x_set_chg_current(struct charger_device *chg_dev, u32 cur)
{
	u8 reg_value = 0;

	cur /= 1000;
	if (cur > 3008) {
		cur = 3008;
	} else if (cur <= 512) {
		cur = 512;
	}

	reg_value = (cur - HL7019X_REG02_ICHG_BASE) / HL7019X_REG02_ICHG_LSB;
	reg_value = reg_value << HL7019X_REG02_ICHG_SHIFT;
	hl7019x_write_reg(HL7019X_REG_02, reg_value);

	return 0;
}

static int hl7019x_get_input_current(struct charger_device *chg_dev, u32 *aicr)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = hl7019x_read_reg(HL7019X_REG_00, &reg_value);
	reg_value = (reg_value & HL7019X_REG00_IINLIM_MASK) >> HL7019X_REG00_IINLIM_SHIFT;
	if (reg_value == 0)
		*aicr = 100000;
	else if (reg_value == 1)
		*aicr = 150000;
	else if (reg_value == 2)
		*aicr = 500000;
	else if (reg_value == 3)
		*aicr = 900000;
	else if (reg_value == 4)
		*aicr = 1000000;
	else if (reg_value == 5)
		*aicr = 1500000;
	else if (reg_value == 6)
		*aicr = 2000000;
	else if (reg_value == 7)
		*aicr = 3000000;

	return ret;
}

static int hl7019x_set_input_current(struct charger_device *chg_dev, u32 limit)
{
	u8 reg_value = 0;
	bool en_highz = HL7019X_REG00_HIZ_DISABLE;

	limit /= 1000;
	if (limit == 0) {
		en_highz = HL7019X_REG00_HIZ_ENABLE;
		pr_info("hl7019x: enable highz!\n");
	}

	if (limit < 150)
		reg_value = 0x00;
	else if (limit < 500)
		reg_value = 0x01;
	else if (limit < 900)
		reg_value = 0x02;
	else if (limit < 1000)
		reg_value = 0x03;
	else if (limit < 1500)
		reg_value = 0x04;
	else if (limit < 2000)
		reg_value = 0x05;
	else if (limit < 3000)
		reg_value = 0x06;
	else if (limit < 3500)
		reg_value = 0x07;
	else
		reg_value = 0x02;

	reg_value |= (en_highz << HL7019X_REG00_ENHIZ_SHIFT);
	hl7019x_write_reg(HL7019X_REG_00, reg_value);
	hl7019x_set_vindpm_voltage(chg_dev, 4520000);

	return 0;
}

static int hl7019x_get_cv_voltage(struct charger_device *chg_dev, u32 *vol)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = hl7019x_read_reg(HL7019X_REG_04, &reg_value);
	reg_value = (reg_value & HL7019X_REG04_VREG_MASK) >> HL7019X_REG04_VREG_SHIFT;
	*vol = (reg_value * HL7019X_REG04_VREG_LSB + HL7019X_REG04_VREG_BASE) * 1000;

	return ret;
}

static int hl7019x_set_cv_voltage(struct charger_device *chg_dev, u32 vol)
{
	u8 reg_value = 0;

	vol /= 1000;
	if (vol < HL7019X_REG04_VREG_BASE)
		vol = HL7019X_REG04_VREG_BASE;
	else if (vol >HL7019X_REG04_VREG_MAX)
		vol = HL7019X_REG04_VREG_MAX;

	reg_value = (vol - HL7019X_REG04_VREG_BASE) / HL7019X_REG04_VREG_LSB;
	hl7019x_set_value(HL7019X_REG_04, HL7019X_REG04_VREG_MASK,
			 HL7019X_REG04_VREG_SHIFT, reg_value);

	return 0;
}

static int hl7019x_get_chg_state(struct charger_device *chg_dev, unsigned char *state)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = hl7019x_read_reg(HL7019X_REG_08, &reg_value);

	*state = (reg_value & HL7019X_REG08_CHRG_STAT_MASK) >> HL7019X_REG08_CHRG_STAT_SHIFT;

	return ret;
}

static int hl7019x_reset_watch_dog_timer(struct charger_device *chg_dev)
{
	hl7019x_set_value(HL7019X_REG_01, HL7019X_REG01_WDT_RESET_MASK,
			  HL7019X_REG01_WDT_RESET_SHIFT, HL7019X_REG01_WDT_RESET);

	return 0;
}

static int hl7019x_get_vindpm_voltage(struct charger_device *chg_dev, u32 *value)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = hl7019x_read_reg(HL7019X_REG_00, &reg_value);
	reg_value = (reg_value & HL7019X_REG00_VINDPM_MASK) >> HL7019X_REG00_VINDPM_SHIFT;

	if (reg_value == 0x4)
		*value = 4200000;
	else if (reg_value == 0x5)
		*value = 4290000;
	else if (reg_value == 0x6)
		*value = 4350000;
	else if (reg_value == 0x7)
		*value = 4440000;
	else if (reg_value == 0x8)
		*value = 4520000;
	else if (reg_value == 0x9)
		*value = 4610000;
	else if (reg_value == 0xa)
		*value = 4670000;
	else if (reg_value == 0xb)
		*value = 4760000;
	else
		*value = 4520000;

	return ret;
}

static int hl7019x_set_vindpm_voltage(struct charger_device *chg_dev, u32 vin)
{
	u8 reg_value = 0;

	vin /= 1000;
	if (vin < 4290)
		reg_value = 0x4;
	else if (vin < 4350)
		reg_value = 0x05;
	else if (vin < 4440)
		reg_value = 0x06;
	else if (vin < 4520)
		reg_value = 0x07;
	else if (vin < 4610)
		reg_value = 0x08;
	else if (vin < 4670)
		reg_value = 0x09;
	else if (vin < 4760)
		reg_value = 0x0a;
	else if (vin < 4840)
		reg_value = 0x0b;
	else
		reg_value = 0x08;

	hl7019x_set_value(HL7019X_REG_00, HL7019X_REG00_VINDPM_MASK,
			HL7019X_REG00_VINDPM_SHIFT, reg_value);

	return 0;
}

static int hl7019x_is_charging_done(struct charger_device *chg_dev, bool *is_done)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = hl7019x_read_reg(HL7019X_REG_08, &reg_value);
	reg_value = (reg_value & HL7019X_REG08_CHRG_STAT_MASK) >> HL7019X_REG08_CHRG_STAT_SHIFT;

	if (reg_value == HL7019X_REG08_CHRG_STAT_CHGDONE)
		*is_done = true;
	else
		*is_done = false;

	return ret;
}

static int hl7019x_enable_otg(struct charger_device *chg_dev, bool enable)
{
	if (enable) {
		pr_info("hl7019x: enable OTG function\n");
		hl7019x_set_value(HL7019X_REG_01, HL7019X_REG01_OTG_CHG_CONFIG_MASK,
				  HL7019X_REG01_OTG_CHG_CONFIG_SHIFT, HL7019X_REG01_OTG_ENABLE);
		schedule_delayed_work(&hl7019x_data->otg_feed_watchdog_work,
				msecs_to_jiffies(50));
	} else {
		pr_info("hl7019x: disable OTG function\n");
		hl7019x_set_value(HL7019X_REG_01, HL7019X_REG01_OTG_CHG_CONFIG_MASK,
				  HL7019X_REG01_OTG_CHG_CONFIG_SHIFT, HL7019X_REG01_OTG_DISABLE);
		cancel_delayed_work_sync(&hl7019x_data->otg_feed_watchdog_work);
	}
	return 0;
}

static int hl7019x_enable_safetytimer(struct charger_device *chg_dev, bool en)
{
	if (en)
		hl7019x_set_value(HL7019X_REG_05, HL7019X_REG05_EN_TIMER_MASK,
				HL7019X_REG05_EN_TIMER_SHIFT, HL7019X_REG05_CHG_TIMER_ENABLE);
	else
		hl7019x_set_value(HL7019X_REG_05, HL7019X_REG05_EN_TIMER_MASK,
				HL7019X_REG05_EN_TIMER_SHIFT, HL7019X_REG05_CHG_TIMER_DISABLE);
	return 0;
}

static int hl7019x_get_is_safetytimer_enable(struct charger_device *chg_dev, bool *en)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = hl7019x_read_reg(HL7019X_REG_05, &reg_value);
	*en = (reg_value & HL7019X_REG05_EN_TIMER_MASK) >> HL7019X_REG05_EN_TIMER_SHIFT;

	return ret;
}

static int hl7019x_set_ship_mode(struct charger_device *chg_dev, bool en)
{
	if (en == 0) {
		hl7019x_set_value(HL7019X_REG_05, HL7019X_REG05_WDT_MASK,
				HL7019X_REG05_WDT_SHIFT, HL7019X_REG05_WDT_DISABLE);

		hl7019x_set_value(HL7019X_REG_07, HL7019X_REG07_BATFET_DIS_MASK,
				HL7019X_REG07_BATFET_DIS_SHIFT, HL7019X_REG07_BATFET_OFF);
	}
	return 0;
}

static int hl7019x_get_ship_mode(struct charger_device *chg_dev, bool *en)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = hl7019x_read_reg(HL7019X_REG_07, &reg_value);
	reg_value = (reg_value & HL7019X_REG07_BATFET_DIS_MASK) >> HL7019X_REG07_BATFET_DIS_SHIFT;
	*en = !reg_value;

	return ret;
}

static int hl7019x_get_eoc_current(struct charger_device *chg_dev, u32 *iterm)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = hl7019x_read_reg(HL7019X_REG_03, &reg_value);
	reg_value = (reg_value & HL7019X_REG03_ITERM_MASK) >> HL7019X_REG03_ITERM_SHIFT;
	*iterm = (reg_value * HL7019X_REG03_ITERM_LSB + HL7019X_REG03_ITERM_BASE) * 1000;

	return ret;
}

static int hl7019x_set_eoc_current(struct charger_device *chg_dev, u32 iterm)
{
	u8 reg_value = 0;

	iterm /= 1000;
	if (iterm > 2048)
		reg_value = 0x0f;
	else if (iterm < 128)
		reg_value = 0x00;
	else if (iterm == 180)
		reg_value = 0x01;
	else
		reg_value = (iterm - HL7019X_REG03_ITERM_BASE) / HL7019X_REG03_ITERM_LSB;

	hl7019x_set_value(HL7019X_REG_03, HL7019X_REG03_ITERM_MASK,
			  HL7019X_REG03_ITERM_SHIFT, reg_value);
	return 0;
}

static int hl7019x_set_rechg_voltage(struct charger_device *chg_dev, u32 vol)
{
	u8 reg_value = 0;

	vol /= 1000;
	if (vol < 100)
		reg_value = HL7019X_REG04_VRECHG_100MV;
	else if (vol >= 300)
		reg_value = HL7019X_REG04_VRECHG_300MV;
	else
		reg_value = HL7019X_REG04_VRECHG_100MV;

	hl7019x_set_value(HL7019X_REG_04, HL7019X_REG04_VRECHG_MASK,
			  HL7019X_REG04_VRECHG_SHIFT, reg_value);

	return 0;
}

static ssize_t hl7019x_show_registers(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret ;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "hl7019x Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = hl7019x_read_reg(addr, &val);
		if (ret >= 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,"Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t hl7019x_store_registers(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0B) {
		hl7019x_write_reg((unsigned char)reg, (unsigned char)val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, hl7019x_show_registers, hl7019x_store_registers);

static struct attribute *hl7019x_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group hl7019x_attr_group = {
	.attrs = hl7019x_attributes,
};

static char *chg_state_name[] = {
	"Not Charging", "Pre Charging", "Fast Charging", "Charge Termination"
};
static int hl7019x_dump_register(struct charger_device *chg_dev)
{
	u32 iinlim = 0;
	u32 ichg = 0;
	u32 cv = 0;
	u32 iterm = 0;
	unsigned char chrg_state = 0;
	bool chr_en = 0;
	u32 vdpm = 0;

	hl7019x_get_input_current(chg_dev, &iinlim);
	hl7019x_get_chg_current(chg_dev, &ichg);
	hl7019x_get_cv_voltage(chg_dev, &cv);
	hl7019x_get_eoc_current(chg_dev, &iterm);
	hl7019x_get_chg_state(chg_dev, &chrg_state);
	hl7019x_get_enable_status(chg_dev, &chr_en);
	hl7019x_get_vindpm_voltage(chg_dev, &vdpm);
	pr_info("%s: Ilim=%umA, Ichg=%umA, Cv=%umV, Iterm=%umA, ChrStat=%s, CHGEN=%d, VDPM=%umV\n",
		__func__, iinlim/1000, ichg/1000, cv/1000, iterm/1000,
		chg_state_name[chrg_state], chr_en, vdpm/1000);

	return 0;
}

static unsigned int hl7019x_hw_init(struct charger_device *chg_dev)
{
	unsigned int status = 0;

	hl7019x_set_vindpm_voltage(chg_dev, 4520000);	/* VIN DPM check 4.6V */
	hl7019x_reset_watch_dog_timer(chg_dev);	/* Kick watchdog */
	hl7019x_set_eoc_current(chg_dev, 150000);	/* Termination current 150mA */
	hl7019x_set_cv_voltage(chg_dev, 4400000);	/* VREG 4.4V */
	hl7019x_set_input_current(chg_dev, 500000);
	hl7019x_set_chg_current(chg_dev, 500000);
	hl7019x_set_rechg_voltage(chg_dev, 100000);	/* VRECHG 0.1V (4.300V) */
	hl7019x_enable_safetytimer(chg_dev, true);	/* Enable charge timer */
	hl7019x_enable_charging(chg_dev, true);
	pr_info("%s: hw_init down!\n", __func__);
	return status;
}

static void hl7019x_otg_feed_work(struct work_struct *work)
{
	pr_info("hl7019x: feed watchdog for OTG function\n");
	hl7019x_reset_watch_dog_timer(hl7019x_data->chg_dev);
	schedule_delayed_work(&hl7019x_data->otg_feed_watchdog_work, HZ * 15);
}

static int hl7019x_parse_dt(struct hl7019x_info *info, struct device *dev)
{
	struct device_node *np = dev->of_node;

	pr_info("%s\n", __func__);

	if (!np) {
		pr_err("%s: no of node\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_string(np, "charger_name", &info->chg_dev_name) < 0) {
		info->chg_dev_name = "primary_chg";
		pr_warn("%s: no charger name\n", __func__);
	}

	info->chg_props.alias_name = "hl7019x";
	return 0;
}

static int hl7019x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	if (chg_dev == NULL)
		return -EINVAL;

	pr_info("%s: event = %d\n", __func__, event);
	switch (event) {
	case EVENT_EOC:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}

	return 0;
}

static struct charger_ops hl7019x_chg_ops = {
	/* Normal charging */
	.dump_registers = hl7019x_dump_register,

	.enable = hl7019x_enable_charging,
	.is_enabled = hl7019x_get_enable_status,

	.get_charging_current = hl7019x_get_chg_current,
	.set_charging_current = hl7019x_set_chg_current,

	.get_input_current = hl7019x_get_input_current,
	.set_input_current = hl7019x_set_input_current,

	.get_eoc_current= hl7019x_get_eoc_current,
	.set_eoc_current = hl7019x_set_eoc_current,

	.get_constant_voltage = hl7019x_get_cv_voltage,
	.set_constant_voltage = hl7019x_set_cv_voltage,

	.kick_wdt = hl7019x_reset_watch_dog_timer,
	.set_mivr = hl7019x_set_vindpm_voltage,
	.is_charging_done = hl7019x_is_charging_done,

	/* Safety timer */
	.enable_safety_timer = hl7019x_enable_safetytimer,
	.is_safety_timer_enabled = hl7019x_get_is_safetytimer_enable,

	/* Power path */
	/*.enable_powerpath = hl7019x_enable_power_path,
	.is_powerpath_enabled = hl7019x_get_is_power_path_enable,*/

	/* OTG */
	.enable_otg = hl7019x_enable_otg,

	.event = hl7019x_do_event,

	/* ship mode */
	.set_ship_mode = hl7019x_set_ship_mode,
	.get_ship_mode = hl7019x_get_ship_mode,
};

int hl7019x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct hl7019x_info *info = NULL;

	pr_info("[hl7019x_probe]\n");
	info = devm_kzalloc(&client->dev, sizeof(struct hl7019x_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	hl7019x_data = info;
	hl7019x_data->client = client;
	info->dev = &client->dev;
	i2c_set_clientdata(client, info);
	ret = hl7019x_parse_dt(info, &client->dev);
	if (ret < 0) {
		devm_kfree(info->dev, info);
		return ret;
	}

	/* Register charger device */
	info->chg_dev = charger_device_register(info->chg_dev_name,
		&client->dev, info, &hl7019x_chg_ops, &info->chg_props);
	if (IS_ERR_OR_NULL(info->chg_dev)) {
		pr_err("%s: register charger device failed\n", __func__);
		ret = PTR_ERR(info->chg_dev);
		devm_kfree(info->dev, info);
		return ret;
	}

	ret = sysfs_create_group(&info->dev->kobj, &hl7019x_attr_group);
	if (ret) {
		pr_err("%s: failed to register sysfs. err\n", __func__);
	}

	INIT_DELAYED_WORK(&hl7019x_data->otg_feed_watchdog_work, hl7019x_otg_feed_work);

	info->psy = power_supply_get_by_name("charger");
	if (!info->psy) {
		pr_err("%s: get power supply failed\n", __func__);
		devm_kfree(info->dev, info);
		return -EINVAL;
	}

	hl7019x_hw_init(info->chg_dev);
	hl7019x_dump_register(info->chg_dev);

	return 0;
}

int hl7019x_remove(struct i2c_client *client)
{
	struct hl7019x_info *info = i2c_get_clientdata(client);

	sysfs_remove_group(&info->dev->kobj, &hl7019x_attr_group);
	devm_kfree(info->dev, info);
	pr_info("%s\n", __func__);

	return 0;
}

static const struct i2c_device_id hl7019x_i2c_id[] = {
	{"hl7019x_chg", 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id hl7019x_of_match[] = {
	{.compatible = "ti,hl7019x_charger"},
	{},
};
#else
static struct i2c_board_info i2c_hl7019x __initdata = {
	I2C_BOARD_INFO("hl7019x", (HL7019X_SLAVE_ADDR_WRITE >> 1))
};
#endif

static struct i2c_driver hl7019x_driver = {
	.driver = {
		   .name = "hl7019x_chg",
#ifdef CONFIG_OF
		   .of_match_table = hl7019x_of_match,
#endif
		   },
	.probe = hl7019x_probe,
	.remove = hl7019x_remove,
	.id_table = hl7019x_i2c_id,
};

static int __init hl7019x_init(void)
{
	return i2c_add_driver(&hl7019x_driver);
}

static void __exit hl7019x_exit(void)
{
	i2c_del_driver(&hl7019x_driver);
}
module_init(hl7019x_init);
module_exit(hl7019x_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C hl7019x Driver");
MODULE_AUTHOR("will cai <will.cai@mediatek.com>");
