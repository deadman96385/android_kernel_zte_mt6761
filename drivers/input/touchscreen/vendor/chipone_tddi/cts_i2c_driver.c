#define LOG_TAG         "I2CDrv"
#include <linux/power_supply.h>
#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sysfs.h"
#include "chipone_common.h"
#include "tpd_sys.h"

bool cts_show_debug_log = false;
module_param_named(debug_log, cts_show_debug_log, bool, 0660);
MODULE_PARM_DESC(debug_log, "Show debug log control");

static int cts_suspend(struct chipone_ts_data *cts_data)
{
	int ret;

	cts_info("Suspend");

	cts_lock_device(&cts_data->cts_dev);
	ret = cts_suspend_device(&cts_data->cts_dev);
	cts_unlock_device(&cts_data->cts_dev);

	if (ret) {
		cts_err("Suspend device failed %d", ret);
		/* TODO: */
		/* return ret; */
	}

	ret = cts_stop_device(&cts_data->cts_dev);
	if (ret) {
		cts_err("Stop device failed %d", ret);
		return ret;
	}
#ifdef CONFIG_CTS_GESTURE
	/* Enable IRQ wake if gesture wakeup enabled */
	if (cts_is_gesture_wakeup_enabled(&cts_data->cts_dev)) {
		ret = cts_plat_enable_irq_wake(cts_data->pdata);
		if (ret) {
			cts_err("Enable IRQ wake failed %d", ret);
			return ret;
		}
		irq_set_irq_type(cts_data->pdata->irq, IRQF_TRIGGER_LOW | IRQF_NO_SUSPEND | IRQF_ONESHOT);
		ret = cts_plat_enable_irq(cts_data->pdata);
		if (ret) {
			cts_err("Enable IRQ failed %d", ret);
			return ret;
		}
	}
#endif /* CONFIG_CTS_GESTURE */

    /** - To avoid waking up while not sleeping,
	delay 20ms to ensure reliability */
	msleep(20);

	return 0;
}

static int cts_resume(struct chipone_ts_data *cts_data)
{
	int ret;

	cts_info("Resume");

#ifdef CONFIG_CTS_GESTURE
	if (cts_is_gesture_wakeup_enabled(&cts_data->cts_dev)) {
		ret = cts_plat_disable_irq_wake(cts_data->pdata);
		if (ret) {
			cts_warn("Disable IRQ wake failed %d", ret);
			/* return ret; */
		}
		irq_set_irq_type(cts_data->pdata->irq, IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND | IRQF_ONESHOT);
		ret = cts_plat_disable_irq(cts_data->pdata);
		if (ret < 0) {
			cts_err("Disable IRQ failed %d", ret);
			/* return ret; */
		}
	}
#endif /* CONFIG_CTS_GESTURE */

	ret = cts_resume_device(&cts_data->cts_dev);
	if (ret) {
		cts_warn("Resume device failed %d", ret);
		return ret;
	}

	ret = cts_start_device(&cts_data->cts_dev);
	if (ret) {
		cts_err("Start device failed %d", ret);
		return ret;
	}

	return 0;
}

void set_tp_suspend(struct chipone_ts_data *cts_data, bool enable)
{
	int ret = 0;

	if (enable) {
		ret = cts_send_command(&cts_data->cts_dev, CTS_CMD_SUSPEND);
		if (ret) {
			cts_err("Suspend enter gesture mode failed %d", ret);
		}
	} else {
		ret = cts_plat_reset_device(cts_data->pdata);
		if (ret < 0) {
			cts_err("Reset device failed %d", ret);
		}
	}
}

#if defined CONFIG_CTS_PM_ADF_NOTIFIER
static int fb_notifier_callback(struct notifier_block *nb, unsigned long action, void *data)
{
	const struct cts_platform_data *pdata = container_of(nb, struct cts_platform_data, fb_notifier);
	struct chipone_ts_data *cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

	struct adf_notifier_event *event = data;
	int adf_event_data;

	if (action != ADF_EVENT_BLANK)
		return NOTIFY_DONE;

	adf_event_data = *(int *)event->data;
	cts_info("receive adf event with adf_event_data=%d", adf_event_data);

	switch (adf_event_data) {
	case DRM_MODE_DPMS_ON:
		queue_work(cts_data->workqueue, &cts_data->ts_resume_work);
		break;
	case DRM_MODE_DPMS_OFF:
		cts_suspend(cts_data);
		break;
	default:
		cts_info("receive adf event with error data, adf_event_data=%d", adf_event_data);
		break;
	}

	return NOTIFY_OK;
}

static int cts_init_pm_fb_notifier(struct chipone_ts_data *cts_data)
{
	cts_info("Init FB notifier");

	cts_data->pdata->fb_notifier.notifier_call = fb_notifier_callback;
	cts_data->pdata->fb_notifier.priority = 1000;

	return adf_register_client(&cts_data->pdata->fb_notifier);
}

static int cts_deinit_pm_fb_notifier(struct chipone_ts_data *cts_data)
{
	cts_info("Deinit FB notifier");

	return adf_unregister_client(&cts_data->pdata->fb_notifier);
}
#elif defined CONFIG_CTS_PM_FB_NOTIFIER
static int fb_notifier_callback(struct notifier_block *nb, unsigned long action, void *data)
{
	const struct cts_platform_data *pdata = container_of(nb, struct cts_platform_data, fb_notifier);
	struct chipone_ts_data *cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
	struct fb_event *evdata = data;
	int blank;

	cts_info("FB notifier callback");
	if (evdata && evdata->data) {
		if (action == FB_EVENT_BLANK) {
			blank = *(int *)evdata->data;
			if (blank == FB_BLANK_UNBLANK) {
				queue_work(cts_data->workqueue, &cts_data->ts_resume_work);
				return NOTIFY_OK;
			}
		} else if (action == FB_EARLY_EVENT_BLANK) {
			blank = *(int *)evdata->data;
			if (blank == FB_BLANK_POWERDOWN) {
				cts_suspend(cts_data);
				return NOTIFY_OK;
			}
		}
	}

	return NOTIFY_DONE;
}

static int cts_init_pm_fb_notifier(struct chipone_ts_data *cts_data)
{
	cts_info("Init FB notifier");

	cts_data->pdata->fb_notifier.notifier_call = fb_notifier_callback;

	return fb_register_client(&cts_data->pdata->fb_notifier);
}

static int cts_deinit_pm_fb_notifier(struct chipone_ts_data *cts_data)
{
	cts_info("Deinit FB notifier");

	return fb_unregister_client(&cts_data->pdata->fb_notifier);
}
#endif /* CONFIG_CTS_PM_FB_NOTIFIER */

#ifdef CONFIG_CTS_CHARGER_DETECT
static int cts_get_charger_ststus(int *status)
{
	static struct power_supply *batt_psy;
	union power_supply_propval val = { 0, };

	if (batt_psy == NULL)
		batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_STATUS, &val);
	}
	if ((val.intval == POWER_SUPPLY_STATUS_CHARGING) ||
		(val.intval == POWER_SUPPLY_STATUS_FULL)){
		*status = 1;
	} else {
		*status = 0;
	}
	cts_info("charger status:%d", *status);
	return 0;
/*
	u8 tmp;
	int ret = 0;
	struct file *file;

	file = filp_open(CONFIG_CTS_CHARGER_DETECT_FILE, O_RDONLY, 0);
	if (IS_ERR(file)) {
		ret = -1;
		cts_err("can not open file:%s", CONFIG_CTS_CHARGER_DETECT_FILE);
		goto cts_get_charger_ststus_exit;
	}
	if (kernel_read(file, 0, &tmp, 1) < 0) {
		ret = -1;
		cts_err("can not read file:%s", CONFIG_CTS_CHARGER_DETECT_FILE);
		goto cts_get_charger_ststus_close_file;
	}
	if (tmp == '1') {
		*status = 1;
	}
	else {
		*status = 0;
	}
cts_get_charger_ststus_close_file:
	filp_close(file, NULL);
cts_get_charger_ststus_exit:
	cts_info("cts_get_charger_ststus:%d", *status);
	return ret;
*/
}

static void cts_delayed_work_charger(struct work_struct *work)
{
	int ret, status;
	struct chipone_ts_data *cts_data;

	cts_data = container_of(work, struct chipone_ts_data, charger_work.work);

	ret = cts_get_charger_ststus(&status);
	if (ret) {
		cts_err("get charger status err");
		return;
	} else {
		cts_info("charger status:%d", status);
	}
	if (!cts_is_device_enabled(&cts_data->cts_dev)) {
		cts_err("Charger status changed, but device is not enabled");
		cts_data->cts_dev.rtdata.charger_exist = status;
		return;
	}
	if (status) {
		cts_charger_plugin(&cts_data->cts_dev);
	} else {
		cts_charger_plugout(&cts_data->cts_dev);
	}
}

static int cts_charger_notify_call(struct notifier_block *nb, unsigned long event, void *data)
{
	struct power_supply *psy = data;
	const struct cts_platform_data *pdata = container_of(nb, struct cts_platform_data, charger_notifier);
	struct chipone_ts_data *cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

	if (event != PSY_EVENT_PROP_CHANGED) {
		return NOTIFY_DONE;
	}

	if ((strcmp(psy->desc->name, "usb") == 0)
	    || (strcmp(psy->desc->name, "ac") == 0)) {
		if (delayed_work_pending(&cts_data->charger_work)) {
			return NOTIFY_DONE;
		}
		queue_delayed_work(cts_data->workqueue, &cts_data->charger_work, msecs_to_jiffies(50));
	}

	return NOTIFY_DONE;
}

static int cts_init_charger_notifier(struct chipone_ts_data *cts_data)
{
	int ret;

	cts_info("Init Charger notifier");

	cts_data->pdata->charger_notifier.notifier_call = cts_charger_notify_call;
	ret = power_supply_reg_notifier(&cts_data->pdata->charger_notifier);
	return ret;
}

static int cts_deinit_charger_notifier(struct chipone_ts_data *cts_data)
{
	cts_info("Deinit Charger notifier");

	power_supply_unreg_notifier(&cts_data->pdata->charger_notifier);
	return 0;
}
#endif

static void cts_delayed_work_upgrade(struct work_struct *work)
{
	struct chipone_ts_data *cts_data = NULL;
	int ret = 0;

	cts_data = container_of(work, struct chipone_ts_data, upgrade_work);
	if (cts_is_device_suspended(&cts_data->cts_dev)) {
		return;
	}
	ret = cts_upgrade_fw_delayed(&cts_data->cts_dev);
	if (ret) {
		cts_err("cts_upgrade_fw_delayed failed");
	}
	ret = cts_start_device(&cts_data->cts_dev);
	if (ret) {
		cts_err("Start device failed %d", ret);
	}
}

static void cts_resume_work(struct work_struct *work)
{
	struct chipone_ts_data *cts_data = NULL;

	cts_data = container_of(work, struct chipone_ts_data, ts_resume_work);
	cts_info("resume queue work.\n");
	cts_resume(cts_data);
}

static int cts_i2c_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct chipone_ts_data *cts_data = NULL;
	int ret = 0;
	int status = 0;

	cts_info("Probe i2c client: name='%s' addr=0x%02x flags=0x%02x irq=%d",
		 client->name, client->addr, client->flags, client->irq);
	if (tpd_fw_cdev.TP_have_registered) {
		cts_info("TP have registered by other TP.\n");
		return -ENODEV;
	}
#if !defined(CONFIG_MTK_PLATFORM)
	if (client->addr != CTS_NORMAL_MODE_I2CADDR) {
		cts_err("Probe i2c addr 0x%02x != driver config addr 0x%02x", client->addr, CTS_NORMAL_MODE_I2CADDR);
		return -ENODEV;
	};
#endif

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		cts_err("Check functionality failed");
		return -ENODEV;
	}

	cts_data = kzalloc(sizeof(*cts_data), GFP_KERNEL);
	if (cts_data == NULL) {
		cts_err("Allocate chipone_ts_data failed");
		return -ENOMEM;
	}

	cts_data->pdata = kzalloc(sizeof(struct cts_platform_data), GFP_KERNEL);
	if (cts_data->pdata == NULL) {
		cts_err("Allocate cts_platform_data failed");
		ret = -ENOMEM;
		goto err_free_cts_data;
	}

	i2c_set_clientdata(client, cts_data);
	cts_data->i2c_client = client;

	cts_init_platform_data(cts_data->pdata, client);

	cts_data->cts_dev.pdata = cts_data->pdata;
	cts_data->pdata->cts_dev = &cts_data->cts_dev;

	cts_data->workqueue = create_singlethread_workqueue(CFG_CTS_DEVICE_NAME "-workqueue");

	if (cts_data->workqueue == NULL) {
		cts_err("Create workqueue failed");
		ret = -ENOMEM;
		goto err_free_pdata;
	}

	INIT_WORK(&cts_data->upgrade_work, cts_delayed_work_upgrade);
	INIT_WORK(&cts_data->ts_resume_work, cts_resume_work);

	ret = cts_plat_request_resource(cts_data->pdata);
	if (ret < 0) {
		cts_err("Request resource failed %d", ret);
		goto err_destroy_workqueue;
	}

	ret = cts_plat_reset_device(cts_data->pdata);
	if (ret < 0) {
		cts_err("Reset device failed %d", ret);
		goto err_free_resource;
	}

	ret = cts_probe_device(&cts_data->cts_dev);
	if (ret) {
		cts_err("Probe device failed %d", ret);
		goto err_free_resource;
	}

	ret = cts_plat_init_touch_device(cts_data->pdata);
	if (ret < 0) {
		cts_err("Init touch device failed %d", ret);
		goto err_free_resource;
	}

	ret = cts_plat_init_vkey_device(cts_data->pdata);
	if (ret < 0) {
		cts_err("Init vkey device failed %d", ret);
		goto err_deinit_touch_device;
	}

	ret = cts_plat_init_gesture(cts_data->pdata);
	if (ret < 0) {
		cts_err("Init gesture failed %d", ret);
		goto err_deinit_vkey_device;
	}

	cts_init_esd_protection(cts_data);

	ret = cts_tool_init(cts_data);
	if (ret < 0) {
		cts_warn("Init tool node failed %d", ret);
	}

	ret = cts_sysfs_add_device(&client->dev);
	if (ret < 0) {
		cts_warn("Add sysfs entry for device failed %d", ret);
	}
#ifdef CONFIG_CTS_PM_FB_NOTIFIER
	ret = cts_init_pm_fb_notifier(cts_data);
	if (ret) {
		cts_err("Init FB notifier failed %d", ret);
		goto err_deinit_sysfs;
	}
#endif /* CONFIG_CTS_PM_FB_NOTIFIER */

#ifdef CONFIG_CTS_CHARGER_DETECT
	INIT_DELAYED_WORK(&cts_data->charger_work, cts_delayed_work_charger);
	ret = cts_init_charger_notifier(cts_data);
	if (ret) {
		cts_err("Init Charger notifer failed %d", ret);
		goto err_deinit_pm_fb;
	}
	ret = cts_get_charger_ststus(&status);
	if (ret) {
		cts_err("get charger status err");
	} else {
		cts_info("charger status:%d", status);
	}
	if (status) {
		cts_charger_plugin(&cts_data->cts_dev);
	} else {
		cts_charger_plugout(&cts_data->cts_dev);
	}
#endif
	ret = cts_plat_request_irq(cts_data->pdata);
	if (ret < 0) {
		cts_err("Request IRQ failed %d", ret);
		goto err_unregister_charger_notifer;
	}

	cts_register_fw_class(&cts_data->cts_dev);
	queue_work(cts_data->workqueue, &cts_data->upgrade_work);
	tpd_fw_cdev.TP_have_registered = true;
	return 0;

err_unregister_charger_notifer:
#ifdef CONFIG_CTS_CHARGER_DETECT
	cts_deinit_charger_notifier(cts_data);
err_deinit_pm_fb:
#endif
#ifdef CONFIG_CTS_PM_FB_NOTIFIER
	cts_deinit_pm_fb_notifier(cts_data);
#endif /* CONFIG_CTS_PM_FB_NOTIFIER */
err_deinit_sysfs:
	cts_sysfs_remove_device(&client->dev);
#ifdef CONFIG_CTS_LEGACY_TOOL
	cts_tool_deinit(cts_data);
#endif /* CONFIG_CTS_LEGACY_TOOL */

#ifdef CONFIG_CTS_ESD_PROTECTION
	cts_deinit_esd_protection(cts_data);
#endif /* CONFIG_CTS_ESD_PROTECTION */

#ifdef CONFIG_CTS_GESTURE
	cts_plat_deinit_gesture(cts_data->pdata);
#endif /* CONFIG_CTS_GESTURE */

err_deinit_vkey_device:
#ifdef CONFIG_CTS_VIRTUALKEY
	cts_plat_deinit_vkey_device(cts_data->pdata);
#endif /* CONFIG_CTS_VIRTUALKEY */

err_deinit_touch_device:
	cts_plat_deinit_touch_device(cts_data->pdata);

err_free_resource:
	cts_plat_free_resource(cts_data->pdata);

err_destroy_workqueue:
	destroy_workqueue(cts_data->workqueue);

err_free_pdata:
	kfree(cts_data->pdata);

err_free_cts_data:
	kfree(cts_data);

	cts_err("Probe failed %d", ret);

	return ret;
}

static int cts_i2c_driver_remove(struct i2c_client *client)
{
	struct chipone_ts_data *cts_data;
	int ret = 0;

	cts_info("Remove");

	cts_data = (struct chipone_ts_data *)i2c_get_clientdata(client);
	if (cts_data && cts_data->pdata) {
		ret = cts_stop_device(&cts_data->cts_dev);
		if (ret) {
			cts_warn("Stop device failed %d", ret);
		}

		cts_plat_free_irq(cts_data->pdata);

#ifdef CONFIG_CTS_CHARGER_DETECT
		cts_deinit_charger_notifier(cts_data);
#endif

#ifdef CONFIG_CTS_PM_FB_NOTIFIER
		cts_deinit_pm_fb_notifier(cts_data);
#endif /* CONFIG_CTS_PM_FB_NOTIFIER */

		cts_tool_deinit(cts_data);

		cts_sysfs_remove_device(&client->dev);

		cts_deinit_esd_protection(cts_data);

		cts_plat_deinit_touch_device(cts_data->pdata);

		cts_plat_deinit_vkey_device(cts_data->pdata);

		cts_plat_deinit_gesture(cts_data->pdata);

		cts_plat_free_resource(cts_data->pdata);

		if (cts_data->workqueue) {
			destroy_workqueue(cts_data->workqueue);
		}
		/*if (cts_data->upgrade_wq) {
		cancel_delayed_work(&cts_data->upgrade_work);
		flush_workqueue(cts_data->upgrade_wq);
		destroy_workqueue(cts_data->upgrade_wq);
		} */

		kfree(cts_data->pdata);
		kfree(cts_data);
	} else {
		cts_warn("Chipone i2c driver remove while NULL chipone_ts_data");
		return -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_CTS_PM_LEGACY
static int cts_i2c_driver_suspend(struct device *dev, pm_message_t state)
{
	cts_info("Suspend by legacy power management");
	return cts_suspend(dev_get_drvdata(dev));
}

static int cts_i2c_driver_resume(struct device *dev)
{
	cts_info("Resume by legacy power management");
	return cts_resume(dev_get_drvdata(dev));
}
#endif /* CONFIG_CTS_PM_LEGACY */

#ifdef CONFIG_CTS_PM_GENERIC
static int cts_i2c_driver_pm_suspend(struct device *dev)
{
	cts_info("Suspend by bus power management");
	return cts_suspend(dev_get_drvdata(dev));
}

static int cts_i2c_driver_pm_resume(struct device *dev)
{
	cts_info("Resume by bus power management");
	return cts_resume(dev_get_drvdata(dev));
}

/* bus control the suspend/resume procedure */
static const struct dev_pm_ops cts_i2c_driver_pm_ops = {
	.suspend = cts_i2c_driver_pm_suspend,
	.resume = cts_i2c_driver_pm_resume,
};
#endif /* CONFIG_CTS_PM_GENERIC */

#ifdef CONFIG_CTS_SYSFS
static ssize_t reset_pin_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_HAS_RESET_PIN: %c\n",
#ifdef CFG_CTS_HAS_RESET_PIN
			'Y'
#else
			'N'
#endif
	    );
}

static DRIVER_ATTR(reset_pin, S_IRUGO, reset_pin_show, NULL);

static ssize_t swap_xy_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_SWAP_XY: %c\n",
#ifdef CFG_CTS_SWAP_XY
			'Y'
#else
			'N'
#endif
	    );
}

static DRIVER_ATTR(swap_xy, S_IRUGO, swap_xy_show, NULL);

static ssize_t wrap_x_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_WRAP_X: %c\n",
#ifdef CFG_CTS_WRAP_X
			'Y'
#else
			'N'
#endif
	    );
}

static DRIVER_ATTR(wrap_x, S_IRUGO, wrap_x_show, NULL);

static ssize_t wrap_y_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_WRAP_Y: %c\n",
#ifdef CFG_CTS_WRAP_Y
			'Y'
#else
			'N'
#endif
	    );
}

static DRIVER_ATTR(wrap_y, S_IRUGO, wrap_y_show, NULL);

static ssize_t force_update_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_HAS_RESET_PIN: %c\n",
#ifdef CFG_CTS_FIRMWARE_FORCE_UPDATE
			'Y'
#else
			'N'
#endif
	    );
}

static DRIVER_ATTR(force_update, S_IRUGO, force_update_show, NULL);

static ssize_t max_touch_num_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_MAX_TOUCH_NUM: %d\n", CFG_CTS_MAX_TOUCH_NUM);
}

static DRIVER_ATTR(max_touch_num, S_IRUGO, max_touch_num_show, NULL);

static ssize_t vkey_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CONFIG_CTS_VIRTUALKEY: %c\n",
#ifdef CONFIG_CTS_VIRTUALKEY
			'Y'
#else
			'N'
#endif
	    );
}

static DRIVER_ATTR(vkey, S_IRUGO, vkey_show, NULL);

static ssize_t gesture_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CONFIG_CTS_GESTURE: %c\n",
#ifdef CONFIG_CTS_GESTURE
			'Y'
#else
			'N'
#endif
	    );
}

static DRIVER_ATTR(gesture, S_IRUGO, gesture_show, NULL);

static ssize_t esd_protection_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CONFIG_CTS_ESD_PROTECTION: %c\n",
#ifdef CONFIG_CTS_ESD_PROTECTION
			'Y'
#else
			'N'
#endif
	    );
}

static DRIVER_ATTR(esd_protection, S_IRUGO, esd_protection_show, NULL);

static ssize_t slot_protocol_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CONFIG_CTS_SLOTPROTOCOL: %c\n",
#ifdef CONFIG_CTS_SLOTPROTOCOL
			'Y'
#else
			'N'
#endif
	    );
}

static DRIVER_ATTR(slot_protocol, S_IRUGO, slot_protocol_show, NULL);

static ssize_t i2c_xfer_size_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_MAX_I2C_XFER_SIZE: %d\n", CFG_CTS_MAX_I2C_XFER_SIZE);
}

static DRIVER_ATTR(i2c_xfer_size, S_IRUGO, i2c_xfer_size_show, NULL);

static struct attribute *cts_i2c_driver_config_attrs[] = {
	&driver_attr_reset_pin.attr,
	&driver_attr_swap_xy.attr,
	&driver_attr_wrap_x.attr,
	&driver_attr_wrap_y.attr,
	&driver_attr_force_update.attr,
	&driver_attr_max_touch_num.attr,
	&driver_attr_vkey.attr,
	&driver_attr_gesture.attr,
	&driver_attr_esd_protection.attr,
	&driver_attr_slot_protocol.attr,
	&driver_attr_i2c_xfer_size.attr,
	NULL
};

static const struct attribute_group cts_i2c_driver_config_group = {
	.name = "config",
	.attrs = cts_i2c_driver_config_attrs,
};

static const struct attribute_group *cts_i2c_driver_config_groups[] = {
	&cts_i2c_driver_config_group,
	NULL,
};
#endif /* CONFIG_CTS_SYSFS */

#ifdef CONFIG_CTS_OF
static const struct of_device_id cts_i2c_of_match_table[] = {
	{.compatible = CFG_CTS_OF_DEVICE_ID_NAME,},
	{},
};

MODULE_DEVICE_TABLE(of, cts_i2c_of_match_table);
#endif /* CONFIG_CTS_OF */

static const struct i2c_device_id cts_i2c_device_id_table[] = {
	{CFG_CTS_DEVICE_NAME, 0},
	{}
};

static struct i2c_driver cts_i2c_driver = {
	.probe = cts_i2c_driver_probe,
	.remove = cts_i2c_driver_remove,
	.driver = {
		   .name = CFG_CTS_DRIVER_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_CTS_OF
		   .of_match_table = of_match_ptr(cts_i2c_of_match_table),
#endif /* CONFIG_CTS_OF */
#ifdef CONFIG_CTS_SYSFS
		   .groups = cts_i2c_driver_config_groups,
#endif /* CONFIG_CTS_SYSFS */
#ifdef CONFIG_CTS_PM_LEGACY
		   .suspend = cts_i2c_driver_suspend,
		   .resume = cts_i2c_driver_resume,
#endif /* CONFIG_CTS_PM_LEGACY */
#ifdef CONFIG_CTS_PM_GENERIC
		   .pm = &cts_i2c_driver_pm_ops,
#endif /* CONFIG_CTS_PM_GENERIC */

		   },
	.id_table = cts_i2c_device_id_table,
};

static int __init cts_i2c_driver_init(void)
{
	cts_info("Init");

	return i2c_add_driver(&cts_i2c_driver);
}

static void __exit cts_i2c_driver_exit(void)
{
	cts_info("Exit");

	i2c_del_driver(&cts_i2c_driver);
}

module_init(cts_i2c_driver_init);
module_exit(cts_i2c_driver_exit);

MODULE_DESCRIPTION("Chipone TDDI touchscreen Driver for QualComm platform");
MODULE_VERSION(CFG_CTS_DRIVER_VERSION);
MODULE_AUTHOR("Miao Defang <dfmiao@chiponeic.com>");
MODULE_LICENSE("GPL");
