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
#include "bq2560x_charger.h"

#ifdef CONFIG_CHARGER_HL7019X
#include "hl7019x_charger.h"
#endif
#include "mtk_charger_intf.h"

#ifdef CONFIG_OF
#else

#define bq2560x_SLAVE_ADDR_WRITE   0xD6
#define bq2560x_SLAVE_ADDR_Read    0xD7

#ifdef I2C_SWITHING_CHARGER_CHANNEL
#define bq2560x_BUSNUM I2C_SWITHING_CHARGER_CHANNEL
#else
#define bq2560x_BUSNUM 0
#endif
#endif

struct bq2560x_info {
	struct charger_device *chg_dev;
	struct power_supply *psy;
	struct charger_properties chg_props;
	struct device *dev;
	const char *chg_dev_name;

	struct i2c_client *client;
	struct delayed_work otg_feed_watchdog_work;
};
static struct bq2560x_info *bq2560x_data;

static DEFINE_MUTEX(bq2560x_i2c_access);
static DEFINE_MUTEX(bq2560x_access_lock);
static int bq2560x_write_reg(u8 reg, u8 val)
{
	int ret = 0;

	mutex_lock(&bq2560x_i2c_access);
	dev_dbg(&bq2560x_data->client->dev, "writer reg=0x%x, val=0x%x\n",
			 reg, val);
	ret = i2c_smbus_write_byte_data(bq2560x_data->client, reg, val);
	if (ret < 0)
		dev_err(&bq2560x_data->client->dev, "reg=0x%x, error=%d\n", reg, ret);
	mutex_unlock(&bq2560x_i2c_access);

	return ret;
}

int bq2560x_read_reg(u8 reg, u8 *dest)
{
	int ret = 0;

	mutex_lock(&bq2560x_i2c_access);
	ret = i2c_smbus_read_byte_data(bq2560x_data->client, reg);
	if (ret < 0)
		dev_err(&bq2560x_data->client->dev, "reg=0x%x, ret=%d\n", reg, ret);

	*dest = ret & 0xff;
	mutex_unlock(&bq2560x_i2c_access);

	return ret;
}

static void bq2560x_set_value(u8 reg, u8 reg_bit,
			      u8 reg_shift, u8 val)
{
	u8 tmp = 0;
	int ret = 0;

	mutex_lock(&bq2560x_access_lock);
	ret = bq2560x_read_reg(reg, &tmp);
	if (ret < 0) {
		dev_err(&bq2560x_data->client->dev, "ignore I2C write process\n");
	} else {
		tmp = (tmp & (~reg_bit)) | (val << reg_shift);
		bq2560x_write_reg(reg, tmp);
	}
	mutex_unlock(&bq2560x_access_lock);
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
static int bq2560x_set_vindpm_voltage(struct charger_device *chg_dev, u32 vin);

static int bq2560x_get_enable_status(struct charger_device *chg_dev, bool *en)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = bq2560x_read_reg(BQ2560X_REG_01, &reg_value);
	*en = (reg_value & BQ2560X_REG01_CHG_CONFIG_MASK) >> BQ2560X_REG01_CHG_CONFIG_SHIFT;

	return ret;
}

static int bq2560x_enable_charging(struct charger_device *chg_dev, bool en)
{
	u8 reg_value = 0;
	if (en) {
		reg_value = BQ2560X_REG01_CHG_ENABLE << BQ2560X_REG01_OTG_CHG_CONFIG_SHIFT;
		reg_value |= (BQ2560X_REG01_SYS_MINV_3P5V << BQ2560X_REG01_SYS_MINV_SHIFT);
		bq2560x_write_reg(BQ2560X_REG_01, reg_value);
	} else {
		reg_value = BQ2560X_REG01_CHG_DISABLE << BQ2560X_REG01_OTG_CHG_CONFIG_SHIFT;
		reg_value |= (BQ2560X_REG01_SYS_MINV_3P5V << BQ2560X_REG01_SYS_MINV_SHIFT);
		bq2560x_write_reg(BQ2560X_REG_01, reg_value);
	}

	return 0;
}

static int bq2560x_get_chg_current(struct charger_device *chg_dev, u32 *cur)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = bq2560x_read_reg(BQ2560X_REG_02, &reg_value);
	reg_value = (reg_value & BQ2560X_REG02_ICHG_MASK) >> BQ2560X_REG02_ICHG_SHIFT;
	*cur = (reg_value * BQ2560X_REG02_ICHG_LSB + BQ2560X_REG02_ICHG_BASE) * 1000;

	return 0;
}

static int bq2560x_set_chg_current(struct charger_device *chg_dev, u32 cur)
{
	u8 reg_value = 0;

	pr_info("bq2560x: [REG] set fcc %d\n", cur);

	cur /= 1000;
	if (cur > 3000)
		reg_value = 0x32;
	else if (cur < 0)
		reg_value = 0x0;
	else
		reg_value = (cur - BQ2560X_REG02_ICHG_BASE) / BQ2560X_REG02_ICHG_LSB;
	bq2560x_set_value(BQ2560X_REG_02, BQ2560X_REG02_ICHG_MASK,
			  BQ2560X_REG02_ICHG_SHIFT, reg_value);

	return 0;
}

static int bq2560x_get_input_current(struct charger_device *chg_dev, u32 *aicr)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = bq2560x_read_reg(BQ2560X_REG_00, &reg_value);
	reg_value = (reg_value & BQ2560X_REG00_IINLIM_MASK) >> BQ2560X_REG00_IINLIM_SHIFT;
	*aicr = (reg_value * BQ2560X_REG00_IINLIM_LSB + BQ2560X_REG00_IINLIM_BASE) * 1000;

	return ret;
}

static int bq2560x_set_input_current(struct charger_device *chg_dev, u32 limit)
{
	u8 reg_value = 0;
	bool en_highz = BQ2560X_REG00_HIZ_DISABLE;

	pr_info("bq2560x: [REG] set icl %d\n", limit);

	limit /= 1000;
	if (limit == 0) {
		en_highz = BQ2560X_REG00_HIZ_ENABLE;
		pr_info("bq2560x: enable highz!\n");
	}

	if (limit > BQ2560X_REG00_IINLIM_MAX)
		reg_value = 0x1F;
	else if (limit < BQ2560X_REG00_IINLIM_BASE)
		reg_value = 0x0;
	else
		reg_value = (limit - BQ2560X_REG00_IINLIM_BASE) / BQ2560X_REG00_IINLIM_LSB;

	reg_value |= (en_highz << BQ2560X_REG00_ENHIZ_SHIFT);
	bq2560x_write_reg(BQ2560X_REG_00, reg_value);
	bq2560x_set_vindpm_voltage(chg_dev, 4500000);

	return 0;
}

static int bq2560x_get_cv_voltage(struct charger_device *chg_dev, u32 *vol)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = bq2560x_read_reg(BQ2560X_REG_04, &reg_value);
	reg_value = (reg_value & BQ2560X_REG04_VREG_MASK) >> BQ2560X_REG04_VREG_SHIFT;

	if (reg_value == 0x0F) {
		*vol = 4352000;
	} else if (reg_value == 0x18) {
		*vol = 4624000;
	} else {
		*vol = (reg_value * BQ2560X_REG04_VREG_LSB + BQ2560X_REG04_VREG_BASE) * 1000;
	}

	return ret;
}

static int bq2560x_set_cv_voltage(struct charger_device *chg_dev, u32 vol)
{
	u8 reg_value = 0;

	pr_info("bq2560x: [REG] set fcv %d\n", vol);

	vol /= 1000;
	if (vol < BQ2560X_REG04_VREG_BASE)
		vol = BQ2560X_REG04_VREG_BASE;
	else if (vol > BQ2560X_REG04_VREG_MAX)
		vol = BQ2560X_REG04_VREG_MAX;

	reg_value = (vol - BQ2560X_REG04_VREG_BASE) / BQ2560X_REG04_VREG_LSB;
	bq2560x_set_value(BQ2560X_REG_04, BQ2560X_REG04_VREG_MASK,
			  BQ2560X_REG04_VREG_SHIFT, reg_value);

	return 0;
}

static int bq2560x_get_chg_state(struct charger_device *chg_dev, unsigned char *state)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = bq2560x_read_reg(BQ2560X_REG_08, &reg_value);

	*state = (reg_value & BQ2560X_REG08_CHRG_STAT_MASK) >> BQ2560X_REG08_CHRG_STAT_SHIFT;

	return ret;
}

static int bq2560x_reset_watch_dog_timer(struct charger_device *chg_dev)
{
	bq2560x_set_value(BQ2560X_REG_01, BQ2560X_REG01_WDT_RESET_MASK,
			  BQ2560X_REG01_WDT_RESET_SHIFT, BQ2560X_REG01_WDT_RESET);

	return 0;
}

static int bq2560x_get_vindpm_voltage(struct charger_device *chg_dev, u32 *value)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = bq2560x_read_reg(BQ2560X_REG_06, &reg_value);
	reg_value = (reg_value & BQ2560X_REG06_VINDPM_MASK) >> BQ2560X_REG06_VINDPM_SHIFT;
	*value = (reg_value * BQ2560X_REG06_VINDPM_LSB + BQ2560X_REG06_VINDPM_BASE) * 1000;

	return ret;
}

static int bq2560x_set_vindpm_voltage(struct charger_device *chg_dev, u32 vin)
{
	u8 reg_value = 0;

	vin /= 1000;
	if (vin < 3900)
		reg_value = 0x0;
	else if (vin > 5400)
		reg_value = 0x0f;
	else
		reg_value = (vin - BQ2560X_REG06_VINDPM_BASE) / BQ2560X_REG06_VINDPM_LSB;

	bq2560x_set_value(BQ2560X_REG_06, BQ2560X_REG06_VINDPM_MASK,
			BQ2560X_REG06_VINDPM_SHIFT, reg_value);

	return 0;
}

static int bq2560x_is_charging_done(struct charger_device *chg_dev, bool *is_done)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = bq2560x_read_reg(BQ2560X_REG_08, &reg_value);
	reg_value = (reg_value & BQ2560X_REG08_CHRG_STAT_MASK) >> BQ2560X_REG08_CHRG_STAT_SHIFT;

	if (reg_value == BQ2560X_REG08_CHRG_STAT_CHGDONE)
		*is_done = true;
	else
		*is_done = false;

	return ret;
}

static int bq2560x_enable_otg(struct charger_device *chg_dev, bool enable)
{
	if (enable) {
		pr_info("bq2560x: enable OTG function\n");
		bq2560x_set_value(BQ2560X_REG_01, BQ2560X_REG01_OTG_CHG_CONFIG_MASK,
				  BQ2560X_REG01_OTG_CHG_CONFIG_SHIFT, BQ2560X_REG01_OTG_ENABLE);
		schedule_delayed_work(&bq2560x_data->otg_feed_watchdog_work,
				msecs_to_jiffies(50));
	} else {
		pr_info("bq2560x: disable OTG function\n");
		bq2560x_set_value(BQ2560X_REG_01, BQ2560X_REG01_OTG_CHG_CONFIG_MASK,
				  BQ2560X_REG01_OTG_CHG_CONFIG_SHIFT, BQ2560X_REG01_OTG_DISABLE);
		cancel_delayed_work_sync(&bq2560x_data->otg_feed_watchdog_work);
	}
	return 0;
}

static int bq2560x_enable_safetytimer(struct charger_device *chg_dev, bool en)
{
	if (en)
		bq2560x_set_value(BQ2560X_REG_05, BQ2560X_REG05_EN_TIMER_MASK,
				BQ2560X_REG05_EN_TIMER_SHIFT, BQ2560X_REG05_CHG_TIMER_ENABLE);
	else
		bq2560x_set_value(BQ2560X_REG_05, BQ2560X_REG05_EN_TIMER_MASK,
				BQ2560X_REG05_EN_TIMER_SHIFT, BQ2560X_REG05_CHG_TIMER_DISABLE);
	return 0;
}

static int bq2560x_get_is_safetytimer_enable(struct charger_device *chg_dev, bool *en)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = bq2560x_read_reg(BQ2560X_REG_05, &reg_value);
	*en = (reg_value & BQ2560X_REG05_EN_TIMER_MASK) >> BQ2560X_REG05_EN_TIMER_SHIFT;

	return ret;
}

static int bq2560x_set_ship_mode(struct charger_device *chg_dev, bool en)
{
	if (en == 0x0) {
		bq2560x_set_value(BQ2560X_REG_05, BQ2560X_REG05_WDT_MASK,
				BQ2560X_REG05_WDT_SHIFT, BQ2560X_REG05_WDT_DISABLE);

		bq2560x_set_value(BQ2560X_REG_07, BQ2560X_REG07_BATFET_DIS_MASK,
				BQ2560X_REG07_BATFET_DIS_SHIFT, BQ2560X_REG07_BATFET_OFF);
	}
	return 0;
}

static int bq2560x_get_ship_mode(struct charger_device *chg_dev, bool *en)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = bq2560x_read_reg(BQ2560X_REG_07, &reg_value);
	reg_value = (reg_value & BQ2560X_REG07_BATFET_DIS_MASK) >> BQ2560X_REG07_BATFET_DIS_SHIFT;
	*en = !reg_value;

	return ret;
}

static int bq2560x_get_eoc_current(struct charger_device *chg_dev, u32 *iterm)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = bq2560x_read_reg(BQ2560X_REG_03, &reg_value);
	reg_value = (reg_value & BQ2560X_REG03_ITERM_MASK) >> BQ2560X_REG03_ITERM_SHIFT;
	*iterm = (reg_value * BQ2560X_REG03_ITERM_LSB + BQ2560X_REG03_ITERM_BASE) * 1000;

	return ret;
}

static int bq2560x_set_eoc_current(struct charger_device *chg_dev, u32 iterm)
{
	u8 reg_value = 0;

	pr_info("bq2560x: [REG] set eoc %d\n", iterm);

	iterm /= 1000;
	if (iterm > 960)
		reg_value = 0x0f;
	else if (iterm < 60)
		reg_value = 0x00;
	else
		reg_value = (iterm - BQ2560X_REG03_ITERM_BASE) / BQ2560X_REG03_ITERM_LSB;

	bq2560x_set_value(BQ2560X_REG_03, BQ2560X_REG03_ITERM_MASK,
			  BQ2560X_REG03_ITERM_SHIFT, reg_value);
	return 0;
}

static int bq2560x_get_rechg_voltage(struct charger_device *chg_dev, u32 *uv)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = bq2560x_read_reg(BQ2560X_REG_04, &reg_value);
	reg_value = (reg_value & BQ2560X_REG04_VRECHG_MASK) >> BQ2560X_REG04_VRECHG_SHIFT;
	*uv = reg_value ? 200000 : 100000;

	return ret;
}


static int bq2560x_set_rechg_voltage(struct charger_device *chg_dev, u32 uv)
{
	u8 reg_value = 0;

	pr_info("bq2560x: [REG] set rechg_voltage %d\n", uv);

	uv /= 1000;
	if (uv < 100)
		reg_value = BQ2560X_REG04_VRECHG_100MV;
	else if (uv >= 200)
		reg_value = BQ2560X_REG04_VRECHG_200MV;
	else
		reg_value = BQ2560X_REG04_VRECHG_100MV;

	bq2560x_set_value(BQ2560X_REG_04, BQ2560X_REG04_VRECHG_MASK,
			  BQ2560X_REG04_VRECHG_SHIFT, reg_value);

	return 0;
}

static ssize_t bq2560x_show_registers(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret ;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "bq2560x Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = bq2560x_read_reg(addr, &val);
		if (ret >= 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,"Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t bq2560x_store_registers(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0B) {
		bq2560x_write_reg((unsigned char)reg, (unsigned char)val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, bq2560x_show_registers, bq2560x_store_registers);

static struct attribute *bq2560x_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group bq2560x_attr_group = {
	.attrs = bq2560x_attributes,
};

static char *chg_state_name[] = {
	"Not Charging", "Pre Charging", "Fast Charging", "Charge Termination"
};
static int bq2560x_dump_register(struct charger_device *chg_dev)
{
	u32 iinlim = 0;
	u32 ichg = 0;
	u32 cv = 0;
	u32 iterm = 0;
	unsigned char chrg_state = 0;
	bool chr_en = 0;
	u32 vdpm = 0, rech_voltage;

	bq2560x_get_input_current(chg_dev, &iinlim);
	bq2560x_get_chg_current(chg_dev, &ichg);
	bq2560x_get_cv_voltage(chg_dev, &cv);
	bq2560x_get_eoc_current(chg_dev, &iterm);
	bq2560x_get_chg_state(chg_dev, &chrg_state);
	bq2560x_get_enable_status(chg_dev, &chr_en);
	bq2560x_get_vindpm_voltage(chg_dev, &vdpm);
	bq2560x_get_rechg_voltage(chg_dev, &rech_voltage);
	pr_info("%s: Ilim=%umA, Ichg=%umA, Cv=%umV, Iterm=%umA, ChrStat=%s, CHGEN=%d, VDPM=%umV, RECH_VOLT=%umV\n",
		__func__, iinlim/1000, ichg/1000, cv/1000, iterm/1000,
		chg_state_name[chrg_state], chr_en, vdpm/1000, rech_voltage/1000);

	return 0;
}

static unsigned int bq2560x_hw_init(struct charger_device *chg_dev)
{
	unsigned int status = 0;

	bq2560x_set_vindpm_voltage(chg_dev, 4500000);	/* VIN DPM check 4.6V */
	bq2560x_reset_watch_dog_timer(chg_dev);	/* Kick watchdog */
	bq2560x_set_eoc_current(chg_dev, 150000);/* Termination current 150mA */
	bq2560x_set_cv_voltage(chg_dev, 4350000);	/* VREG 4.35V */
	bq2560x_set_input_current(chg_dev, 500000);
	bq2560x_set_chg_current(chg_dev, 500000);
	bq2560x_set_rechg_voltage(chg_dev, 100000);   /* VRECHG 0.1V (4.300V) */
	bq2560x_enable_safetytimer(chg_dev, true);	/* Enable charge timer */
	bq2560x_enable_charging(chg_dev, true);
	pr_info("%s: hw_init down!\n", __func__);
	return status;
}

static void bq2560x_otg_feed_work(struct work_struct *work)
{
	pr_info("bq2560x: feed watchdog for OTG function\n");
	bq2560x_reset_watch_dog_timer(bq2560x_data->chg_dev);
	schedule_delayed_work(&bq2560x_data->otg_feed_watchdog_work, HZ * 15);
}

static int bq2560x_parse_dt(struct bq2560x_info *info, struct device *dev)
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

	if (of_property_read_string(np, "alias_name", &(info->chg_props.alias_name)) < 0) {
		info->chg_props.alias_name = "bq2560x";
		pr_warn("%s: no alias name\n", __func__);
	}

	return 0;
}

static int bq2560x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
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

static struct charger_ops bq2560x_chg_ops = {
	/* Normal charging */
	.dump_registers = bq2560x_dump_register,

	.enable = bq2560x_enable_charging,
	.is_enabled = bq2560x_get_enable_status,

	.get_charging_current = bq2560x_get_chg_current,
	.set_charging_current = bq2560x_set_chg_current,

	.get_input_current = bq2560x_get_input_current,
	.set_input_current = bq2560x_set_input_current,

	.get_eoc_current= bq2560x_get_eoc_current,
	.set_eoc_current = bq2560x_set_eoc_current,

#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	.get_recharge_voltage = bq2560x_get_rechg_voltage,
	.set_recharge_voltage = bq2560x_set_rechg_voltage,
#endif

	.get_constant_voltage = bq2560x_get_cv_voltage,
	.set_constant_voltage = bq2560x_set_cv_voltage,

	.kick_wdt = bq2560x_reset_watch_dog_timer,
	.set_mivr = bq2560x_set_vindpm_voltage,
	.is_charging_done = bq2560x_is_charging_done,

	/* Safety timer */
	.enable_safety_timer = bq2560x_enable_safetytimer,
	.is_safety_timer_enabled = bq2560x_get_is_safetytimer_enable,

	/* Power path */
	/*.enable_powerpath = bq2560x_enable_power_path,
	.is_powerpath_enabled = bq2560x_get_is_power_path_enable,*/

	/* OTG */
	.enable_otg = bq2560x_enable_otg,

	.event = bq2560x_do_event,

	/* ship mode */
	.set_ship_mode = bq2560x_set_ship_mode,
	.get_ship_mode = bq2560x_get_ship_mode,
};

#ifdef CONFIG_CHARGER_HL7019X
static bool bq2560x_is_checked = true;
extern int hl7019x_probe(struct i2c_client *client, const struct i2c_device_id *id);
extern int hl7019x_remove(struct i2c_client *client);

bool bq2560x_check_if_bq25601(void)
{
	u8 reg_value = 0, pn = 0;

	bq2560x_read_reg(BQ2560X_REG_0B, &reg_value);
	pr_info("%s reg=0x%x, value=0x%x\n", __func__, BQ2560X_REG_0B, reg_value);

	pn = (reg_value & BQ2560X_REG0B_PN_MASK) >> BQ2560X_REG0B_PN_SHIFT;
	if (pn == BQ2560x_REG0B_PN_IS_BQ25601) {
		pr_info("is bq25601 pn=0x%x\n", pn);
		return true;
	}

	pr_info("not bq25601 pn=0x%x\n", pn);
	return false;
}

bool bq2560x_check_if_hl7019x(void)
{
	u8 reg_value = 0, pre_value = 0;
	u8 pn = 0, pn_extra = 0;

	bq2560x_read_reg(HL7019X_REG_0A, &pre_value);
	bq2560x_set_value(HL7019X_REG_0A, HL7019X_REG0A_PN_EXTRA_MASK,
			HL7019X_REG0A_PN_EXTRA_SHIFT, 0x03);

	bq2560x_read_reg(HL7019X_REG_0A, &reg_value);
	pr_info("%s reg=0x%x, value=0x%x\n", __func__, HL7019X_REG_0A, reg_value);

	pn = (reg_value & HL7019X_REG0A_PN_MASK) >> HL7019X_REG0A_PN_SHIFT;
	pn_extra = (reg_value & HL7019X_REG0A_PN_EXTRA_MASK) >> HL7019X_REG0A_PN_EXTRA_SHIFT;

	if (pn == HL7019X_REG0A_PN_IS_HL7019 && pn_extra == HL7019X_REG0A_PN_EXTRA_IS_HL7019) {
		pr_info("is hl7019, pn=0x%x, pn_extra=0x%x\n", pn, pn_extra);
		return true;
	}
	pr_info("not hl7019, pn=0x%x, pn_extra=0x%x\n", pn, pn_extra);
	bq2560x_write_reg(HL7019X_REG_0A, pre_value);

	return false;
}
#endif

static int bq2560x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct bq2560x_info *info = NULL;

	pr_info("[bq2560x_probe]\n");
	info = devm_kzalloc(&client->dev, sizeof(struct bq2560x_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	bq2560x_data = info;
	bq2560x_data->client = client;
	info->dev = &client->dev;
	i2c_set_clientdata(client, info);
	ret = bq2560x_parse_dt(info, &client->dev);
	if (ret < 0) {
		devm_kfree(info->dev, info);
		return ret;
	}

#ifdef CONFIG_CHARGER_HL7019X
	if (bq2560x_check_if_hl7019x() && !bq2560x_check_if_bq25601()) {
		pr_info("charger chip check done! not bq25601, it is hl7019!\n");
		bq2560x_is_checked = false;
		hl7019x_probe(client, id);
		return 0;
	}
#endif

	/* Register charger device */
	info->chg_dev = charger_device_register(info->chg_dev_name,
		&client->dev, info, &bq2560x_chg_ops, &info->chg_props);
	if (IS_ERR_OR_NULL(info->chg_dev)) {
		pr_err("%s: register charger device failed\n", __func__);
		ret = PTR_ERR(info->chg_dev);
		devm_kfree(info->dev, info);
		return ret;
	}

	ret = sysfs_create_group(&info->dev->kobj, &bq2560x_attr_group);
	if (ret) {
		pr_err("%s: failed to register sysfs. err\n", __func__);
	}

	INIT_DELAYED_WORK(&bq2560x_data->otg_feed_watchdog_work, bq2560x_otg_feed_work);

	info->psy = power_supply_get_by_name("charger");
	if (!info->psy) {
		pr_err("%s: get power supply failed\n", __func__);
		devm_kfree(info->dev, info);
		return -EINVAL;
	}
	bq2560x_hw_init(info->chg_dev);

	bq2560x_dump_register(info->chg_dev);

	return 0;
}

static int bq2560x_remove(struct i2c_client *client)
{
	struct bq2560x_info *info = i2c_get_clientdata(client);

#ifdef CONFIG_CHARGER_HL7019X
	if (bq2560x_is_checked == false) {
		hl7019x_remove(client);
		return 0;
	}
#endif

	sysfs_remove_group(&info->dev->kobj, &bq2560x_attr_group);
	devm_kfree(info->dev, info);
	pr_info("%s\n", __func__);

	return 0;
}


static const struct i2c_device_id bq2560x_i2c_id[] = {
	{"bq2560x_chg", 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id bq2560x_of_match[] = {
	{.compatible = "ti,bq2560x_charger"},
	{},
};
#else
static struct i2c_board_info i2c_bq2560x __initdata = {
	I2C_BOARD_INFO("bq2560x", (bq2560x_SLAVE_ADDR_WRITE >> 1))
};
#endif

static struct i2c_driver bq2560x_driver = {
	.driver = {
		   .name = "bq2560x_chg",
#ifdef CONFIG_OF
		   .of_match_table = bq2560x_of_match,
#endif
		   },
	.probe = bq2560x_probe,
	.remove = bq2560x_remove,
	.id_table = bq2560x_i2c_id,
};

static int __init bq2560x_init(void)
{
	return i2c_add_driver(&bq2560x_driver);
}

static void __exit bq2560x_exit(void)
{
	i2c_del_driver(&bq2560x_driver);
}
module_init(bq2560x_init);
module_exit(bq2560x_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq2560x Driver");
MODULE_AUTHOR("will cai <will.cai@mediatek.com>");
