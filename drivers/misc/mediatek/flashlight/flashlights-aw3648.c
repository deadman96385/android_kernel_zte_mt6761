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

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#include "flashlight-core.h"
#include "flashlight-dt.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef AW3648_DTNAME
#define AW3648_DTNAME "mediatek,flashlights_aw3648"
#endif
#ifndef AW3648_DTNAME_I2C
#define AW3648_DTNAME_I2C "mediatek,flashlights_aw3648_i2c"
#endif

#define AW3648_NAME "flashlights-aw3648"

#define NOERR 0
#define ERR_RET 1

/* define registers */
#define AW3648_REG_SILICON_REVISION (0x0C)
#define AW3648_REG_ENABLE (0x01)
#define AW3648_REG_FLASH_CURRENT_CONTROL (0x03)
#define AW3648_REG_TORCH_CURRENT_CONTROL (0x05)
#define AW3648_REG_TIMING_CONFIGURATION (0x08)
/*enable bit code*/
#define AW3648_ENABLE_STANDBY (0x00)
#define AW3648_ENABLE_TORCH (0x0B)
#define AW3648_ENABLE_FLASH (0x0F)
#define AW3648_FLASH_TIMEOUT_MAX (0x1F)  /*max timeout is 1600ms*/
/* define level */
#define AW3648_LEVEL_NUM 32
#define AW3648_LEVEL_TORCH 7
#define AW3648_HW_TIMEOUT 1600 /* ms */
/* define pinctrl */
#define AW3648_PINCTRL_PIN_HWEN 0
#define AW3648_PINCTRL_PINSTATE_LOW 0
#define AW3648_PINCTRL_PINSTATE_HIGH 1
#define AW3648_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define AW3648_PINCTRL_STATE_HWEN_LOW  "hwen_low"
static struct pinctrl *aw3648_pinctrl;
static struct pinctrl_state *aw3648_hwen_high;
static struct pinctrl_state *aw3648_hwen_low;

/* define mutex and work queue */
static DEFINE_MUTEX(aw3648_mutex);
static struct work_struct aw3648_work;

/* aw3648 revision */
static int is_aw3648;

/* define usage count */
static int use_count;

/* define i2c */
static struct i2c_client *aw3648_i2c_client;

/* platform data */
struct aw3648_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

/* aw3648 chip data */
struct aw3648_chip_data {
	struct i2c_client *client;
	struct aw3648_platform_data *pdata;
	struct mutex lock;
};

/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int aw3648_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	aw3648_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(aw3648_pinctrl)) {
		pr_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(aw3648_pinctrl);
	}

	/* Flashlight HWEN pin initialization */
	aw3648_hwen_high = pinctrl_lookup_state(aw3648_pinctrl, AW3648_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(aw3648_hwen_high)) {
		pr_err("Failed to init (%s)\n", AW3648_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(aw3648_hwen_high);
	}
	aw3648_hwen_low = pinctrl_lookup_state(aw3648_pinctrl, AW3648_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(aw3648_hwen_low)) {
		pr_err("Failed to init (%s)\n", AW3648_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(aw3648_hwen_low);
	}

	return ret;
}

static int aw3648_pinctrl_set(int pin, int state)
{
	int ret = 0;

	if (IS_ERR(aw3648_pinctrl)) {
		pr_err("pinctrl is not available\n");
		return -ERR_RET;
	}

	switch (pin) {
	case AW3648_PINCTRL_PIN_HWEN:
		if (state == AW3648_PINCTRL_PINSTATE_LOW && !IS_ERR(aw3648_hwen_low))
			pinctrl_select_state(aw3648_pinctrl, aw3648_hwen_low);
		else if (state == AW3648_PINCTRL_PINSTATE_HIGH && !IS_ERR(aw3648_hwen_high))
			pinctrl_select_state(aw3648_pinctrl, aw3648_hwen_high);
		else
			pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	pr_err("pin(%d) state(%d)\n", pin, state);

	return ret;
}


/******************************************************************************
 * aw3648 operations
 *****************************************************************************/
static const int aw3648_current[AW3648_LEVEL_NUM] = {
	/*torch*/
	28, 52, 75, 98, 127, 152, 176,
	/*flash*/
	210, 234, 257, 281, 304, 328, 351, 374, 398, 445, 492, 539,
	609, 656, 703, 750, 797, 843, 890, 937, 1008, 1055, 1101, 1148, 1195
};

static const unsigned char aw3648_flash_level[AW3648_LEVEL_NUM] = {
	/*torch*/
	0x84, 0x88, 0x8c, 0x90, 0x95, 0x99, 0x9c,
	/*flash*/
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x92, 0x94, 0x96,
	0x99, 0x9b, 0x9d, 0x9f, 0xa1, 0xa3, 0xa5, 0xa7, 0xaa, 0xac, 0xae, 0xb0, 0xb2
};


static int aw3648_level = -1;

static int aw3648_is_torch(int level)
{
	if (level >= AW3648_LEVEL_TORCH)
		return -ERR_RET;

	return NOERR;
}

static int aw3648_verify_level(int level)
{
	if (level < 0) {
		level = 0;
	} else if (level >= AW3648_LEVEL_NUM) {
		level = AW3648_LEVEL_NUM - 1;
	}

	return level;
}

/* i2c wrapper function */
static int aw3648_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct aw3648_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

static int aw3648_read_reg(struct i2c_client *client, u8 reg)
{
	int val = 0;
	struct aw3648_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	return val;
}

/* flashlight enable function */
static int aw3648_enable(void)
{
	unsigned char reg, val;

	reg = AW3648_REG_ENABLE;
	if (!aw3648_is_torch(aw3648_level)) {
		/* torch mode */
		val = AW3648_ENABLE_TORCH;
	} else {
		/* flash mode */
		val = AW3648_ENABLE_FLASH;
	}
	return aw3648_write_reg(aw3648_i2c_client, reg, val);
}

/* flashlight disable function */
static int aw3648_disable(void)
{
	unsigned char reg, val;

	reg = AW3648_REG_ENABLE;
	val = AW3648_ENABLE_STANDBY;

	return aw3648_write_reg(aw3648_i2c_client, reg, val);
}

static int aw3648_set_timeout_configuration(void)
{
	return aw3648_write_reg(aw3648_i2c_client,
		AW3648_REG_TIMING_CONFIGURATION,
		AW3648_FLASH_TIMEOUT_MAX);
}


/* set flashlight level */
static int aw3648_set_level(int level)
{
	unsigned char reg, val;

	level = aw3648_verify_level(level);
	aw3648_level = level;
	if (!aw3648_is_torch(aw3648_level)) {
	       /* torch mode */
		 reg = AW3648_REG_TORCH_CURRENT_CONTROL;
		 val = aw3648_flash_level[level];
	} else {
		/* flash mode */
		reg = AW3648_REG_FLASH_CURRENT_CONTROL;
		val = aw3648_flash_level[level];
		aw3648_set_timeout_configuration();
	}

	return aw3648_write_reg(aw3648_i2c_client, reg, val);
}

/* flashlight init */
int aw3648_init(void)
{
	int ret;

	aw3648_pinctrl_set(AW3648_PINCTRL_PIN_HWEN, AW3648_PINCTRL_PINSTATE_HIGH);
	msleep(20);

	/* get silicon revision ,0x0a is AW3648*/
	is_aw3648 = aw3648_read_reg(aw3648_i2c_client, AW3648_REG_SILICON_REVISION);
	pr_err("AW3648 revision(0x%x).\n", is_aw3648);

	/* disable */
	ret = aw3648_write_reg(aw3648_i2c_client, AW3648_REG_ENABLE, AW3648_ENABLE_STANDBY);

	return ret;
}

/* flashlight uninit */
int aw3648_uninit(void)
{
	aw3648_disable();
	aw3648_pinctrl_set(AW3648_PINCTRL_PIN_HWEN, AW3648_PINCTRL_PINSTATE_LOW);

	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer aw3648_timer;
static unsigned int aw3648_timeout_ms;

static void aw3648_work_disable(struct work_struct *data)
{
	pr_err("work queue callback\n");
	aw3648_disable();
}

static enum hrtimer_restart aw3648_timer_func(struct hrtimer *timer)
{
	schedule_work(&aw3648_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int aw3648_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;
	unsigned int s;
	unsigned int ns;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_err("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		aw3648_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_err("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		aw3648_set_level(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_err("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (aw3648_timeout_ms) {
				s = aw3648_timeout_ms / 1000;
				ns = aw3648_timeout_ms % 1000 * 1000000;
				ktime = ktime_set(s, ns);
				hrtimer_start(&aw3648_timer, ktime, HRTIMER_MODE_REL);
			}
			aw3648_enable();
		} else {
			aw3648_disable();
			hrtimer_cancel(&aw3648_timer);
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_err("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = AW3648_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_err("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = AW3648_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = aw3648_verify_level(fl_arg->arg);
		pr_err("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = aw3648_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_err("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = AW3648_HW_TIMEOUT;
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int aw3648_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int aw3648_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int aw3648_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&aw3648_mutex);
	if (set) {
		if (!use_count)
			ret = aw3648_init();
		use_count++;
		pr_err("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = aw3648_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_err("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&aw3648_mutex);

	return ret;
}

static ssize_t aw3648_strobe_store(struct flashlight_arg arg)
{
	aw3648_set_driver(1);
	aw3648_set_level(arg.level);
	aw3648_timeout_ms = 0;
	aw3648_enable();
	msleep(arg.dur);
	aw3648_disable();
	aw3648_set_driver(0);

	return 0;
}

static struct flashlight_operations aw3648_ops = {
	aw3648_open,
	aw3648_release,
	aw3648_ioctl,
	aw3648_strobe_store,
	aw3648_set_driver
};


/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int aw3648_chip_init(struct aw3648_chip_data *chip)
{
	/* NOTE: Chip initialication move to "set driver" operation for power saving issue.
	 * aw3648_init();
	 */

	return 0;
}

static int aw3648_parse_dt(struct device *dev,
		struct aw3648_platform_data *pdata)
{
	struct device_node *np, *cnp;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		pr_info("Parse no dt, node.\n");
		return 0;
	}
	pr_info("Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		pr_info("Parse no dt, decouple.\n");

	pdata->dev_id = devm_kzalloc(dev,
			pdata->channel_num * sizeof(struct flashlight_device_id),
			GFP_KERNEL);
	if (!pdata->dev_id)
		return -ENOMEM;

	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type", &pdata->dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp, "ct", &pdata->dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp, "part", &pdata->dev_id[i].part))
			goto err_node_put;
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE, AW3648_NAME);
		pdata->dev_id[i].channel = i;
		pdata->dev_id[i].decouple = decouple;

		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				pdata->dev_id[i].type, pdata->dev_id[i].ct,
				pdata->dev_id[i].part, pdata->dev_id[i].name,
				pdata->dev_id[i].channel, pdata->dev_id[i].decouple);
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int aw3648_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct aw3648_chip_data *chip;
	int err;

	pr_err("i2c probe start.\n");

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	/* init chip private data */
	chip = kzalloc(sizeof(struct aw3648_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	i2c_set_clientdata(client, chip);
	aw3648_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init chip hw */
	aw3648_chip_init(chip);

	pr_err("i2c probe done.\n");

	return 0;

err_out:
	return err;
}

static int aw3648_i2c_remove(struct i2c_client *client)
{
	struct aw3648_chip_data *chip = i2c_get_clientdata(client);

	pr_err("Remove start.\n");

	client->dev.platform_data = NULL;

	/* free resource */
	kfree(chip);

	pr_err("Remove done.\n");

	return 0;
}

static const struct i2c_device_id aw3648_i2c_id[] = {
	{AW3648_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, aw3648_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id aw3648_i2c_of_match[] = {
	{.compatible = AW3648_DTNAME_I2C},
	{},
};
MODULE_DEVICE_TABLE(of, aw3648_i2c_of_match);
#endif

static struct i2c_driver aw3648_i2c_driver = {
	.driver = {
		.name = AW3648_NAME,
#ifdef CONFIG_OF
		.of_match_table = aw3648_i2c_of_match,
#endif
	},
	.probe = aw3648_i2c_probe,
	.remove = aw3648_i2c_remove,
	.id_table = aw3648_i2c_id,
};

/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int aw3648_probe(struct platform_device *pdev)
{
	struct aw3648_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct aw3648_chip_data *chip = NULL;
	int err;
	int i;

	pr_err("Probe start.\n");

	/* init pinctrl */
	if (aw3648_pinctrl_init(pdev)) {
		pr_err("Failed to init pinctrl.\n");
		return -ERR_RET;
	}

	if (i2c_add_driver(&aw3648_i2c_driver)) {
		pr_err("Failed to add i2c driver.\n");
		return -ERR_RET;
	}

	/* init platform data */
	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err_free;
		}
		pdev->dev.platform_data = pdata;
		err = aw3648_parse_dt(&pdev->dev, pdata);
		if (err)
			goto err_free;
	}

	/* init work queue */
	INIT_WORK(&aw3648_work, aw3648_work_disable);

	/* init timer */
	hrtimer_init(&aw3648_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw3648_timer.function = aw3648_timer_func;
	aw3648_timeout_ms = 1600;

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(&pdata->dev_id[i], &aw3648_ops)) {
				err = -EFAULT;
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(AW3648_NAME, &aw3648_ops)) {
			err = -EFAULT;
			goto err_free;
		}
	}

	pr_err("Probe done.\n");

	return 0;
err_free:
	chip = i2c_get_clientdata(aw3648_i2c_client);
	i2c_set_clientdata(aw3648_i2c_client, NULL);
	kfree(chip);
	return err;
}

static int aw3648_remove(struct platform_device *pdev)
{
	struct aw3648_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_err("Remove start.\n");

	i2c_del_driver(&aw3648_i2c_driver);

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(AW3648_NAME);

	/* flush work queue */
	flush_work(&aw3648_work);

	pr_err("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aw3648_of_match[] = {
	{.compatible = AW3648_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, aw3648_of_match);
#else
static struct platform_device aw3648_platform_device[] = {
	{
		.name = AW3648_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, aw3648_platform_device);
#endif

static struct platform_driver aw3648_platform_driver = {
	.probe = aw3648_probe,
	.remove = aw3648_remove,
	.driver = {
		.name = AW3648_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aw3648_of_match,
#endif
	},
};

static int __init flashlight_aw3648_init(void)
{
	int ret;

	pr_err("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&aw3648_platform_device);
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&aw3648_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	pr_err("Init done.\n");

	return 0;
}

static void __exit flashlight_aw3648_exit(void)
{
	pr_err("Exit start.\n");

	platform_driver_unregister(&aw3648_platform_driver);

	pr_err("Exit done.\n");
}

module_init(flashlight_aw3648_init);
module_exit(flashlight_aw3648_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xi Chen <xixi.chen@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight AW3648 Driver");

