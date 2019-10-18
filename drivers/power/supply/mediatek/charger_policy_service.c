#include <linux/alarmtimer.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/bitops.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include "zte_misc.h"

#if (KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE)
#define KERNEL_ABOVE_4_1_0
#endif

struct dts_config_prop {
	u32			turn_on;
	u32			expired_mode_enable;
	u32			policy_lowtemp_enable;
	u32			retry_times;
	u32			timeout_seconds;
	int			expired_capacity_max;
	int			expired_capacity_min;
	int			demo_capacity_max;
	int			demo_capacity_min;
	const char	*bms_phy_name;
	const char	*battery_phy_name;
};

struct charger_policy_info {
	struct device				*dev;
	struct power_supply		*bms_psy;
	struct power_supply		*battery_psy;
	struct power_supply		*usb_psy;
	struct notifier_block		nb;
	struct alarm				timeout_timer;
	struct workqueue_struct	*timeout_workqueue;
	struct delayed_work		timeout_work;
	struct workqueue_struct	*policy_probe_wq;
	struct delayed_work		policy_probe_work;
	struct mutex				timer_lock;
	struct dts_config_prop		config_prop;
	ktime_t					timer_interval;
	int						usb_present;
	struct alarm				charging_demo_alarm;
	struct charging_policy_ops   battery_charging_policy_ops;
};

#define POLICY_ERR			-1
#define SEC_PER_MS			1000ULL

#define pr_policy(fmt, args...)	pr_info("policy: %s(): "fmt, __func__, ## args)

#define OF_READ_PROPERTY(store, dt_property, retval, default_val)	\
do {											\
	retval = of_property_read_u32(np,			\
					"policy," dt_property,		\
					&store);					\
											\
	if (retval == -EINVAL) {					\
		retval = 0;							\
		store = default_val;					\
	} else if (retval) {							\
		pr_policy("Error reading " #dt_property	\
				" property rc = %d\n", retval);	\
	}										\
	pr_policy("config: " #dt_property				\
				" property: [%d]\n", store);		\
} while (0)

#define OF_READ_PROPERTY_STRINGS(store, dt_property, retval, default_val)	\
do {											\
	retval = of_property_read_string(np,		\
					"policy," dt_property,		\
					&(store));				\
											\
	if (retval == -EINVAL) {					\
		retval = 0;							\
		store = default_val;					\
	} else if (retval) {							\
		pr_policy("Error reading " #dt_property	\
				" property rc = %d\n", retval);	\
		return retval;						\
	}										\
	pr_policy("config: " #dt_property				\
				" property: [%s]\n", store);		\
} while (0)

#define OF_READ_ARRAY_PROPERTY(prop_data, prop_length, prop_size, dt_property, retval) \
do { \
	if (of_find_property(np, "policy," dt_property, &prop_length)) { \
		prop_data = kzalloc(prop_length, GFP_KERNEL); \
		retval = of_property_read_u32_array(np, "policy," dt_property, \
				 (u32 *)prop_data, prop_length / sizeof(u32)); \
		if (retval) { \
			retval = -EINVAL; \
			pr_policy("Error reading " #dt_property \
				" property rc = %d\n", retval); \
			kfree(prop_data); \
			prop_data = NULL; \
			prop_length = 0; \
			return retval; \
		} else { \
			prop_length = prop_length / prop_size; \
		} \
	} else { \
		retval = -EINVAL; \
		prop_data = NULL; \
		prop_length = 0; \
		pr_policy("Error geting " #dt_property \
				" property rc = %d\n", retval); \
		return retval; \
	} \
	pr_policy("config: " #dt_property \
				" prop_length: [%d]\n", prop_length);\
} while (0)

static bool charger_policy_status = false;
extern bool is_kernel_power_off_charging(void);

static void charger_policy_timer_reset(struct charger_policy_info *policy_info)
{
	u64 timeout_ms = 0;

	if (!policy_info) {
		return;
	}

	mutex_lock(&policy_info->timer_lock);

	timeout_ms = policy_info->config_prop.timeout_seconds * SEC_PER_MS;
	alarm_try_to_cancel(&policy_info->timeout_timer);
	alarm_start_relative(&policy_info->timeout_timer, ms_to_ktime(timeout_ms));

	mutex_unlock(&policy_info->timer_lock);
}

static void charger_policy_timer_disable(struct charger_policy_info *policy_info)
{
	if (!policy_info) {
		return;
	}

	mutex_lock(&policy_info->timer_lock);

	alarm_try_to_cancel(&policy_info->timeout_timer);

	mutex_unlock(&policy_info->timer_lock);
}

static int charger_policy_parse_dt(struct charger_policy_info *policy_info)
{
	int retval = 0;
	struct device_node *np = policy_info->dev->of_node;

	OF_READ_PROPERTY(policy_info->config_prop.turn_on,
			"enable", retval, 0);

	OF_READ_PROPERTY(policy_info->config_prop.expired_mode_enable,
			"expired-mode-enable", retval, 0);

	OF_READ_PROPERTY(policy_info->config_prop.policy_lowtemp_enable,
			"policy-lowtemp-enable", retval, 1);

	OF_READ_PROPERTY(policy_info->config_prop.retry_times,
			"retry-times", retval, 10);

	OF_READ_PROPERTY(policy_info->config_prop.timeout_seconds,
			"timeout-seconds", retval, CHARGING_EXPIRATION_TIME_MS/SEC_PER_MS);

	OF_READ_PROPERTY(policy_info->config_prop.expired_capacity_max,
			"max-capacity", retval, 70);

	OF_READ_PROPERTY(policy_info->config_prop.expired_capacity_min,
			"min-capacity", retval, 50);

	OF_READ_PROPERTY(policy_info->config_prop.demo_capacity_max,
			"demo-max-capacity", retval, 70);

	OF_READ_PROPERTY(policy_info->config_prop.demo_capacity_min,
			"demo-min-capacity", retval, 50);

	OF_READ_PROPERTY_STRINGS(policy_info->config_prop.battery_phy_name,
			"battery-phy-name", retval, "battery");

	OF_READ_PROPERTY_STRINGS(policy_info->config_prop.bms_phy_name,
			"bms-phy-name", retval, "bms");


	return 0;
}

static int charger_policy_charging_enabled(struct charger_policy_info *policy_info, bool flag)
{
	union power_supply_propval charging_enabled = {0, };
	int rc = 0;

	if (!policy_info) {
		return -EINVAL;
	}

	rc = power_supply_get_property(policy_info->battery_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &charging_enabled);
	if (rc < 0) {
		pr_policy("Failed to get charging enabled property rc=%d\n", rc);
		goto failed_loop;
	}

	if (charging_enabled.intval != flag) {
		charging_enabled.intval = flag;
		pr_policy("set charging enabled to %d\n", flag);
		rc = power_supply_set_property(policy_info->battery_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &charging_enabled);
		if (rc < 0) {
			pr_policy("Failed to get present property rc=%d\n", rc);
			goto failed_loop;
		}
	} else {
		pr_policy("charging enabled status already is %d\n", flag);
	}

	return 0;

failed_loop:
	return -EINVAL;
}

static int charger_policy_battery_charging_enabled(struct charger_policy_info *policy_info, bool flag)
{
	union power_supply_propval battery_charging_enabled = {0, };
	int rc = 0;

	if (!policy_info) {
		return -EINVAL;
	}

	rc = power_supply_get_property(policy_info->battery_psy,
			POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &battery_charging_enabled);
	if (rc < 0) {
		pr_policy("Failed to get battery charging enabled property rc=%d\n", rc);
		goto failed_loop;
	}

	if (battery_charging_enabled.intval != flag) {
		battery_charging_enabled.intval = flag;
		pr_policy("set battery charging enabled to %d\n", flag);
		rc = power_supply_set_property(policy_info->battery_psy,
			POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &battery_charging_enabled);
		if (rc < 0) {
			pr_policy("Failed to get present property rc=%d\n", rc);
			goto failed_loop;
		}
	} else {
		pr_policy("battery charging enabled status already is %d\n", flag);
	}

	return 0;

failed_loop:
	return -EINVAL;
}


static int charger_policy_recovery_charging(struct charger_policy_info *policy_info)
{
	if (!policy_info) {
		return -EINVAL;
	}

	charger_policy_battery_charging_enabled(policy_info, true);

	charger_policy_charging_enabled(policy_info, true);
	pr_policy("\n");

	return 0;
}

static int charger_policy_discharging(struct charger_policy_info *policy_info)
{
	if (!policy_info) {
		return -EINVAL;
	}

	charger_policy_battery_charging_enabled(policy_info, false);

	charger_policy_charging_enabled(policy_info, false);
	pr_policy("\n");

	return 0;
}

static int charger_policy_update_usb_status(struct charger_policy_info *policy_info)
{
	union power_supply_propval usb_pressent_val = {0, };
	int rc = 0;

	if (!policy_info) {
		return -EINVAL;
	}

	rc = power_supply_get_property(policy_info->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &usb_pressent_val);
	if (rc < 0) {
		pr_policy("Failed to get present property rc=%d\n", rc);
		return POLICY_ERR;
	}

	if (!policy_info->usb_present && usb_pressent_val.intval) {
		policy_info->usb_present = usb_pressent_val.intval;
		if (policy_info->config_prop.expired_mode_enable)
			charger_policy_timer_reset(policy_info);
		pr_policy("usb_insert\n");
		if (policy_info->battery_charging_policy_ops.charging_policy_status & DEMO_CHARGING_POLICY) {
			alarm_try_to_cancel(&policy_info->charging_demo_alarm);
			alarm_start_relative(&policy_info->charging_demo_alarm, ms_to_ktime(50));
		}
	} else if (policy_info->usb_present && !usb_pressent_val.intval) {
		pr_policy("usb_remove\n");
		policy_info->usb_present = usb_pressent_val.intval;
		charger_policy_timer_disable(policy_info);
		if (policy_info->battery_charging_policy_ops.charging_policy_status & DEMO_CHARGING_POLICY)
			alarm_try_to_cancel(&policy_info->charging_demo_alarm);
		if ((policy_info->battery_charging_policy_ops.charging_policy_status & DEMO_CHARGING_POLICY)
			|| (policy_info->battery_charging_policy_ops.charging_policy_status & EXPIRED_CHARGING_POLICY)) {
			pr_policy("usb remove recovery charging\n");
			charger_policy_recovery_charging(policy_info);
			charger_policy_status = false;
		}

		policy_info->battery_charging_policy_ops.charging_policy_status &= ~EXPIRED_CHARGING_POLICY;
	}

	return 0;
}

static int charger_policy_notifier_handler(struct charger_policy_info *policy_info)
{
	int rc = 0;
	union power_supply_propval capacity_val = {0, };
	union power_supply_propval val_pre = {0, };
	union power_supply_propval temp_val = {0, };

	if (!policy_info) {
		return -EINVAL;
	}

	rc = power_supply_get_property(policy_info->bms_psy,
			POWER_SUPPLY_PROP_CAPACITY, &capacity_val);
	if (rc < 0) {
		pr_policy("Failed to get capacity property rc=%d\n", rc);
		goto failed_loop;
	}

	rc = power_supply_get_property(policy_info->battery_psy,
				POWER_SUPPLY_PROP_CHARGING_ENABLED, &val_pre);
	if (rc < 0) {
		pr_policy("Failed to get present property rc=%d\n", rc);
	}

	rc = power_supply_get_property(policy_info->bms_psy,
				POWER_SUPPLY_PROP_TEMP, &temp_val);
	if (rc < 0) {
		pr_policy("Failed to get battery temp property rc=%d\n", rc);
	}

	pr_policy("soc=%d, temp=%d, policy_lowtemp_en=%d, charging_policy_status=0x%x,"
		"expired[%d,%d] demo[%d,%d], old status: %s charging, battery_status=%d\n",
		capacity_val.intval, temp_val.intval, policy_info->config_prop.policy_lowtemp_enable,
		policy_info->battery_charging_policy_ops.charging_policy_status,
		policy_info->config_prop.expired_capacity_min,
		policy_info->config_prop.expired_capacity_max,
		policy_info->config_prop.demo_capacity_min,
		policy_info->config_prop.demo_capacity_max,
		val_pre.intval ? "enable" : "disable",
		policy_info->battery_charging_policy_ops.battery_status);
	pm_stay_awake(policy_info->dev);
#ifdef ZTE_CHARGER_TYPE_OEM
	if ((policy_info->battery_charging_policy_ops.charging_policy_status & DEMO_CHARGING_POLICY)) {
		charger_policy_status = true;
		if (capacity_val.intval >= policy_info->config_prop.demo_capacity_max) {
			if (val_pre.intval == 1) {
				if (policy_info->config_prop.policy_lowtemp_enable == 0 &&
					temp_val.intval <= CHARGING_POLICY_LOWTEMP_THRESHOLD)
					pr_policy("policy_lowtemp_en disabled, keep power supply\n");
				else
					charger_policy_discharging(policy_info);
			}
			policy_info->battery_charging_policy_ops.battery_status = BATTERY_DISCHARGING;
		} else if (capacity_val.intval <= policy_info->config_prop.demo_capacity_min) {
			if (val_pre.intval == 0)
				charger_policy_recovery_charging(policy_info);
			policy_info->battery_charging_policy_ops.battery_status = BATTERY_CHARGING;
		} else if (policy_info->battery_charging_policy_ops.battery_status == BATTERY_DISCHARGING) {
			if (val_pre.intval == 1) {
				if (policy_info->config_prop.policy_lowtemp_enable == 0 &&
					temp_val.intval <= CHARGING_POLICY_LOWTEMP_THRESHOLD)
					pr_policy("policy_lowtemp_en disabled, keep power supply\n");
				else
					charger_policy_discharging(policy_info);
			}
		}

		if (policy_info->config_prop.policy_lowtemp_enable == 0 &&
			temp_val.intval <= CHARGING_POLICY_LOWTEMP_THRESHOLD) {
			if (val_pre.intval == 0) {
				pr_policy("#policy_lowtemp_en disabled, keep power supply\n");
				charger_policy_recovery_charging(policy_info);
			}
		}
	} else if ((policy_info->battery_charging_policy_ops.charging_policy_status
					& EXPIRED_CHARGING_POLICY)) {
		charger_policy_status = true;
		if (capacity_val.intval >= policy_info->config_prop.expired_capacity_max) {
			if (val_pre.intval == 1) {
				if (policy_info->config_prop.policy_lowtemp_enable == 0 &&
					temp_val.intval <= CHARGING_POLICY_LOWTEMP_THRESHOLD)
					pr_policy("policy_lowtemp_en disabled, keep power supply\n");
				else
					charger_policy_discharging(policy_info);
			}
			policy_info->battery_charging_policy_ops.battery_status = BATTERY_DISCHARGING;
		} else if (capacity_val.intval <= policy_info->config_prop.expired_capacity_min) {
			if (val_pre.intval == 0)
				charger_policy_recovery_charging(policy_info);
			policy_info->battery_charging_policy_ops.battery_status = BATTERY_CHARGING;
		}

		if (policy_info->config_prop.policy_lowtemp_enable == 0 &&
			temp_val.intval <= CHARGING_POLICY_LOWTEMP_THRESHOLD) {
			if (val_pre.intval == 0) {
				pr_policy("#policy_lowtemp_en disabled, keep power supply\n");
				charger_policy_recovery_charging(policy_info);
			}
		}
	} else {
		charger_policy_status = false;
		charger_policy_recovery_charging(policy_info);
		policy_info->battery_charging_policy_ops.battery_status = BATTERY_CHARGING;
	}
#else
	if ((policy_info->battery_charging_policy_ops.charging_policy_status & DEMO_CHARGING_POLICY)
		|| (policy_info->battery_charging_policy_ops.charging_policy_status & EXPIRED_CHARGING_POLICY)) {
		charger_policy_status = true;
		if (capacity_val.intval >= policy_info->config_prop.expired_capacity_max) {
			if (val_pre.intval == 1) {
				if (policy_info->config_prop.policy_lowtemp_enable == 0 &&
					temp_val.intval <= CHARGING_POLICY_LOWTEMP_THRESHOLD)
					pr_policy("policy_lowtemp_en disabled, keep power supply\n");
				else
					charger_policy_discharging(policy_info);
			}
			policy_info->battery_charging_policy_ops.battery_status = BATTERY_DISCHARGING;
		} else if (capacity_val.intval <= policy_info->config_prop.expired_capacity_min) {
			if (val_pre.intval == 0)
				charger_policy_recovery_charging(policy_info);
			policy_info->battery_charging_policy_ops.battery_status = BATTERY_CHARGING;
		} else if (policy_info->battery_charging_policy_ops.battery_status == BATTERY_DISCHARGING) {
			if (val_pre.intval == 1) {
				if (policy_info->config_prop.policy_lowtemp_enable == 0 &&
					temp_val.intval <= CHARGING_POLICY_LOWTEMP_THRESHOLD)
					pr_policy("policy_lowtemp_en disabled, keep power supply\n");
				else
					charger_policy_discharging(policy_info);
			}
		}

		if (policy_info->config_prop.policy_lowtemp_enable == 0 &&
			temp_val.intval <= CHARGING_POLICY_LOWTEMP_THRESHOLD) {
			if (val_pre.intval == 0) {
				pr_policy("#policy_lowtemp_en disabled, keep power supply\n");
				charger_policy_recovery_charging(policy_info);
			}
		}
	} else {
		charger_policy_status = false;
		charger_policy_recovery_charging(policy_info);
		policy_info->battery_charging_policy_ops.battery_status = BATTERY_CHARGING;
	}
#endif
	charger_policy_update_usb_status(policy_info);
	pm_relax(policy_info->dev);

	return 0;

failed_loop:
	return -EINVAL;
}

bool charger_policy_get_status(void)
{
	pr_policy("charger get policy status = %d\n", charger_policy_status);
	return charger_policy_status;
}

static int charger_policy_notifier_switch(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct charger_policy_info *policy_info = container_of(nb, struct charger_policy_info, nb);
	const char *name = NULL;
	union power_supply_propval usb_pressent_val = {0, };
	int rc = 0;

	if (event != PSY_EVENT_PROP_CHANGED) {
		return NOTIFY_DONE;
	}

	if (delayed_work_pending(&policy_info->timeout_work))
		return NOTIFY_OK;

#ifdef KERNEL_ABOVE_4_1_0
	name = psy->desc->name;
#else
	name = psy->name;
#endif

	if ((strcmp(name, policy_info->config_prop.battery_phy_name) == 0)
		|| (strcmp(name, "usb") == 0)) {
		/* pr_policy("Notify, update status\n"); */
		rc = power_supply_get_property(policy_info->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &usb_pressent_val);

		if (rc < 0) {
			pr_policy("Failed to get present property rc=%d\n", rc);
			return POLICY_ERR;
		}
		if (policy_info->usb_present ^ usb_pressent_val.intval) {
			queue_delayed_work(policy_info->timeout_workqueue, &policy_info->timeout_work, 0);
		}
	}

	return NOTIFY_OK;
}

static int charger_policy_register_notifier(struct charger_policy_info *policy_info)
{
	int rc = 0;

	if (!policy_info) {
		return -EINVAL;
	}

	policy_info->nb.notifier_call = charger_policy_notifier_switch;

	rc = power_supply_reg_notifier(&policy_info->nb);
	if (rc < 0) {
		pr_policy("Couldn't register psy notifier rc = %d\n", rc);
		return -EINVAL;
	}

	return 0;
}

static int charger_policy_check_retry(struct charger_policy_info *policy_info)
{
	static int probe_count = 0;

	if (!policy_info) {
		return -EINVAL;
	}

	probe_count++;
	if (probe_count < policy_info->config_prop.retry_times) {
		pr_policy("charger policy driver retry[%d]!!!\n", probe_count);
		queue_delayed_work(policy_info->policy_probe_wq, &policy_info->policy_probe_work,
							msecs_to_jiffies(1000));
		return true;
	}

	return false;
}

static void charger_policy_timeout_handler_work(struct work_struct *work)
{
	struct charger_policy_info *policy_info =
			container_of(work, struct charger_policy_info, timeout_work.work);

	charger_policy_notifier_handler(policy_info);
}

enum alarmtimer_restart charger_policy_expired_alarm_cb(struct alarm *alarm,
							ktime_t now)
{
	struct charger_policy_info *policy_info = container_of(alarm, struct charger_policy_info,
					timeout_timer);

	pr_policy("\n");

	policy_info->battery_charging_policy_ops.charging_policy_status &= ~NORMAL_CHARGING_POLICY;
	policy_info->battery_charging_policy_ops.charging_policy_status |= EXPIRED_CHARGING_POLICY;

	alarm_forward_now(alarm, ms_to_ktime(CHARGING_POLICY_PERIOD));

	if (delayed_work_pending(&policy_info->timeout_work))
		return ALARMTIMER_RESTART;

	queue_delayed_work(policy_info->timeout_workqueue, &policy_info->timeout_work, msecs_to_jiffies(0));

	return ALARMTIMER_RESTART;
}

enum alarmtimer_restart charger_policy_demo_alarm_cb(struct alarm *alarm,
							ktime_t now)
{
	struct charger_policy_info *policy_info = container_of(alarm, struct charger_policy_info,
					charging_demo_alarm);

	pr_policy("\n");

	policy_info->battery_charging_policy_ops.charging_policy_status &= ~NORMAL_CHARGING_POLICY;
	policy_info->battery_charging_policy_ops.charging_policy_status |= DEMO_CHARGING_POLICY;

	alarm_forward_now(alarm, ms_to_ktime(CHARGING_POLICY_PERIOD));

	if (delayed_work_pending(&policy_info->timeout_work))
		return ALARMTIMER_RESTART;

	queue_delayed_work(policy_info->timeout_workqueue, &policy_info->timeout_work, msecs_to_jiffies(0));

	return ALARMTIMER_RESTART;
}


int charger_policy_demo_sts_set(struct charging_policy_ops *charging_policy, bool enable)
{
	struct charger_policy_info *policy_info = container_of(charging_policy, struct charger_policy_info,
					battery_charging_policy_ops);

	pr_policy("charging_policy_status = 0x%x, enable = %d\n",
		 policy_info->battery_charging_policy_ops.charging_policy_status, enable);

	if (enable) {
		policy_info->battery_charging_policy_ops.charging_policy_status &= ~NORMAL_CHARGING_POLICY;
		policy_info->battery_charging_policy_ops.charging_policy_status |= DEMO_CHARGING_POLICY;
	} else {
		policy_info->battery_charging_policy_ops.charging_policy_status &= ~DEMO_CHARGING_POLICY;
		if ((policy_info->battery_charging_policy_ops.charging_policy_status & EXPIRED_CHARGING_POLICY)
			!= EXPIRED_CHARGING_POLICY)
			policy_info->battery_charging_policy_ops.charging_policy_status |= NORMAL_CHARGING_POLICY;
	}

	pr_policy("charging_policy_status = 0x%x\n",
		policy_info->battery_charging_policy_ops.charging_policy_status);

	queue_delayed_work(policy_info->timeout_workqueue, &policy_info->timeout_work, msecs_to_jiffies(0));

	if (enable) {
		alarm_start_relative(&policy_info->charging_demo_alarm, ms_to_ktime(CHARGING_POLICY_PERIOD));
	} else {
		alarm_try_to_cancel(&policy_info->charging_demo_alarm);
	}

	return 0;
}

int charger_policy_demo_sts_get(struct charging_policy_ops *charging_policy)
{
	struct charger_policy_info *policy_info = container_of(charging_policy, struct charger_policy_info,
					battery_charging_policy_ops);

	pr_policy("charging_policy_status = 0x%x\n",
		policy_info->battery_charging_policy_ops.charging_policy_status);

	if ((policy_info->battery_charging_policy_ops.charging_policy_status & DEMO_CHARGING_POLICY)
		== DEMO_CHARGING_POLICY)
		return 1;
	else
		return 0;
}

int charger_policy_expired_mode_set(struct charging_policy_ops *charging_policy, bool enable)
{
	struct charger_policy_info *policy_info = container_of(charging_policy, struct charger_policy_info,
					battery_charging_policy_ops);

	if (enable) {
		policy_info->config_prop.expired_mode_enable = 1;
		charger_policy_timer_reset(policy_info);
	} else {
		policy_info->config_prop.expired_mode_enable = 0;
		policy_info->battery_charging_policy_ops.charging_policy_status &= ~EXPIRED_CHARGING_POLICY;
		charger_policy_timer_disable(policy_info);
	}

	pr_policy("charging_policy_status = 0x%x, set expired_mode_enable = %d\n",
			 policy_info->battery_charging_policy_ops.charging_policy_status, enable);
	queue_delayed_work(policy_info->timeout_workqueue, &policy_info->timeout_work, msecs_to_jiffies(0));

	return 0;
}

int charger_policy_expired_mode_get(struct charging_policy_ops *charging_policy)
{
	struct charger_policy_info *policy_info = container_of(charging_policy, struct charger_policy_info,
					battery_charging_policy_ops);

	pr_policy("expired_mode_enable = 0x%x\n",
		policy_info->config_prop.expired_mode_enable);

	return policy_info->config_prop.expired_mode_enable;
}

int charger_policy_expired_sts_get(struct charging_policy_ops *charging_policy)
{
	struct charger_policy_info *policy_info = container_of(charging_policy, struct charger_policy_info,
					battery_charging_policy_ops);

	pr_policy("charging_policy_status = 0x%x\n",
		policy_info->battery_charging_policy_ops.charging_policy_status);

	if ((policy_info->battery_charging_policy_ops.charging_policy_status & EXPIRED_CHARGING_POLICY)
		== EXPIRED_CHARGING_POLICY)
		return 1;
	else
		return 0;
}

int charger_policy_expired_sec_set(struct charging_policy_ops *charging_policy, int sec)
{
	struct charger_policy_info *policy_info = container_of(charging_policy, struct charger_policy_info,
					battery_charging_policy_ops);

	if (!charging_policy) {
		return -EINVAL;
	}

	policy_info->config_prop.timeout_seconds = sec;
	if (policy_info->config_prop.expired_mode_enable)
		charger_policy_timer_reset(policy_info);
	pr_policy("sec = %d\n", policy_info->config_prop.timeout_seconds);
	return 0;
}

int charger_policy_expired_sec_get(struct charging_policy_ops *charging_policy)
{
	struct charger_policy_info *policy_info = container_of(charging_policy, struct charger_policy_info,
					battery_charging_policy_ops);

	if (!charging_policy) {
		return -EINVAL;
	}

	pr_policy("\n");
	return policy_info->config_prop.timeout_seconds;
}

int charger_policy_lowtemp_en_set(struct charging_policy_ops *charging_policy, bool enable)
{
	struct charger_policy_info *policy_info = container_of(charging_policy, struct charger_policy_info,
					battery_charging_policy_ops);

	if (!charging_policy) {
		return -EINVAL;
	}

	policy_info->config_prop.policy_lowtemp_enable = enable;
	charger_policy_notifier_handler(policy_info);
	pr_policy("policy_lowtemp_enable = %d\n", policy_info->config_prop.policy_lowtemp_enable);
	return 0;
}

int charger_policy_lowtemp_en_get(struct charging_policy_ops *charging_policy)
{
	struct charger_policy_info *policy_info = container_of(charging_policy, struct charger_policy_info,
					battery_charging_policy_ops);

	if (!charging_policy) {
		return -EINVAL;
	}

	pr_policy("\n");
	return policy_info->config_prop.policy_lowtemp_enable;
}

int charger_policy_cap_min_set(struct charging_policy_ops *charging_policy, int cap)
{
	struct charger_policy_info *policy_info = container_of(charging_policy, struct charger_policy_info,
					battery_charging_policy_ops);

	if (!charging_policy) {
		return -EINVAL;
	}
	policy_info->config_prop.demo_capacity_min = cap;
	policy_info->config_prop.expired_capacity_min = cap;
	pr_policy("expired_capacity_min = %d\n", policy_info->config_prop.expired_capacity_min);
	return 0;
}

int charger_policy_cap_min_get(struct charging_policy_ops *charging_policy)
{
	struct charger_policy_info *policy_info = container_of(charging_policy, struct charger_policy_info,
					battery_charging_policy_ops);

	if (!charging_policy) {
		return -EINVAL;
	}

	pr_policy("\n");
	return policy_info->config_prop.expired_capacity_min;
}

int charger_policy_cap_max_set(struct charging_policy_ops *charging_policy, int cap)
{
	struct charger_policy_info *policy_info = container_of(charging_policy, struct charger_policy_info,
					battery_charging_policy_ops);

	if (!charging_policy) {
		return -EINVAL;
	}
	policy_info->config_prop.demo_capacity_max = cap;
	policy_info->config_prop.expired_capacity_max = cap;
	pr_policy("expired_capacity_max = %d\n", policy_info->config_prop.expired_capacity_max);
	return 0;
}

int charger_policy_cap_max_get(struct charging_policy_ops *charging_policy)
{
	struct charger_policy_info *policy_info = container_of(charging_policy, struct charger_policy_info,
					battery_charging_policy_ops);

	if (!charging_policy) {
		return -EINVAL;
	}

	pr_policy("\n");
	return policy_info->config_prop.expired_capacity_max;
}

int charger_policy_init(struct charger_policy_info *policy_info)
{
	if (!policy_info) {
		return -EINVAL;
	}

	pr_policy("\n");
	policy_info->battery_charging_policy_ops.battery_status = BATTERY_CHARGING;
	policy_info->battery_charging_policy_ops.charging_policy_status = DEFAULT_CHARGING_POLICY;
	policy_info->battery_charging_policy_ops.charging_policy_demo_sts_set = charger_policy_demo_sts_set;
	policy_info->battery_charging_policy_ops.charging_policy_demo_sts_get = charger_policy_demo_sts_get;
	policy_info->battery_charging_policy_ops.charging_policy_expired_mode_set = charger_policy_expired_mode_set;
	policy_info->battery_charging_policy_ops.charging_policy_expired_mode_get = charger_policy_expired_mode_get;
	policy_info->battery_charging_policy_ops.charging_policy_expired_sts_get = charger_policy_expired_sts_get;
	policy_info->battery_charging_policy_ops.charging_policy_expired_sec_set = charger_policy_expired_sec_set;
	policy_info->battery_charging_policy_ops.charging_policy_expired_sec_get = charger_policy_expired_sec_get;
	policy_info->battery_charging_policy_ops.charging_policy_lowtemp_en_set = charger_policy_lowtemp_en_set;
	policy_info->battery_charging_policy_ops.charging_policy_lowtemp_en_get = charger_policy_lowtemp_en_get;
	policy_info->battery_charging_policy_ops.charging_policy_cap_min_set = charger_policy_cap_min_set;
	policy_info->battery_charging_policy_ops.charging_policy_cap_min_get = charger_policy_cap_min_get;
	policy_info->battery_charging_policy_ops.charging_policy_cap_max_set = charger_policy_cap_max_set;
	policy_info->battery_charging_policy_ops.charging_policy_cap_max_get = charger_policy_cap_max_get;

	return 0;
}

static void charger_policy_probe_work(struct work_struct *work)
{
	struct charger_policy_info *policy_info =
			container_of(work, struct charger_policy_info, policy_probe_work.work);

	pr_policy("charge policy driver init begin\n");
	policy_info->bms_psy = power_supply_get_by_name(policy_info->config_prop.bms_phy_name);
	if (!policy_info->bms_psy) {
		pr_policy("Get bms psy failed\n");
		goto bms_psy_failed;
	}

	policy_info->battery_psy = power_supply_get_by_name(policy_info->config_prop.battery_phy_name);
	if (!policy_info->battery_psy) {
		pr_policy("Get battery psy failed\n");
		goto battery_psy_failed;
	}

	policy_info->usb_psy = power_supply_get_by_name("usb");
	if (!policy_info->usb_psy) {
		pr_policy("Get usb psy failed\n");
		goto usb_psy_failed;
	}

	mutex_init(&policy_info->timer_lock);

	alarm_init(&policy_info->timeout_timer, ALARM_BOOTTIME, charger_policy_expired_alarm_cb);
	alarm_init(&policy_info->charging_demo_alarm, ALARM_BOOTTIME, charger_policy_demo_alarm_cb);
	charger_policy_init(policy_info);
	zte_misc_register_charging_policy_ops(&policy_info->battery_charging_policy_ops);

	policy_info->timeout_workqueue = create_singlethread_workqueue("charger-policy-service");
	INIT_DELAYED_WORK(&policy_info->timeout_work, charger_policy_timeout_handler_work);

	if (charger_policy_register_notifier(policy_info) < 0) {
		pr_policy("init register notifier info failed\n");
		goto register_notifier_failed;
	}

	queue_delayed_work(policy_info->timeout_workqueue, &policy_info->timeout_work, msecs_to_jiffies(0));

	pr_policy("charge policy driver init finished\n");

	return;

register_notifier_failed:
	mutex_destroy(&policy_info->timer_lock);
usb_psy_failed:
#ifdef KERNEL_ABOVE_4_1_0
	power_supply_put(policy_info->battery_psy);
#endif
	policy_info->battery_psy =  NULL;
battery_psy_failed:
#ifdef KERNEL_ABOVE_4_1_0
	power_supply_put(policy_info->bms_psy);
#endif
	policy_info->bms_psy =  NULL;
bms_psy_failed:
	if (charger_policy_check_retry(policy_info) == true) {
		return;
	}

	devm_kfree(policy_info->dev, policy_info);

	pr_policy("Charge Arbitrate Driver Init Failed!!!\n");
}

static int charger_policy_probe(struct platform_device *pdev)
{
	struct charger_policy_info *policy_info;

	pr_policy("charge policy driver probe begin\n");

	policy_info = devm_kzalloc(&pdev->dev, sizeof(*policy_info), GFP_KERNEL);
	if (!policy_info) {
		pr_policy("devm_kzalloc failed\n");
		return -ENOMEM;
	}

	policy_info->dev = &pdev->dev;

	platform_set_drvdata(pdev, policy_info);

	if (charger_policy_parse_dt(policy_info) < 0) {
		pr_policy("Parse dts failed\n");
		goto parse_dt_failed;
	}

#ifdef ZTE_FEATURE_PV_AR
	goto parse_dt_failed;
#else
	if (!policy_info->config_prop.turn_on || is_kernel_power_off_charging()) {
		pr_policy("charge policy disabled, please config policy,enable\n");
		goto parse_dt_failed;
	}
#endif

	policy_info->policy_probe_wq = create_singlethread_workqueue("policy_probe_wq");

	INIT_DELAYED_WORK(&policy_info->policy_probe_work, charger_policy_probe_work);

	queue_delayed_work(policy_info->policy_probe_wq, &policy_info->policy_probe_work, msecs_to_jiffies(0));

	pr_policy("charge policy driver probe finished\n");

	return 0;

parse_dt_failed:
	devm_kfree(policy_info->dev, policy_info);
	policy_info = NULL;

	pr_policy("charger policy driver failed\n");

	return 0;
}

static int charger_policy_remove(struct platform_device *pdev)
{
	struct charger_policy_info *policy_info = platform_get_drvdata(pdev);

	pr_policy("charger policy driver remove begin\n");

	if (policy_info == NULL) {
		goto ExitLoop;
	}

	power_supply_unreg_notifier(&policy_info->nb);

#ifdef KERNEL_ABOVE_4_1_0
	power_supply_put(policy_info->bms_psy);

	power_supply_put(policy_info->battery_psy);
	power_supply_put(policy_info->usb_psy);
#endif
	devm_kfree(policy_info->dev, policy_info);
	policy_info = NULL;

ExitLoop:
	pr_policy("charger policy driver remove finished\n");

	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "zte,charger-policy-service", },
	{ },
};

static struct platform_driver charger_policy_driver = {
	.driver		= {
		.name		= "zte,charger-policy-service",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
	},
	.probe		= charger_policy_probe,
	.remove		= charger_policy_remove,
};

module_platform_driver(charger_policy_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zte.charger <zte.charger@zte.com>");
MODULE_DESCRIPTION("Charge policy Service Driver");
