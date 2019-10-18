/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * * VERSION		DATE			AUTHOR		Note
 *
 */

#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
/* #include <soc/sprd/regulator.h> */
#include <linux/input/mt.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/err.h>

#include <linux/proc_fs.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/unistd.h>
#include <asm/io.h>
#include <linux/pm_runtime.h>
#include <linux/types.h>

#if defined(CONFIG_ADF)
#include <linux/notifier.h>
#include <video/adf_notifier.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>

#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#include <linux/suspend.h>
#include <linux/irq.h>

#ifdef MMI_TP_VERSION
#include <linux/string.h>
#endif

#include "tlsc6x_main.h"
#include "tpd_sys.h"

#define	TOUCH_VIRTUAL_KEYS
#define	MULTI_PROTOCOL_TYPE_B	0
#define	TS_MAX_FINGER		2

#define MAX_CHIP_ID   (10)
#define TS_NAME		"tlsc6x_ts"
unsigned char tlsc6x_chip_name[MAX_CHIP_ID][20] = {
"null", "tlsc6206a", "0x6306", "tlsc6206", "tlsc6324", "tlsc6332", "tlsc6440", "tlsc6432", "tlsc6424", "tlsc6448"
};

int g_is_telink_comp = 0;

struct tlsc6x_data *g_tp_drvdata = NULL;
static struct i2c_client *this_client;

#ifdef CONFIG_PM_WAKELOCKS
struct wakeup_source tlsc6x_wakelock;
#else
struct wake_lock tlsc6x_wakelock;
#endif


DEFINE_MUTEX(i2c_rw_access);

#if defined(CONFIG_ADF)
static int tlsc6x_adf_suspend(void);
static int tlsc6x_adf_resume(void);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void tlsc6x_ts_suspend(struct early_suspend *handler);
static void tlsc6x_ts_resume(struct early_suspend *handler);
#endif

#ifdef TLSC_ESD_HELPER_EN
static int tpd_esd_flag = 0;
static struct hrtimer tpd_esd_kthread_timer;
static DECLARE_WAIT_QUEUE_HEAD(tpd_esd_waiter);
#endif

#ifdef TLSC_TPD_PROXIMITY
static int tlsc6x_prox_ctrl(int enable);
unsigned char tpd_prox_old_state = 0;
static int tpd_prox_active = 0;
static struct class *sprd_tpd_class;
static struct device *sprd_ps_cmd_dev;
#endif

#define TLSC_PINCTRL_INIT "pmx_ts_init"
#ifdef CONFIG_TS_TLSC6X_MTK_INTERFACE
#define TLSC_PINCTRL_RST_0_STATE "rst_output0"
#define TLSC_PINCTRL_RST_1_STATE "rst_output1"

static int tlsc_select_rst_output(struct tlsc6x_data *ts, int value);
#endif

#ifdef TOUCH_VIRTUAL_KEYS

static ssize_t virtual_keys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct tlsc6x_data *data = i2c_get_clientdata(this_client);
	struct tlsc6x_platform_data *pdata = data->platform_data;

	return snprintf(buf, PAGE_SIZE, "%s:%s:%d:%d:%d:%d:%s:%s:%d:%d:%d:%d:%s:%s:%d:%d:%d:%d\n",
			__stringify(EV_KEY), __stringify(KEY_APPSELECT),
			pdata->virtualkeys[0], pdata->virtualkeys[1], pdata->virtualkeys[2],
		       pdata->virtualkeys[3]
		       , __stringify(EV_KEY), __stringify(KEY_HOMEPAGE), pdata->virtualkeys[4], pdata->virtualkeys[5],
		       pdata->virtualkeys[6], pdata->virtualkeys[7]
		       , __stringify(EV_KEY), __stringify(KEY_BACK), pdata->virtualkeys[8], pdata->virtualkeys[9],
		       pdata->virtualkeys[10], pdata->virtualkeys[11]);
}

static struct kobj_attribute virtual_keys_attr = {
	.attr = {
		 .name = "virtualkeys.tlsc6x_touch",
		 .mode = 0444,
		 },
	.show = &virtual_keys_show,
};

static struct attribute *properties_attrs[] = {
	&virtual_keys_attr.attr,
	NULL
};

static struct attribute_group properties_attr_group = {
	.attrs = properties_attrs,
};

static void tlsc6x_virtual_keys_init(void)
{
	int ret = 0;
	struct kobject *properties_kobj;

	TLSC_FUNC_ENTER();

	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj) {
		ret = sysfs_create_group(properties_kobj, &properties_attr_group);
	}
	if (!properties_kobj || ret) {
		tlsc_err("failed to create board_properties\n");
	}
}

#endif

void tlsc_irq_disable(void)
{
	TLSC_FUNC_ENTER();
	if (!g_tp_drvdata->irq_disabled) {
		disable_irq_nosync(this_client->irq);
		g_tp_drvdata->irq_disabled = true;
	}
}

void tlsc_irq_enable(void)
{
	TLSC_FUNC_ENTER();
	if (g_tp_drvdata->irq_disabled) {
		enable_irq(this_client->irq);
		g_tp_drvdata->irq_disabled = false;
	}
}
/*
    iic access interface
*/
int tlsc6x_i2c_read_sub(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen)
{
	int ret = 0;

	if (client == NULL) {
		tlsc_err("[IIC][%s]i2c_client==NULL!\n", __func__);
		return -EINVAL;
	}

	if (readlen > 0) {
		if (writelen > 0) {
			struct i2c_msg msgs[] = {
				{
				 .addr = client->addr,
				 .flags = 0,
				 .len = writelen,
				 .buf = writebuf,
				 },
				{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 2);
			if (ret < 0) {
				tlsc_err("[IIC]: i2c_transfer(2) error, addr= 0x%x!!\n", writebuf[0]);
				tlsc_err("[IIC]: i2c_transfer(2) error, ret=%d, rlen=%d, wlen=%d!!\n", ret, readlen,
				       writelen);
			}
		} else {
			struct i2c_msg msgs[] = {
				{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 1);
			if (ret < 0) {
				tlsc_err("[IIC]: i2c_transfer(read) error, ret=%d, rlen=%d, wlen=%d!!", ret, readlen,
				       writelen);
			}
		}
	}

	return ret;
}

/* fail : <0 */
int tlsc6x_i2c_read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen)
{
	int retry = 0;
	int ret = 0;

	/* lock in this function so we can do direct mode iic transfer in debug fun */
	mutex_lock(&i2c_rw_access);
	for (retry = 0; retry < 3; retry ++) {
		ret = tlsc6x_i2c_read_sub(client, writebuf, writelen, readbuf, readlen);
		if (ret >= 0) {
			break;
		}
	}
	mutex_unlock(&i2c_rw_access);

	return ret;
}

/* fail : <0 */
int tlsc6x_i2c_write_sub(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret = 0;

	if (client == NULL) {
		tlsc_err("[IIC][%s]i2c_client==NULL!\n", __func__);
		return -EINVAL;
	}

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0) {
			tlsc_err("[IIC]: i2c_transfer(write) error, ret=%d!!\n", ret);
		}
	}

	return ret;

}

/* fail : <0 */
int tlsc6x_i2c_write(struct i2c_client *client, char *writebuf, int writelen)
{
	int retry = 0;
	int ret = 0;

	mutex_lock(&i2c_rw_access);
	for (retry = 0; retry < 3; retry ++) {
		ret = tlsc6x_i2c_write_sub(client, writebuf, writelen);
		if (ret >= 0) {
			break;
		}
	}
	mutex_unlock(&i2c_rw_access);

	return ret;

}

/* fail : <0 */
int tlsc6x_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue)
{
	unsigned char buf[2] = { 0 };

	buf[0] = regaddr;
	buf[1] = regvalue;

	return tlsc6x_i2c_write(client, buf, sizeof(buf));
}

/* fail : <0 */
int tlsc6x_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue)
{
	return tlsc6x_i2c_read(client, &regaddr, 1, regvalue, 1);
}

static void tlsc6x_clear_report_data(struct tlsc6x_data *drvdata)
{
	int i;

	for (i = 0; i < TS_MAX_FINGER; i++) {
#if MULTI_PROTOCOL_TYPE_B
		input_mt_slot(drvdata->input_dev, i);
		input_mt_report_slot_state(drvdata->input_dev, MT_TOOL_FINGER, false);
#endif
	}

	input_report_key(drvdata->input_dev, BTN_TOUCH, 0);
#if !MULTI_PROTOCOL_TYPE_B
	input_mt_sync(drvdata->input_dev);
#endif
	input_sync(drvdata->input_dev);
}

static int tlsc6x_update_data(void)
{
	struct tlsc6x_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
	u8 buf[20] = { 0 };
	int ret = -1;
	int i;
	u16 x, y;
	u8 ft_pressure, ft_size;
	static char finger_down[TS_MAX_FINGER] = {0};

	ret = tlsc6x_i2c_read(this_client, buf, 1, buf, 18);
	if (ret < 0) {
		tlsc_err("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		return ret;
	}

	memset(event, 0, sizeof(struct ts_event));
	event->touch_point = buf[2] & 0x07;

#ifdef TLSC_TPD_PROXIMITY
	if (tpd_prox_active && (event->touch_point == 0)) {
		if (((buf[1] == 0xc0) || (buf[1] == 0xe0)) && (tpd_prox_old_state != buf[1])) {
			input_report_abs(data->ps_input_dev, ABS_DISTANCE, (buf[1] == 0xc0) ? 0 : 1);
			input_mt_sync(data->ps_input_dev);
			input_sync(data->ps_input_dev);
			tlsc_info("%s proximity report is %d.\n", __func__, (buf[1] == 0xc0) ? 0 : 1);
		}
		tpd_prox_old_state = buf[1];
	}
#endif

	for (i = 0; i < TS_MAX_FINGER; i++) {
		if ((buf[6 * i + 3] & 0xc0) == 0xc0) {
			continue;
		}
		x = (s16) (buf[6 * i + 3] & 0x0F) << 8 | (s16) buf[6 * i + 4];
		y = (s16) (buf[6 * i + 5] & 0x0F) << 8 | (s16) buf[6 * i + 6];
		ft_pressure = buf[6 * i + 7];
		if (ft_pressure > 127) {
			ft_pressure = 127;
		}
		ft_size = (buf[6 * i + 8] >> 4) & 0x0F;
		if ((buf[6 * i + 3] & 0x40) == 0x0) {
			if (!finger_down[i]) {
				finger_down[i] = 1;
				tlsc_info("touch down id: %d, coord [%d:%d]\n", buf[6 * i + 5] >> 4, x, y);
			}
#if MULTI_PROTOCOL_TYPE_B
			input_mt_slot(data->input_dev, buf[6 * i + 5] >> 4);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
#else
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, buf[6 * i + 5] >> 4);
#endif
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, y);
#ifdef PRESSURE_REPORT
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, 15);
#endif
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, ft_size);
			input_report_key(data->input_dev, BTN_TOUCH, 1);
#if !MULTI_PROTOCOL_TYPE_B
			input_mt_sync(data->input_dev);
#endif
		} else {
			if (finger_down[i] == 1) {
				finger_down[i] = 0;
				tlsc_info("touch up id: %d, coord [%d:%d]\n", buf[6 * i + 5] >> 4, x, y);
			}

#if MULTI_PROTOCOL_TYPE_B
			input_mt_slot(data->input_dev, buf[6 * i + 5] >> 4);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
#endif
		}
	}
	if (event->touch_point == 0) {
		tlsc6x_clear_report_data(data);
	}
	input_sync(data->input_dev);

	return 0;

}

static irqreturn_t touch_event_thread_handler(int irq, void *devid)
{

	tlsc6x_update_data();

	return IRQ_HANDLED;
}

void tlsc6x_tpd_reset_force(void)
{
#ifndef CONFIG_TS_TLSC6X_MTK_INTERFACE
	struct tlsc6x_platform_data *pdata = g_tp_drvdata->platform_data;
#endif

	TLSC_FUNC_ENTER();

#ifdef CONFIG_TS_TLSC6X_MTK_INTERFACE
	tlsc_select_rst_output(g_tp_drvdata, 1);
#else
	gpio_direction_output(pdata->reset_gpio_number, 1);
#endif
	usleep_range(10000, 11000);

#ifdef CONFIG_TS_TLSC6X_MTK_INTERFACE
	tlsc_select_rst_output(g_tp_drvdata, 0);
#else
	gpio_set_value(pdata->reset_gpio_number, 0);
#endif

	msleep(20);

#ifdef CONFIG_TS_TLSC6X_MTK_INTERFACE
	tlsc_select_rst_output(g_tp_drvdata, 1);
#else
	gpio_set_value(pdata->reset_gpio_number, 1);
#endif
	msleep(30);
}

static void tlsc6x_tpd_reset(void)
{
	TLSC_FUNC_ENTER();
	if (g_tp_drvdata->needKeepRamCode) {
		return;
	}

	tlsc6x_tpd_reset_force();
}

unsigned char real_suspend_flag = 0;

int tlsc6x_do_suspend(void)
{
	int ret = -1;

	TLSC_FUNC_ENTER();
	if (g_tp_drvdata->suspended) {
		tlsc_info("Already in suspend state.\n");
		return 0;
	}

#ifdef TLSC_ESD_HELPER_EN
	hrtimer_cancel(&tpd_esd_kthread_timer);
#endif

#ifdef TLSC_TPD_PROXIMITY
	if (tpd_prox_active) {
		real_suspend_flag = 0;
		enable_irq_wake(this_client->irq);
		g_tp_drvdata->suspended = true;
		return 0;
	}
#endif

	tlsc_irq_disable();
	ret = tlsc6x_write_reg(this_client, 0xa5, 0x03);
	if (ret < 0) {
		tlsc_err("tlsc6x error::setup suspend fail!\n");
	}
	real_suspend_flag = 1;
	tlsc6x_clear_report_data(g_tp_drvdata);
	g_tp_drvdata->suspended = true;
	return 0;
}

int tlsc6x_do_resume(void)
{
	TLSC_FUNC_ENTER();
#ifdef TLSC_ESD_HELPER_EN
	hrtimer_start(&tpd_esd_kthread_timer, ktime_set(3, 0), HRTIMER_MODE_REL);
#endif

	queue_work(g_tp_drvdata->tp_resume_workqueue, &g_tp_drvdata->resume_work);
	return 0;
}

#if defined(CONFIG_ADF)
static int tlsc6x_adf_suspend(void)
{
	TLSC_FUNC_ENTER();

	return tlsc6x_do_suspend();
}

static int tlsc6x_adf_resume(void)
{
	TLSC_FUNC_ENTER();

	return tlsc6x_do_resume();
}
#elif defined(CONFIG_FB)
static void tlsc6x_ts_suspend(void)
{
	TLSC_FUNC_ENTER();

	tlsc6x_do_suspend();
}

static void tlsc6x_ts_resume(void)
{
	TLSC_FUNC_ENTER();

	tlsc6x_do_resume();
}

#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void tlsc6x_ts_suspend(struct early_suspend *handler)
{
	TLSC_FUNC_ENTER();

	tlsc6x_do_suspend();
}

static void tlsc6x_ts_resume(struct early_suspend *handler)
{
	TLSC_FUNC_ENTER();

	tlsc6x_do_resume();
}
#endif

static void tlsc6x_resume_work(struct work_struct *work)
{
	TLSC_FUNC_ENTER();

	if (g_tp_drvdata->suspended == false) {
		tlsc_info("Already in awake state");
		return;
	}
	tlsc6x_tpd_reset();

	if (g_tp_drvdata->needKeepRamCode) {	/* need wakeup cmd in this mode */
		tlsc6x_write_reg(this_client, 0xa5, 0x00);
	}
	tlsc6x_clear_report_data(g_tp_drvdata);
#ifdef TLSC_TPD_PROXIMITY
	tlsc6x_prox_ctrl(tpd_prox_active);
	if (tpd_prox_active && (real_suspend_flag == 0)) {
		disable_irq_wake(this_client->irq);
		g_tp_drvdata->suspended = false;
		return;
	}
#endif

	tlsc_irq_enable();

	real_suspend_flag = 0;
	g_tp_drvdata->suspended = false;
}

#if defined(CONFIG_ADF)
/*
 * touchscreen's suspend and resume state should rely on screen state,
 * as fb_notifier and early_suspend are all disabled on our platform,
 * we can only use adf_event now
 */
static int ts_adf_event_handler(struct notifier_block *nb, unsigned long action, void *data)
{

	struct adf_notifier_event *event = data;
	int adf_event_data;

	if (action != ADF_EVENT_BLANK) {
		return NOTIFY_DONE;
	}
	adf_event_data = *(int *)event->data;
	tlsc_info("receive adf event with adf_event_data=%d", adf_event_data);

	switch (adf_event_data) {
	case DRM_MODE_DPMS_ON:
		tlsc6x_adf_resume();
		break;
	case DRM_MODE_DPMS_OFF:
		tlsc6x_adf_suspend();
		break;
	default:
		tlsc_info("receive adf event with error data, adf_event_data=%d", adf_event_data);
		break;
	}

	return NOTIFY_OK;
}

#elif defined(CONFIG_FB)
/*****************************************************************************
*  Name: fb_notifier_callback
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct tlsc6x_data *tlsc6x_data = container_of(self, struct tlsc6x_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && tlsc6x_data && tlsc6x_data->client) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			tlsc_info("resume notifier.\n");
			tlsc6x_ts_resume();
		} else if (*blank == FB_BLANK_POWERDOWN) {
			tlsc_info("suspend notifier.\n");
			tlsc6x_ts_suspend();
		}
	}

	return 0;
}

#endif

static int tlsc6x_hw_init(struct tlsc6x_data *drvdata)
{
	struct regulator *reg_vdd;
	struct i2c_client *client = drvdata->client;
	struct tlsc6x_platform_data *pdata = drvdata->platform_data;

	TLSC_FUNC_ENTER();
	if (gpio_request(pdata->irq_gpio_number, NULL) < 0) {
		goto OUT;
	}

#ifndef CONFIG_TS_TLSC6X_MTK_INTERFACE
	if (gpio_request(pdata->reset_gpio_number, NULL) < 0) {
		goto OUT;
	}
#endif

#ifdef CONFIG_TS_TLSC6X_MTK_INTERFACE
	tlsc_select_rst_output(g_tp_drvdata, 1);
#else
	gpio_direction_output(pdata->reset_gpio_number, 1);
#endif
	gpio_direction_input(pdata->irq_gpio_number);

	reg_vdd = regulator_get(&client->dev, pdata->vdd_name);
	if (!WARN(IS_ERR(reg_vdd), "tlsc6x_hw_init regulator: failed to get %s.\n", pdata->vdd_name)) {
		regulator_set_voltage(reg_vdd, 2800000, 2800000);
		if (regulator_enable(reg_vdd)) {
			tlsc_info("tlsc6x_hw_init:regulator_enable return none zero\n");
		}
		if (regulator_is_enabled(reg_vdd) == 0) {
			tlsc_err("tlsc6x_hw_init:regulator_enable fail\n");
		}
		drvdata->reg_vdd = reg_vdd;
	} else {
		drvdata->reg_vdd = NULL;
	}

	tlsc6x_tpd_reset();
	return 0;
OUT:
	return -EPERM;
}

static int tlsc6x_pinctrl_init(struct tlsc6x_data *ts)
{
	int ret = 0;
	struct i2c_client *client = ts->client;

	ts->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(ts->pinctrl)) {
		tlsc_err("Failed to get pinctrl, please check dts");
		ret = PTR_ERR(ts->pinctrl);
		goto err_pinctrl_get;
	}

	ts->pins_init = pinctrl_lookup_state(ts->pinctrl, TLSC_PINCTRL_INIT);
	if (IS_ERR_OR_NULL(ts->pins_init)) {
		tlsc_err("Pin state[init] not found %d", IS_ERR_OR_NULL(ts->pins_init));
		ret = PTR_ERR(ts->pins_init);
		goto err_pinctrl_lookup;
	}

#ifdef CONFIG_TS_TLSC6X_MTK_INTERFACE
	ts->pinctrl_rst_output0
		= pinctrl_lookup_state(ts->pinctrl, TLSC_PINCTRL_RST_0_STATE);
	if (IS_ERR_OR_NULL(ts->pinctrl_rst_output0)) {
		ret = PTR_ERR(ts->pinctrl_rst_output0);
		tlsc_err("Can not lookup %s pinstate %d\n", TLSC_PINCTRL_RST_0_STATE, ret);
		goto err_pinctrl_lookup;
	}

	ts->pinctrl_rst_output1
		= pinctrl_lookup_state(ts->pinctrl, TLSC_PINCTRL_RST_1_STATE);
	if (IS_ERR_OR_NULL(ts->pinctrl_rst_output1)) {
		ret = PTR_ERR(ts->pinctrl_rst_output1);
		tlsc_err("Can not lookup %s pinstate %d\n", TLSC_PINCTRL_RST_1_STATE, ret);
		goto err_pinctrl_lookup;
	}
#endif

	return 0;
err_pinctrl_lookup:
	if (ts->pinctrl) {
		devm_pinctrl_put(ts->pinctrl);
	}
err_pinctrl_get:
	ts->pinctrl = NULL;
	ts->pins_init = NULL;
#ifdef CONFIG_TS_TLSC6X_MTK_INTERFACE
	ts->pinctrl_rst_output0 = NULL;
	ts->pinctrl_rst_output1 = NULL;
#endif

	return ret;
}

static int tlsc_pinctrl_select_init(struct tlsc6x_data *ts)
{
	int ret = 0;

	if (ts->pinctrl && ts->pins_init) {
		ret = pinctrl_select_state(ts->pinctrl, ts->pins_init);
		if (ret < 0) {
			tlsc_err("Set pin init state error:%d", ret);
		} else {
			tlsc_info("Set pin init state success");
		}
	}

	return ret;
}


#ifdef CONFIG_TS_TLSC6X_MTK_INTERFACE
static int tlsc_select_rst_output(struct tlsc6x_data *ts, int value)
{
	int ret = 0;

	if (ts->pinctrl == NULL) {
		tlsc_err("%s failed: pinctrl is null", __func__);
		return ret;
	}

	if ((value == 0) && ts->pinctrl_rst_output0) {
		ret = pinctrl_select_state(ts->pinctrl, ts->pinctrl_rst_output0);
		if (ret < 0) {
			tlsc_err("Set pin rst state 0 error:%d", ret);
		} else {
			tlsc_info("Set pin rst state 0 success");
		}
	} else if ((value == 1) && ts->pinctrl_rst_output1) {
		ret = pinctrl_select_state(ts->pinctrl, ts->pinctrl_rst_output1);
		if (ret < 0) {
			tlsc_err("Set pin rst state 1 error:%d", ret);
		} else {
			tlsc_info("Set pin rst state 1 success");
		}
	}

	return ret;
}
#endif


#ifdef CONFIG_OF
static struct tlsc6x_platform_data *tlsc6x_parse_dt(struct device *dev)
{
	int ret;
	struct tlsc6x_platform_data *pdata;
	struct device_node *np = dev->of_node;

	TLSC_FUNC_ENTER();
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		tlsc_err("Could not allocate struct tlsc6x_platform_data.\n");
		return NULL;
	}

	pdata->reset_gpio_number = of_get_gpio(np, 0);
	if (pdata->reset_gpio_number < 0) {
		tlsc_err("fail to get reset_gpio_number\n");
		goto fail;
	}

	pdata->irq_gpio_number = of_get_gpio(np, 1);
	if (pdata->irq_gpio_number < 0) {
		tlsc_err("fail to get irq_gpio_number\n");
		goto fail;
	}

	ret = of_property_read_string(np, "vdd_name", &pdata->vdd_name);
	if (ret) {
		tlsc_err("fail to get vdd_name\n");
		goto fail;
	}
	#ifdef TOUCH_VIRTUAL_KEYS
	ret = of_property_read_u32_array(np, "virtualkeys", pdata->virtualkeys, 12);
	if (ret) {
		tlsc_err("fail to get virtualkeys\n");
		/* goto fail; */
	}
	#endif
	ret = of_property_read_u32(np, "TP_MAX_X", &pdata->x_res_max);
	if (ret) {
		tlsc_err("fail to get TP_MAX_X\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "TP_MAX_Y", &pdata->y_res_max);
	if (ret) {
		tlsc_err("fail to get TP_MAX_Y\n");
		goto fail;
	}

	return pdata;
fail:
	kfree(pdata);
	return NULL;
}
#endif
/* file interface - write*/
int tlsc6x_fif_write(char *fname, u8 *pdata, u16 len)
{
	int ret = 0;
	loff_t pos = 0;
	static struct file *pfile = NULL;
	mm_segment_t old_fs = KERNEL_DS;

	pfile = filp_open(fname, O_TRUNC | O_CREAT | O_RDWR, 0644);
	if (IS_ERR(pfile)) {
		ret = -EFAULT;
		tlsc_err("tlsc6x tlsc6x_fif_write:open error!\n");
	} else {
		tlsc_info("tlsc6x tlsc6x_fif_write:start write!\n");
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		ret = (int)vfs_write(pfile, (__force const char __user *)pdata, (size_t)len, &pos);
		vfs_fsync(pfile, 0);
		filp_close(pfile, NULL);
		set_fs(old_fs);
	}
	return ret;
}
#if (defined TPD_AUTO_UPGRADE_PATH) || (defined TLSC_APK_DEBUG)
extern int tlsx6x_update_running_cfg(u16 *ptcfg);
extern int tlsx6x_update_burn_cfg(u16 *ptcfg);
extern int tlsc6x_load_ext_binlib(u8 *pcode, u16 len);
extern int tlsc6x_update_f_combboot(u8 *pdata, u16 len);
int auto_upd_busy = 0;
/* 0:success */
/* 1: no file OR open fail */
/* 2: wrong file size OR read error */
/* -1:op-fial */
int tlsc6x_proc_cfg_update(u8 *dir, int behave)
{
	int ret = 1;
	u8 *pbt_buf = NULL;
	u32 fileSize;
	mm_segment_t old_fs;
	static struct file *file = NULL;
	struct inode *inode;
	loff_t pos;

	TLSC_FUNC_ENTER();
	tlsc_info("tlsc6x proc-file:%s\n", dir);

	file = filp_open(dir, O_RDONLY, 0);
	if (IS_ERR(file)) {
		tlsc_err("tlsc6x proc-file:open error!\n");
	} else {
		ret = 2;
		inode = file->f_inode;
		fileSize = inode->i_size;
		tlsc_info("tlsc6x proc-file, size:%d\n", fileSize);
		pbt_buf = vmalloc(fileSize);
		if (pbt_buf == NULL) {
			tlsc_err("file buf malloc fail\r");
			filp_close(file, NULL);
			return -ENOMEM;
		}

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		pos = 0;
		ret = vfs_read(file, pbt_buf, fileSize, &pos);
		if (ret < 0) {
			tlsc_err("read file failed\r");
		} else {
			tlsc_info("tlsc6x proc-file, read ok1!\n");
			ret = 3;
		}

		if (ret == 3) {
			auto_upd_busy = 1;
			tlsc_irq_disable();
			msleep(1000);
#ifdef CONFIG_PM_WAKELOCKS
			__pm_wakeup_event(&tlsc6x_wakelock, 2000);
#else
			wake_lock_timeout(&tlsc6x_wakelock, msecs_to_jiffies(2000));
#endif

			if (behave == 0) {
				if (fileSize == 204) {
					ret = tlsx6x_update_running_cfg((u16 *) pbt_buf);
				} else if (fileSize > 0x400) {
					tlsc6x_load_ext_binlib((u8 *) pbt_buf, (u16) fileSize);
				}
			} else if (behave == 1) {
				if (fileSize == 204) {
					ret = tlsx6x_update_burn_cfg((u16 *) pbt_buf);
				} else if (fileSize > 0x400) {
					ret = tlsc6x_update_f_combboot((u8 *) pbt_buf, (u16) fileSize);
				}
				tlsc6x_tpd_reset();
			}
			tlsc_irq_enable();
			auto_upd_busy = 0;
		}

		filp_close(file, NULL);
		set_fs(old_fs);

		vfree(pbt_buf);
	}

	return ret;
}

#endif

#ifdef TLSC_APK_DEBUG
unsigned char proc_out_len;
unsigned char proc_out_buf[256];

unsigned char debug_type;
unsigned char iic_reg[2];
unsigned char sync_flag_addr[3];
unsigned char sync_buf_addr[2];
unsigned char reg_len;

static struct proc_dir_entry *tlsc6x_proc_entry = NULL;

static int debug_read(char *writebuf, int writelen, char *readbuf, int readlen)
{
	int ret = 0;

	mutex_lock(&i2c_rw_access);
	tlsc6x_set_dd_mode_sub();

	ret = tlsc6x_i2c_read_sub(this_client, writebuf, writelen, readbuf, readlen);

	tlsc6x_set_nor_mode_sub();
	mutex_unlock(&i2c_rw_access);
	if (ret > 0) {
		ret = readlen;
	}
	return ret;
}

static int debug_write(char *writebuf, int writelen)
{
	int ret = 0;

	mutex_lock(&i2c_rw_access);
	tlsc6x_set_dd_mode_sub();

	ret = tlsc6x_i2c_write_sub(this_client, writebuf, writelen);

	tlsc6x_set_nor_mode_sub();
	mutex_unlock(&i2c_rw_access);
	if (ret > 0) {
		ret = writelen;
	}
	return ret;
}

static int debug_read_sync(char *writebuf, int writelen, char *readbuf, int readlen)
{
	int ret = 0;

	mutex_lock(&i2c_rw_access);
	tlsc6x_set_dd_mode_sub();
	sync_flag_addr[2] = 1;
	ret = tlsc6x_i2c_write_sub(this_client, sync_flag_addr, 3);
	do {
		ret = tlsc6x_i2c_read_sub(this_client, sync_flag_addr, 2, &sync_flag_addr[2], 1);
		if (ret < 0) {
			return ret;
		}
	} while (sync_flag_addr[2] == 1);
	if (ret >= 0) {
		/* read data */
		ret = tlsc6x_i2c_read_sub(this_client, sync_buf_addr, 2, readbuf, readlen);
	}

	tlsc6x_set_nor_mode_sub();
	mutex_unlock(&i2c_rw_access);
	if (ret > 0) {
		ret = readlen;
	}
	return ret;
}

static ssize_t tlsc6x_proc_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
	int ret;
	int buflen = len;
	unsigned char local_buf[256];

	if (buflen > 255) {
		return -EFAULT;
	}

	if (copy_from_user(&local_buf, buff, buflen)) {
		tlsc_err("%s:copy from user error\n", __func__);
		return -EFAULT;
	}
	ret = 0;
	debug_type = local_buf[0];
	/* format:cmd+para+data0+data1+data2... */
	switch (local_buf[0]) {
	case 0:		/* cfg version */
		proc_out_len = 4;
		proc_out_buf[0] = g_tlsc6x_cfg_ver;
		proc_out_buf[1] = g_tlsc6x_cfg_ver >> 8;
		proc_out_buf[2] = g_tlsc6x_cfg_ver >> 16;
		proc_out_buf[3] = g_tlsc6x_cfg_ver >> 24;
		break;
	case 1:
		local_buf[buflen] = '\0';
		if (tlsc6x_proc_cfg_update(&local_buf[2], 0)) {
			len = -EIO;
		}
		break;
	case 2:
		local_buf[buflen] = '\0';
		if (tlsc6x_proc_cfg_update(&local_buf[2], 1)) {
			len = -EIO;
		}
		break;
	case 3:
		ret = debug_write(&local_buf[1], len - 1);
		break;
	case 4:		/* read */
		reg_len = local_buf[1];
		iic_reg[0] = local_buf[2];
		iic_reg[1] = local_buf[3];
		break;
	case 5:		/* read with sync */
		ret = debug_write(&local_buf[1], 4);	/* write size */
		if (ret >= 0) {
			ret = debug_write(&local_buf[5], 4);	/* write addr */
		}
		sync_flag_addr[0] = local_buf[9];
		sync_flag_addr[1] = local_buf[10];
		sync_buf_addr[0] = local_buf[11];
		sync_buf_addr[1] = local_buf[12];
		break;
	case 14:	/* e, esd control */
		g_tp_drvdata->esdHelperFreeze = (int)local_buf[1];
		break;

	default:
		break;
	}
	if (ret < 0) {
		len = ret;
	}

	return len;
}

static ssize_t tlsc6x_proc_read(struct file *filp, char __user *page, size_t len, loff_t *pos)
{
	int ret = 0;

	switch (debug_type) {
	case 0:		/* version information */
		proc_out_len = 4;
		proc_out_buf[0] = g_tlsc6x_cfg_ver;
		proc_out_buf[1] = g_tlsc6x_cfg_ver >> 8;
		proc_out_buf[2] = g_tlsc6x_cfg_ver >> 16;
		proc_out_buf[3] = g_tlsc6x_cfg_ver >> 24;
		if (copy_to_user(page, proc_out_buf, proc_out_len)) {
			ret = -EFAULT;
		} else {
			ret = proc_out_len;
		}
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	case 4:
		len = debug_read(iic_reg, reg_len, proc_out_buf, len);
		if (len > 0) {
			if (copy_to_user(page, proc_out_buf, len)) {
				ret = -EFAULT;
			} else {
				ret = len;
			}
		} else {
			ret = len;
		}
		break;
	case 5:
		len = debug_read_sync(iic_reg, reg_len, proc_out_buf, len);
		if (len > 0) {
			if (copy_to_user(page, proc_out_buf, len)) {
				ret = -EFAULT;
			} else {
				ret = len;
			}
		} else {
			ret = len;
		}
		break;
	default:
		break;
	}

	return ret;
}

static struct file_operations tlsc6x_proc_ops = {
	.owner = THIS_MODULE,
	.read = tlsc6x_proc_read,
	.write = tlsc6x_proc_write,
};

void tlsc6x_release_apk_debug_channel(void)
{
	if (tlsc6x_proc_entry) {
		remove_proc_entry("tlsc6x-debug", NULL);
	}
}

int tlsc6x_create_apk_debug_channel(struct i2c_client *client)
{
	tlsc6x_proc_entry = proc_create("tlsc6x-debug", 0644, NULL, &tlsc6x_proc_ops);

	if (tlsc6x_proc_entry == NULL) {
		tlsc_err("Couldn't create proc entry!\n");
		return -ENOMEM;
	}
	tlsc_info("Create proc entry success!\n");

	return 0;
}
#endif

#ifdef TLSC_ESD_HELPER_EN
static int tlsc6x_power_on(struct regulator *p_reg)
{
	int err = 0;
	int retry = 0;

	TLSC_FUNC_ENTER();
	if (p_reg == NULL) {
		return -EPERM;
	}

	while (retry++ < 30) {
		err = regulator_enable(p_reg);
		if (regulator_is_enabled(p_reg)) {
			break;
		}
	}

	return err;
}

static int tlsc6x_power_off(struct regulator *p_reg)
{
	int err = 0;
	int retry = 0;

	TLSC_FUNC_ENTER();
	if (p_reg == NULL) {
		return -EPERM;
	}

	while (retry++ < 30) {
		err = regulator_disable(p_reg);
		if (regulator_is_enabled(p_reg) == 0) {
			break;
		}
	}

	return err;
}

static int esd_check_work(void)
{
	int ret = -1;
	u8 buf[4] = {0};

	TLSC_FUNC_ENTER();

	buf[0] = 0xe0;
	ret = tlsc6x_i2c_read(this_client, buf, 1, buf, 2);

	if (ret < 0) {		/* maybe confused by some noise,so retry is make sense. */
		msleep(60);
		buf[0] = 0xe0;
		ret = tlsc6x_i2c_read(this_client, buf, 1, buf, 2);
		buf[0] = 0xe0;
		ret = tlsc6x_i2c_read(this_client, buf, 1, buf, 2);
	}

	if (ret >= 0) {
		if ((buf[0] != 0x0e) || (buf[1] == 0x0) || (buf[1] > 0x80)) {
			ret = -EIO;
		}
	}

	if (ret < 0) {
		tlsc6x_tpd_reset();
		/* double check, reserve time for bootting up */
		msleep(50);
		buf[0] = 0xe0;
		ret = tlsc6x_i2c_read(this_client, buf, 1, buf, 2);
		if (ret >= 0) {
			if ((buf[0] != 0x0e) || (buf[1] == 0x0) || (buf[1] > 0x80)) {
				ret = -EIO;
			}
		}
		if (ret < 0) {
			/* re-power-on and reset*/
			tlsc6x_power_off(g_tp_drvdata->reg_vdd);
			msleep(20);
			tlsc6x_power_on(g_tp_drvdata->reg_vdd);

			tlsc6x_tpd_reset();
		}
	}

	return ret;
}

static int esd_checker_handler(void *unused)
{
	ktime_t ktime;

	do {
		wait_event_interruptible(tpd_esd_waiter, tpd_esd_flag != 0);
		tpd_esd_flag = 0;

		ktime = ktime_set(4, 0);
		hrtimer_start(&tpd_esd_kthread_timer, ktime, HRTIMER_MODE_REL);

		if (g_tp_drvdata->esdHelperFreeze) {
			continue;
		}
#if (defined TPD_AUTO_UPGRADE_PATH) || (defined TLSC_APK_DEBUG)
		if (auto_upd_busy) {
			continue;
		}
#endif
		esd_check_work();

	} while (!kthread_should_stop());

	return 0;
}

enum hrtimer_restart tpd_esd_kthread_hrtimer_func(struct hrtimer *timer)
{
	tpd_esd_flag = 1;
	wake_up_interruptible(&tpd_esd_waiter);

	return HRTIMER_NORESTART;
}
#endif

#ifdef TLSC_TP_PROC_SELF_TEST
int tlsc6x_chip_self_test(void)
{
	int ret = 0;

	TLSC_FUNC_ENTER();
	auto_upd_busy = 1;
	g_tp_drvdata->esdHelperFreeze = 1;
	tlsc_irq_disable();
#ifdef CONFIG_PM_WAKELOCKS
	__pm_wakeup_event(&tlsc6x_wakelock, 2000);
#else
	wake_lock_timeout(&tlsc6x_wakelock, msecs_to_jiffies(2000));
#endif
	ret = tlsc6x_chip_self_test_sub();

	auto_upd_busy = 0;
	g_tp_drvdata->esdHelperFreeze = 0;
	tlsc_irq_enable();

	return ret;
}

#endif

#ifdef TLSC_TPD_PROXIMITY

static int tlsc6x_prox_ctrl(int enable)
{
	TLSC_FUNC_ENTER();
	if (enable == 1) {
		tpd_prox_active = 1;
		tlsc6x_write_reg(this_client, 0xb0, 0x01);
	} else if (enable == 0) {
		tpd_prox_active = 0;
		tlsc6x_write_reg(this_client, 0xb0, 0x00);
	}
	tpd_prox_old_state = 0xe0;	/* default is far away*/
	tlsc_info("%s: tpd_prox_active is %d\n", __func__, tpd_prox_active);
	return 1;
}

static ssize_t tlsc6x_prox_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "ps enable %d\n", tpd_prox_active);
}

static ssize_t tlsc6x_prox_enable_store(struct device *dev, struct device_attribute *attr, const char *buf,
					size_t count)
{
	unsigned int enable;
	int ret = 0;

	TLSC_FUNC_ENTER();
	ret = kstrtouint(buf, 0, &enable);
	if (ret)
		return -EINVAL;
	enable = (enable > 0) ? 1 : 0;
	if (!real_suspend_flag) {
		tlsc6x_prox_ctrl(enable);
	} else {
		tpd_prox_active = enable;
	}

	return count;

}

static DEVICE_ATTR(proximity, 0664, tlsc6x_prox_enable_show, tlsc6x_prox_enable_store);

/* default cmd interface(refer to sensor HAL):"/sys/class/sprd-tpd/device/proximity" */
static void tlsc6x_prox_cmd_path_init(void)
{
	sprd_tpd_class = class_create(THIS_MODULE, "sprd-tpd");
	if (IS_ERR(sprd_tpd_class)) {
		tlsc_err("tlsc6x error::create sprd-tpd fail\n");
	} else {
		sprd_ps_cmd_dev = device_create(sprd_tpd_class, NULL, 0, NULL, "device");
		if (IS_ERR(sprd_ps_cmd_dev)) {
			tlsc_err("tlsc6x error::create ges&ps cmd-io fail\n");
		} else {
			/* sys/class/sprd-tpd/device/proximity */
			if (device_create_file(sprd_ps_cmd_dev, &dev_attr_proximity) < 0) {
				tlsc_err("tlsc6x error::create device file fail(%s)!\n", dev_attr_proximity.attr.name);
			}
		}
	}

}

static ssize_t tpd_psensor_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 64, "%d\n", tpd_prox_active);
}

static ssize_t tpd_psensor_enable_store(struct device *dev, struct device_attribute *attr, const char *buf,
					size_t count)
{
	unsigned int enable;
	int ret = 0;

	TLSC_FUNC_ENTER();
	ret = kstrtouint(buf, 0, &enable);
	if (ret)
		return -EINVAL;
	enable = (enable > 0) ? 1 : 0;
	if (!real_suspend_flag) {
		tlsc6x_prox_ctrl(enable);
	} else {
		tpd_prox_active = enable;
	}

	return count;

}

static ssize_t tpd_psensor_flush_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 64, "%d\n", tpd_prox_active);
}

static ssize_t tpd_psensor_flush_store(struct device *dev, struct device_attribute *attr, const char *buf,
					size_t count)
{
	struct tlsc6x_data *data = i2c_get_clientdata(this_client);

	input_report_abs(data->ps_input_dev, ABS_DISTANCE, -1);
	input_mt_sync(data->ps_input_dev);
	input_sync(data->ps_input_dev);

	return count;

}

static DEVICE_ATTR(enable, 0644, tpd_psensor_enable_show, tpd_psensor_enable_store);
static DEVICE_ATTR(flush, 0644, tpd_psensor_flush_show, tpd_psensor_flush_store);

static struct attribute *tpd_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_flush.attr,
	NULL
};

static struct attribute_group tpd_attribute_group = {
	.attrs = tpd_attributes
};
#endif
static int tlsc6x_request_irq_work(void)
{
	int ret = 0;

	this_client->irq = gpio_to_irq(g_tp_drvdata->platform_data->irq_gpio_number);
	tlsc_info("The irq node num is %d", this_client->irq);

	ret = request_threaded_irq(this_client->irq,
				   NULL, touch_event_thread_handler,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   "tlsc6x_tpd_irq", NULL);
	if (ret < 0) {
		tlsc_err("Request irq thread error!");
		return  ret;
	}

	return ret;
}

static int tlsc6x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = -1;
	int err = 0;
	struct input_dev *input_dev;
	struct tlsc6x_platform_data *pdata = NULL;

	TLSC_FUNC_ENTER();
	if (tpd_fw_cdev.TP_have_registered) {
		pr_notice("TP have registered by other TP.\n");
		return -EPERM;
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_alloc_platform_data_failed;
	}
#ifdef CONFIG_OF		/* NOTE:THIS IS MUST!!! */
	if (client->dev.of_node) {
		pdata = tlsc6x_parse_dt(&client->dev);
		if (pdata) {
			client->dev.platform_data = pdata;
		}
	}
#endif

	if (pdata == NULL) {
		err = -ENOMEM;
		tlsc_err("%s: no platform data!!!\n", __func__);
		goto exit_alloc_platform_data_failed;
	}

	g_tp_drvdata = kzalloc(sizeof(*g_tp_drvdata), GFP_KERNEL);	/* auto clear */
	if (!g_tp_drvdata) {
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	this_client = client;
	g_tp_drvdata->client = client;
	g_tp_drvdata->platform_data = pdata;

	err = tlsc6x_pinctrl_init(g_tp_drvdata);
	if (err < 0) {
		tlsc_err("tlsc6x get pinctrl failed\n");
		goto exit_pinctrl_init_failed;
	} else {
		tlsc_pinctrl_select_init(g_tp_drvdata);
	}

	err = tlsc6x_hw_init(g_tp_drvdata);
	if (err < 0) {
		goto exit_gpio_request_failed;
	}

	i2c_set_clientdata(client, g_tp_drvdata);

	/* #ifdef CONFIG_I2C_SPRD */
	/* sprd_i2c_ctl_chg_clk(client->adapter->nr, 400000); */
	/* #endif */
	g_is_telink_comp = tlsc6x_tp_dect(client);

	g_tp_drvdata->needKeepRamCode = g_needKeepRamCode;

	if (g_is_telink_comp) {
		tlsc6x_tpd_reset();
	} else {
		tlsc_err("tlsc6x:%s, no tlsc6x!\n", __func__);
		err = -ENODEV;
		goto exit_chip_check_failed;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		tlsc_err("tlsc6x error::failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	g_tp_drvdata->input_dev = input_dev;

	__set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	__set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);
	__set_bit(KEY_MENU, input_dev->keybit);
	__set_bit(KEY_BACK, input_dev->keybit);
	__set_bit(KEY_HOMEPAGE, input_dev->keybit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

#if MULTI_PROTOCOL_TYPE_B
	input_mt_init_slots(input_dev, TS_MAX_FINGER, 0);
#else
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->x_res_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->y_res_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 15, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
#ifdef PRESSURE_REPORT
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 127, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_DISTANCE, -1, 1, 0, 0);	/* give this capability aways */
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

	input_dev->name = "tlsc6x_touch";
	err = input_register_device(input_dev);
	if (err) {
		tlsc_err("tlsc6x error::failed to register input device: %s\n", dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}
#ifdef TLSC_TPD_PROXIMITY
	tlsc6x_prox_cmd_path_init();
	g_tp_drvdata->ps_input_dev = input_allocate_device();
	if (!g_tp_drvdata->ps_input_dev) {
		err = -ENOMEM;
		tlsc_err("tlsc6x error::failed to allocate ps-input device\n");
		goto exit_input_register_device_failed;
	}
	g_tp_drvdata->ps_input_dev->name = "proximity_tp";
	set_bit(EV_ABS, g_tp_drvdata->ps_input_dev->evbit);
	input_set_capability(g_tp_drvdata->ps_input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(g_tp_drvdata->ps_input_dev, ABS_DISTANCE, -1, 1, 0, 0);
	err = input_register_device(g_tp_drvdata->ps_input_dev);
	if (err) {
		tlsc_err("tlsc6x error::failed to register ps-input device: %s\n", dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}
	err = sysfs_create_group(&g_tp_drvdata->ps_input_dev->dev.kobj, &tpd_attribute_group);
	if (err) {
		tlsc_err("input create group failed.\n");
		goto exit_input_register_device_failed;
	}
#endif

#ifdef TOUCH_VIRTUAL_KEYS
	tlsc6x_virtual_keys_init();
#endif

	INIT_WORK(&g_tp_drvdata->resume_work, tlsc6x_resume_work);
	g_tp_drvdata->tp_resume_workqueue = create_singlethread_workqueue("tlsc6x_resume_work");
	if (!g_tp_drvdata->tp_resume_workqueue) {
		err = -ESRCH;
		goto exit_input_register_device_failed;
	}

#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_init(&tlsc6x_wakelock, "tlsc6x_wakelock");
#else
	wake_lock_init(&tlsc6x_wakelock, WAKE_LOCK_SUSPEND, "tlsc6x_wakelock");
#endif

	err = tlsc6x_request_irq_work();
	if (err < 0) {
		tlsc_err("tlsc6x error::request irq failed %d\n", err);
		goto exit_irq_request_failed;
	}
#if defined(CONFIG_ADF)
	g_tp_drvdata->fb_notif.notifier_call = ts_adf_event_handler;
	g_tp_drvdata->fb_notif.priority = 1000;
	ret = adf_register_client(&g_tp_drvdata->fb_notif);
	if (ret) {
		tlsc_err("tlsc6x error::unable to register fb_notifier: %d", ret);
	}
#elif defined(CONFIG_FB)
	g_tp_drvdata->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&g_tp_drvdata->fb_notif);
	if (ret) {
		tlsc_err("tlsc6x error::Unable to register fb_notifier: %d", ret);
	}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	g_tp_drvdata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	g_tp_drvdata->early_suspend.suspend = tlsc6x_ts_suspend;
	g_tp_drvdata->early_suspend.resume = tlsc6x_ts_resume;
	register_early_suspend(&g_tp_drvdata->early_suspend);
#endif

#ifdef TLSC_APK_DEBUG
	tlsc6x_create_apk_debug_channel(client);
#endif

#ifdef TLSC_ESD_HELPER_EN
	{			/* esd issue: i2c monitor thread */
		ktime_t ktime = ktime_set(30, 0);

		hrtimer_init(&tpd_esd_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		tpd_esd_kthread_timer.function = tpd_esd_kthread_hrtimer_func;
		hrtimer_start(&tpd_esd_kthread_timer, ktime, HRTIMER_MODE_REL);
		kthread_run(esd_checker_handler, 0, "tlsc6x_esd_helper");
	}
#endif

	tlsc6x_get_tp_vendor_info();
	tlsc6x_tpd_register_fw_class();
	tpd_fw_cdev.TP_have_registered = true;
	tlsc_info(" tlsc6x_probe success!");
	return 0;

exit_irq_request_failed:
	input_unregister_device(input_dev);
exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
exit_chip_check_failed:
	gpio_free(pdata->irq_gpio_number);
	gpio_free(pdata->reset_gpio_number);
exit_gpio_request_failed:
	devm_pinctrl_put(g_tp_drvdata->pinctrl);
	g_tp_drvdata->pinctrl = NULL;
	g_tp_drvdata->pins_init = NULL;
#ifdef CONFIG_TS_TLSC6X_MTK_INTERFACE
	g_tp_drvdata->pinctrl_rst_output0 = NULL;
	g_tp_drvdata->pinctrl_rst_output1 = NULL;
#endif
exit_pinctrl_init_failed:
	kfree(g_tp_drvdata);
exit_alloc_data_failed:
	if (pdata != NULL) {
		kfree(pdata);
	}
	g_tp_drvdata = NULL;
	i2c_set_clientdata(client, g_tp_drvdata);
exit_alloc_platform_data_failed:
	return err;
}

static int tlsc6x_remove(struct i2c_client *client)
{
	struct tlsc6x_data *drvdata = i2c_get_clientdata(client);

	TLSC_FUNC_ENTER();

#ifdef TLSC_APK_DEBUG
	tlsc6x_release_apk_debug_channel();
#endif

#ifdef TLSC_ESD_HELPER_EN
	hrtimer_cancel(&tpd_esd_kthread_timer);
#endif

	devm_pinctrl_put(drvdata->pinctrl);
	drvdata->pinctrl = NULL;
	drvdata->pins_init = NULL;
#ifdef CONFIG_TS_TLSC6X_MTK_INTERFACE
	drvdata->pinctrl_rst_output0 = NULL;
	drvdata->pinctrl_rst_output1 = NULL;
#endif

	if (drvdata == NULL) {
		return 0;
	}
#if defined(CONFIG_ADF)
	adf_unregister_client(&g_tp_drvdata->fb_notif);
#elif defined(CONFIG_FB)
	fb_unregister_client(&g_tp_drvdata->fb_notif);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&drvdata->early_suspend);
#endif

	free_irq(client->irq, drvdata);
	input_unregister_device(drvdata->input_dev);
	input_free_device(drvdata->input_dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	cancel_work_sync(&drvdata->resume_work);
	destroy_workqueue(drvdata->tp_resume_workqueue);
#endif
	kfree(drvdata);
	drvdata = NULL;
	i2c_set_clientdata(client, drvdata);

	return 0;
}

static const struct i2c_device_id tlsc6x_id[] = {
	{TS_NAME, 0}, {}
};

MODULE_DEVICE_TABLE(i2c, tlsc6x_id);

static const struct of_device_id tlsc6x_of_match[] = {
	{.compatible = "tlsc6x,tlsc6x_ts",},
	{}
};

MODULE_DEVICE_TABLE(of, tlsc6x_of_match);
static struct i2c_driver tlsc6x_driver = {
	.probe = tlsc6x_probe,
	.remove = tlsc6x_remove,
	.id_table = tlsc6x_id,
	.driver = {
		   .name = TS_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = tlsc6x_of_match,
		   },
};

static int __init tlsc6x_init(void)
{
	tlsc_info("%s: ++\n", __func__);
	return i2c_add_driver(&tlsc6x_driver);
}

static void __exit tlsc6x_exit(void)
{
	i2c_del_driver(&tlsc6x_driver);
}

module_init(tlsc6x_init);
module_exit(tlsc6x_exit);

MODULE_DESCRIPTION("Chipsemi touchscreen driver");
MODULE_LICENSE("GPL");
