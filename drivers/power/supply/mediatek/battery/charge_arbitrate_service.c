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
/*#include <linux/wakelock.h>*/

#if (KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE)
#define KERNEL_ABOVE_4_1_0
#endif

#ifndef POWER_SUPPLY_PROP_CHARGE_VOLTAGE_MAX_ZTE
#define POWER_SUPPLY_PROP_CHARGE_VOLTAGE_MAX_ZTE POWER_SUPPLY_PROP_VOLTAGE_MAX
#endif

#ifndef POWER_SUPPLY_PROP_CHARGE_CURRENT_MAX_ZTE
#define POWER_SUPPLY_PROP_CHARGE_CURRENT_MAX_ZTE POWER_SUPPLY_PROP_CURRENT_MAX
#endif

#ifndef POWER_SUPPLY_PROP_RECHARGE_SOC_ZTE
#define POWER_SUPPLY_PROP_RECHARGE_SOC_ZTE POWER_SUPPLY_PROP_RECHARGE_SOC
#endif

#ifndef POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT_ZTE
#define POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT_ZTE POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT
#endif

#ifndef POWER_SUPPLY_PROP_RECHARGE_VOLTAGE_ZTE
#define POWER_SUPPLY_PROP_RECHARGE_VOLTAGE_ZTE POWER_SUPPLY_PROP_RECHARGE_SOC
#endif

#ifndef POWER_SUPPLY_PROP_CHARGE_CC_TO_CV_ZTE
#define POWER_SUPPLY_PROP_CHARGE_CC_TO_CV_ZTE POWER_SUPPLY_PROP_CALIBRATE
#endif

#ifndef POWER_SUPPLY_PROP_CAPACITY_RAW_ZTE
#define POWER_SUPPLY_PROP_CAPACITY_RAW_ZTE POWER_SUPPLY_PROP_CAPACITY_RAW
#endif

struct battery_age_prop {
	u32			age;
	u32			age_fcv;
	u32			age_fcc;
	u32			age_rech_soc_1st;
	u32			age_rech_soc_2st;
	u32			age_rech_voltage_1st;
	u32			age_rech_voltage_2st;
};

struct dts_config_prop {
	u32			turn_on;
	u32			retry_times;
	u32			timeout_seconds;
	u32			force_full_aging;
	int			temp_warm;
	int			temp_warm_recov;
	int			temp_cool;
	int			temp_cool_recov;
	int			normal_topoff;
	int			abnormal_topoff;
	int			full_raw_soc;
	u32			age_step;
	const char	*subsidiary_psy_name;
	const char	*battery_phy_name;
	const char	*bcl_phy_name;
	const char	*interface_phy_name;
	struct battery_age_prop	*age_data;
};

enum {
	CHARGE_STATUS_CHARGE_START_C1,
	CHARGE_STATUS_CHARGE_RUNNING_C2,
	CHARGE_STATUS_CHARGE_SAFE_MODE_C3,
	CHARGE_STATUS_RECHARGE_START_R1,
	CHARGE_STATUS_RECHARGE_RUNNING_R2,
	CHARGE_STATUS_RECHARGE_SAFE_MODE_R3,
	CHARGE_STATUS_RECHARGE_PLUS_START_P1,
	CHARGE_STATUS_RECHARGE_PLUS_RUNNING_P2,
	CHARGE_STATUS_MAX,
};

enum safe_mode_state {
	TEMP_SAFE_MODE_NORMAL = 0,
	TEMP_SAFE_MODE_COOL,
	TEMP_SAFE_MODE_WARM,
};

struct cas_run_prop {
	int			age_index;
	u32			run_fcv;
	u32			run_fcc;
	u32			run_rech_soc;
	u32			run_rech_voltage;
	u32			topoff_current_percent;
	struct timespec time_begin;
};

struct cas_debug_prop {
	int			age;
	int			capacity;
	int			capacity_raw;
	int			temperature;
	int			charge_status;
	int			topoff_current_percent;
	int			volt_ocv;
	int			volt_max;
	int			machine_status;
};

struct charge_arbitrate_info {
	struct device				*dev;
#ifdef KERNEL_ABOVE_4_1_0
	struct power_supply		*cas_psy_pointer;
#else
	struct power_supply		cas_psy;
#endif
	struct power_supply		*subsidiary_psy;
	struct power_supply		*battery_psy;
	struct power_supply		*interface_psy;
	struct notifier_block		nb;
	struct hrtimer				timeout_timer;
	struct workqueue_struct	*timeout_workqueue;
	struct delayed_work		timeout_work;
	struct workqueue_struct	*cas_probe_wq;
	struct delayed_work		cas_probe_work;
	struct mutex				data_lock;
	spinlock_t				timer_lock;
	struct dts_config_prop		config_prop;
	struct cas_run_prop		run_prop;
	struct cas_debug_prop		debug_prop;
	/*struct wake_lock			cas_wake_lock;*/
	ktime_t					timer_interval;
	u32						capacity_design;
	u32						status;
	bool						force_charger_full;
	bool						capacity_abnormal;
	bool						disable_cas;
	atomic_t					init_finished;
};

struct cas_status_list {
	u32 status;
	char *status_name;
	int (*charging)(struct charge_arbitrate_info *cas_info);
	int (*charge_full)(struct charge_arbitrate_info *cas_info);
	int (*discharge)(struct charge_arbitrate_info *cas_info);
};

#define CAPACITY_TO_CALC_TOPOFF_CURRENT		95
#define SECONDS_IN_ONE_HOUR					3600
#define SECONDS_IN_ONE_MINUTES				60
#define HOURS_IN_ONE_DAY						24

#define pr_alarm(fmt, args...)	pr_info("CAS: %s(): "fmt, __func__, ## args)

#define OF_READ_PROPERTY(store, dt_property, retval, default_val)	\
do {											\
	retval = of_property_read_u32(np,			\
					"cas," dt_property,		\
					&store);					\
											\
	if (retval == -EINVAL) {					\
		retval = 0;							\
		store = default_val;					\
	} else if (retval) {							\
		pr_alarm("Error reading " #dt_property	\
				" property rc = %d\n", retval);	\
	}										\
	pr_alarm("config: " #dt_property				\
				" property: [%d]\n", store);		\
} while (0)

#define OF_READ_PROPERTY_STRINGS(store, dt_property, retval, default_val)	\
do {											\
	retval = of_property_read_string(np,		\
					"cas," dt_property,		\
					&(store));				\
											\
	if (retval == -EINVAL) {					\
		retval = 0;							\
		store = default_val;					\
	} else if (retval) {							\
		pr_alarm("Error reading " #dt_property	\
				" property rc = %d\n", retval);	\
		return retval;						\
	}										\
	pr_alarm("config: " #dt_property				\
				" property: [%s]\n", store);		\
} while (0)

#define OF_READ_ARRAY_PROPERTY(prop_data, prop_length, prop_size, dt_property, retval) \
do { \
	if (of_find_property(np, "cas," dt_property, &prop_length)) { \
		prop_data = kzalloc(prop_length, GFP_KERNEL); \
		retval = of_property_read_u32_array(np, "cas," dt_property, \
				 (u32 *)prop_data, prop_length / sizeof(u32)); \
		if (retval) { \
			retval = -EINVAL; \
			pr_alarm("Error reading " #dt_property \
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
		pr_alarm("Error geting " #dt_property \
				" property rc = %d\n", retval); \
		return retval; \
	} \
	pr_alarm("config: " #dt_property \
				" prop_length: [%d]\n", prop_length);\
} while (0)

static int charge_arbitrate_turnoff_state_machine(struct charge_arbitrate_info *cas_info);

static void charge_arbitrate_ktime_enable(struct charge_arbitrate_info *cas_info)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&cas_info->timer_lock, flags);

	hrtimer_start(&cas_info->timeout_timer, cas_info->timer_interval, HRTIMER_MODE_REL);

	spin_unlock_irqrestore(&cas_info->timer_lock, flags);
}

static void charge_arbitrate_ktime_reset(struct charge_arbitrate_info *cas_info)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&cas_info->timer_lock, flags);

	hrtimer_cancel(&cas_info->timeout_timer);

	hrtimer_start(&cas_info->timeout_timer, cas_info->timer_interval, HRTIMER_MODE_REL);

	spin_unlock_irqrestore(&cas_info->timer_lock, flags);
}

static void charge_arbitrate_ktime_disable(struct charge_arbitrate_info *cas_info)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&cas_info->timer_lock, flags);

	hrtimer_cancel(&cas_info->timeout_timer);

	spin_unlock_irqrestore(&cas_info->timer_lock, flags);
}

static int power_supply_set_prop_by_name(const char *name, enum power_supply_property psp, int data)
{
	struct power_supply *psy = NULL;
	union power_supply_propval val = {0, };
	int rc = 0;

	if (name == NULL) {
		pr_alarm("psy name is NULL!!\n");
		goto failed_loop;
	}

	psy = power_supply_get_by_name(name);
	if (!psy) {
		pr_alarm("get %s psy failed!!\n", name);
		goto failed_loop;
	}

	val.intval = data;

	rc = power_supply_set_property(psy,
				psp, &val);
	if (rc < 0) {
		pr_alarm("Failed to set %s property:%d rc=%d\n", name, psp, rc);
		return rc;
	}

#ifdef KERNEL_ABOVE_4_1_0
	power_supply_put(psy);
#endif

	return 0;

failed_loop:
	return -EINVAL;
}

static int power_supply_get_prop_by_name(const char *name, enum power_supply_property psp, int *data)
{
	struct power_supply *psy = NULL;
	union power_supply_propval val = {0, };
	int rc = 0;

	if (name == NULL) {
		pr_alarm("psy name is NULL!!\n");
		goto failed_loop;
	}

	psy = power_supply_get_by_name(name);
	if (!psy) {
		pr_alarm("get %s psy failed!!\n", name);
		goto failed_loop;
	}

	rc = power_supply_get_property(psy,
				psp, &val);
	if (rc < 0) {
		pr_alarm("Failed to set %s property:%d rc=%d\n", name, psp, rc);
		return rc;
	}

	*data = val.intval;

#ifdef KERNEL_ABOVE_4_1_0
	power_supply_put(psy);
#endif

	return 0;

failed_loop:
	return -EINVAL;
}


static int charge_arbitrate_update_chip_status(struct charge_arbitrate_info *cas_info)
{
	static struct cas_run_prop run_prop_prev;
	int topoff_current = 0;

	/*set fcc*/
	if (cas_info->run_prop.run_fcc != run_prop_prev.run_fcc) {
		power_supply_set_prop_by_name(cas_info->config_prop.interface_phy_name,
							POWER_SUPPLY_PROP_CHARGE_CURRENT_MAX_ZTE,
							cas_info->run_prop.run_fcc);
	}

	/*set fcv*/
	if (cas_info->run_prop.run_fcv != run_prop_prev.run_fcv) {
		power_supply_set_prop_by_name(cas_info->config_prop.interface_phy_name,
							POWER_SUPPLY_PROP_CHARGE_VOLTAGE_MAX_ZTE,
							cas_info->run_prop.run_fcv);
	}

	/*set cc-cv voltage: fcv - 10mv*/
	if (cas_info->run_prop.run_fcv != run_prop_prev.run_fcv) {
		power_supply_set_prop_by_name(cas_info->config_prop.interface_phy_name,
							POWER_SUPPLY_PROP_CHARGE_CC_TO_CV_ZTE,
							cas_info->run_prop.run_fcv - 10000);
	}

	/*set recharge soc*/
	if (cas_info->run_prop.run_rech_soc != run_prop_prev.run_rech_soc) {
		power_supply_set_prop_by_name(cas_info->config_prop.interface_phy_name,
							POWER_SUPPLY_PROP_RECHARGE_SOC_ZTE,
							cas_info->run_prop.run_rech_soc);
	}

	/*set recharge voltage*/
	if (cas_info->run_prop.run_rech_voltage != run_prop_prev.run_rech_voltage) {
		power_supply_set_prop_by_name(cas_info->config_prop.interface_phy_name,
							POWER_SUPPLY_PROP_RECHARGE_VOLTAGE_ZTE,
							cas_info->run_prop.run_rech_voltage * 1000);
	}

	/*set topoff*/
	if (cas_info->run_prop.topoff_current_percent != run_prop_prev.topoff_current_percent) {
		topoff_current = (cas_info->run_prop.topoff_current_percent * cas_info->capacity_design) / 100;
		topoff_current = topoff_current / 1000;
		power_supply_set_prop_by_name(cas_info->config_prop.interface_phy_name,
							POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT_ZTE,
							topoff_current);
	}

	memcpy(&run_prop_prev, &cas_info->run_prop, sizeof(struct cas_run_prop));

	return 0;
}

static int charge_arbitrate_parse_dt(struct charge_arbitrate_info *cas_info)
{
	int retval = 0, i = 0;
	struct device_node *np = cas_info->dev->of_node;

	OF_READ_PROPERTY(cas_info->config_prop.turn_on,
			"enable", retval, 0);

	OF_READ_PROPERTY(cas_info->config_prop.retry_times,
			"retry-times", retval, 10);

	OF_READ_PROPERTY(cas_info->config_prop.timeout_seconds,
			"timeout-seconds", retval, 30);

	OF_READ_PROPERTY(cas_info->config_prop.force_full_aging,
			"force-full-aging", retval, 0);

	OF_READ_PROPERTY(cas_info->config_prop.temp_warm,
			"temperature-warm", retval, 450);

	OF_READ_PROPERTY(cas_info->config_prop.temp_warm_recov,
			"temperature-warm-recov", retval, 20);

	OF_READ_PROPERTY(cas_info->config_prop.temp_cool,
			"temperature-cool", retval, 100);

	OF_READ_PROPERTY(cas_info->config_prop.temp_cool_recov,
			"temperature-cool-recov", retval, 20);

	OF_READ_PROPERTY(cas_info->config_prop.normal_topoff,
			"normal-topoff", retval, 4);

	OF_READ_PROPERTY(cas_info->config_prop.abnormal_topoff,
			"abnormal-topoff", retval, 8);

	OF_READ_PROPERTY(cas_info->config_prop.full_raw_soc,
			"full-raw-soc", retval, 255);

	OF_READ_PROPERTY_STRINGS(cas_info->config_prop.battery_phy_name,
			"battery-phy-name", retval, "battery");

	OF_READ_PROPERTY_STRINGS(cas_info->config_prop.subsidiary_psy_name,
			"subsidiary-phy-name", retval, "bms");

	OF_READ_PROPERTY_STRINGS(cas_info->config_prop.bcl_phy_name,
			"bcl-phy-name", retval, "bcl");

	OF_READ_PROPERTY_STRINGS(cas_info->config_prop.interface_phy_name,
			"interface-phy-name", retval, "interface");

	OF_READ_ARRAY_PROPERTY(cas_info->config_prop.age_data,
								cas_info->config_prop.age_step,
								sizeof(struct battery_age_prop),
								"age-data", retval);

	for (i = 0; i < cas_info->config_prop.age_step; i++) {
		pr_alarm("age data:%d\t%d\t%d\t%d\t%d\t%d\t%d\n", cas_info->config_prop.age_data[i].age,
							cas_info->config_prop.age_data[i].age_fcv,
							cas_info->config_prop.age_data[i].age_fcc,
							cas_info->config_prop.age_data[i].age_rech_soc_1st,
							cas_info->config_prop.age_data[i].age_rech_soc_2st,
							cas_info->config_prop.age_data[i].age_rech_voltage_1st,
							cas_info->config_prop.age_data[i].age_rech_voltage_2st);
	}

	return 0;
}

static int cas_psy_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *pval)
{
	struct charge_arbitrate_info *cas_info = power_supply_get_drvdata(psy);
	int rc = 0;

	if (!cas_info || !atomic_read(&cas_info->init_finished)) {
		pr_alarm("CAS Uninitialized!!!\n");
		return -ENODATA;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		pval->intval = cas_info->capacity_abnormal;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		pval->intval = cas_info->status;
		break;
	default:
		pr_err("unsupported property %d\n", psp);
		rc = -EINVAL;
		break;
	}

	if (rc < 0)
		return -ENODATA;

	return 0;
}

static int cas_psy_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *pval)
{
	struct charge_arbitrate_info *cas_info = power_supply_get_drvdata(psy);
	int rc = 0;

	if (!cas_info || !atomic_read(&cas_info->init_finished)) {
		pr_alarm("CAS Uninitialized!!!\n");
		return -ENODATA;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		if (cas_info->disable_cas != pval->intval) {
			cas_info->disable_cas = pval->intval;
			pr_alarm("disable_cas set to %d\n", cas_info->disable_cas);
			/*Don't need check recharge when cas is disabled*/
			if (cas_info->disable_cas && cas_info->force_charger_full) {
				pr_alarm("<<<---Exit force charge full; disabled cas found!!!\n");
				cas_info->force_charger_full = false;
			}
		}
		break;
	case POWER_SUPPLY_PROP_STATUS:
		cas_info->debug_prop.charge_status = pval->intval;
		pr_alarm("charge_status %d\n",
					cas_info->debug_prop.charge_status);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		cas_info->debug_prop.age = pval->intval;
		pr_alarm("age %d\n",
					cas_info->debug_prop.age);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		cas_info->debug_prop.temperature = pval->intval;
		pr_alarm("temperature %d\n",
					cas_info->debug_prop.temperature);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		cas_info->debug_prop.capacity = pval->intval;
		pr_alarm("capacity %d\n",
					cas_info->debug_prop.capacity);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		cas_info->debug_prop.volt_ocv = pval->intval;
		pr_alarm("volt_ocv %d\n",
					cas_info->debug_prop.volt_ocv);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		cas_info->debug_prop.volt_max = pval->intval;
		pr_alarm("volt_max %d\n",
					cas_info->debug_prop.volt_max);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_RAW_ZTE:
		cas_info->debug_prop.capacity_raw = pval->intval;
		pr_alarm("capacity_raw %d\n",
					cas_info->debug_prop.capacity_raw);
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		cas_info->debug_prop.machine_status = pval->intval;
		pr_alarm("machine_status %d\n",
					cas_info->debug_prop.machine_status);
		if (!pval->intval) {
			memset(&(cas_info->debug_prop), 0, sizeof(struct cas_debug_prop));
		}
		break;
	default:
		break;
	}

#ifdef KERNEL_ABOVE_4_1_0
	power_supply_changed(cas_info->cas_psy_pointer);
#else
	power_supply_changed(&cas_info->cas_psy);
#endif

	return rc;
}

static int cas_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_AUTHENTIC:
	case POWER_SUPPLY_PROP_MANUFACTURER:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CAPACITY_RAW_ZTE:
		return 1;
	default:
		break;
	}

	return 0;
}

static void cas_external_power_changed(struct power_supply *psy)
{
	pr_debug("power supply changed\n");
}


static enum power_supply_property cas_psy_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CAPACITY_RAW_ZTE,
};

#ifdef KERNEL_ABOVE_4_1_0
static const struct power_supply_desc cas_psy_desc = {
	.name = "cas",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = cas_psy_props,
	.num_properties = ARRAY_SIZE(cas_psy_props),
	.get_property = cas_psy_get_property,
	.set_property = cas_psy_set_property,
	.external_power_changed = cas_external_power_changed,
	.property_is_writeable = cas_property_is_writeable,
};
#endif

static int charge_arbitrate_debug_battery_info(struct charge_arbitrate_info *cas_info)
{
	union power_supply_propval capacity_val = {0, };
	union power_supply_propval capacity_raw = {0, };
	union power_supply_propval voltage_ocv = {0, };
	union power_supply_propval voltage_now = {0, };
	union power_supply_propval current_val = {0, };
	union power_supply_propval batt_temp = {0, };
	int rc = 0;

	rc = power_supply_get_property(cas_info->battery_psy,
				POWER_SUPPLY_PROP_CAPACITY_RAW_ZTE, &capacity_raw);
	if (rc < 0) {
		pr_alarm("Failed to get CAPACITY_RAW property rc=%d\n", rc);
	}

	rc = power_supply_get_property(cas_info->battery_psy,
				POWER_SUPPLY_PROP_CAPACITY, &capacity_val);
	if (rc < 0) {
		pr_alarm("Failed to get CAPACITY property rc=%d\n", rc);
	}

	rc = power_supply_get_property(cas_info->battery_psy,
				POWER_SUPPLY_PROP_VOLTAGE_OCV, &voltage_ocv);
	if (rc < 0) {
		pr_alarm("Failed to get VOLTAGE_OCV property rc=%d\n", rc);
	}

	rc = power_supply_get_property(cas_info->battery_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &voltage_now);
	if (rc < 0) {
		pr_alarm("Failed to get VOLTAGE_NOW property rc=%d\n", rc);
	}

	rc = power_supply_get_property(cas_info->battery_psy,
				POWER_SUPPLY_PROP_CURRENT_NOW, &current_val);
	if (rc < 0) {
		pr_alarm("Failed to get CURRENT_NOW property rc=%d\n", rc);
	}

	rc = power_supply_get_property(cas_info->battery_psy,
					POWER_SUPPLY_PROP_TEMP, &batt_temp);
	if (rc < 0) {
		pr_alarm("Failed to get TEMP property rc=%d\n", rc);
	}

	pr_alarm("capacity: %d, capacity raw: %d, voltage ocv: %d, voltage now: %d, current: %d, batt_temp: %d\n",
						capacity_val.intval,
						capacity_raw.intval,
						voltage_ocv.intval,
						voltage_now.intval,
						current_val.intval,
						batt_temp.intval);

	return rc;
}

static int charge_arbitrate_get_age_data(struct charge_arbitrate_info *cas_info)
{
	union power_supply_propval charge_full_design = {0, };
	union power_supply_propval charge_full = {0, };
	u32 age_percent = 0;
	int rc = 0, i = 0;

	if (!cas_info->debug_prop.age) {
		rc = power_supply_get_prop_by_name(cas_info->config_prop.bcl_phy_name,
				POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &charge_full_design.intval);
		if (rc < 0) {
			pr_alarm("Failed to get CHARGE_FULL_DESIGN property rc=%d\n", rc);
			goto failed_loop;
		}

		rc = power_supply_get_prop_by_name(cas_info->config_prop.bcl_phy_name,
				POWER_SUPPLY_PROP_CHARGE_FULL, &charge_full.intval);
		if (rc < 0) {
			pr_alarm("Failed to get CHARGE_FULL property rc=%d\n", rc);
			goto failed_loop;
		}

		if (charge_full.intval == 0) {
			pr_alarm("age is zero, return\n");
			return -ENODATA;
		}

		age_percent = charge_full.intval * 100 / charge_full_design.intval;
		cas_info->capacity_design = charge_full_design.intval;
	} else {
		age_percent = cas_info->debug_prop.age;
	}

	pr_alarm("age_percent: %d; %d/%d\n", age_percent,
										charge_full.intval,
										charge_full_design.intval);

	for (i = (cas_info->config_prop.age_step - 1); i >= 0;  i--) {
		if (age_percent <= cas_info->config_prop.age_data[i].age) {
			break;
		}
	}

	i = (i < 0) ? 0 : i;
	
	i = cas_info->config_prop.force_full_aging ? 0 : i;

	return i;

failed_loop:
	return rc;
}

static int temperature_normal_status_switch(struct charge_arbitrate_info *cas_info)
{
	union power_supply_propval batt_temp = {0, };
	int rc = 0;

	if (!cas_info->debug_prop.temperature) {
		rc = power_supply_get_property(cas_info->battery_psy,
					POWER_SUPPLY_PROP_TEMP, &batt_temp);
		if (rc < 0) {
			pr_alarm("Failed to get present property rc=%d\n", rc);
			goto failed_loop;
		}
	} else {
		batt_temp.intval = cas_info->debug_prop.temperature;
	}

	if (batt_temp.intval < (cas_info->config_prop.temp_cool + cas_info->config_prop.temp_cool_recov)) {
		return TEMP_SAFE_MODE_COOL;
	} else if (batt_temp.intval > (cas_info->config_prop.temp_warm - cas_info->config_prop.temp_warm_recov)) {
		return TEMP_SAFE_MODE_WARM;
	} else {
		return TEMP_SAFE_MODE_NORMAL;
	}

failed_loop:
	return -EINVAL;
}

static int charge_arbitrate_set_input_suspend(struct charge_arbitrate_info *cas_info, bool flag)
{
	union power_supply_propval input_suspend = {0, };
	int rc = 0;

	rc = power_supply_get_property(cas_info->interface_psy,
					POWER_SUPPLY_PROP_INPUT_SUSPEND, &input_suspend);
	if (rc < 0) {
		pr_alarm("Failed to get present property rc=%d\n", rc);
		goto failed_loop;
	}

	if (input_suspend.intval != flag) {
		input_suspend.intval = flag;
		pr_alarm("set input suspend to %d\n", flag);
		rc = power_supply_set_property(cas_info->interface_psy,
					POWER_SUPPLY_PROP_INPUT_SUSPEND, &input_suspend);
		if (rc < 0) {
			pr_alarm("Failed to get present property rc=%d\n", rc);
			goto failed_loop;
		}
	} else {
		pr_alarm("input suspend status already is %d\n", flag);
	}

	return 0;

failed_loop:
	return -EINVAL;
}

static int temperature_abnormal_status_switch(struct charge_arbitrate_info *cas_info)
{
	union power_supply_propval batt_temp = {0, };
	struct cas_run_prop *run_prop_pre = NULL;
	int age_index = 0, status = 0, rc = 0;

	run_prop_pre = &cas_info->run_prop;

	if (!cas_info->debug_prop.temperature) {
		rc = power_supply_get_property(cas_info->battery_psy,
					POWER_SUPPLY_PROP_TEMP, &batt_temp);
		if (rc < 0) {
			pr_alarm("Failed to get present property rc=%d\n", rc);
			goto failed_loop;
		}
	} else {
		batt_temp.intval = cas_info->debug_prop.temperature;
	}

	if (batt_temp.intval < cas_info->config_prop.temp_cool) {
		status = TEMP_SAFE_MODE_COOL;
	} else if (batt_temp.intval > cas_info->config_prop.temp_warm) {
		status = TEMP_SAFE_MODE_WARM;
	} else {
		return TEMP_SAFE_MODE_NORMAL;
	}

	age_index = charge_arbitrate_get_age_data(cas_info);
	if (age_index < 0) {
		pr_alarm("Get age data failed\n");
		goto failed_loop;
	}

	run_prop_pre->run_fcv = (status == TEMP_SAFE_MODE_COOL) ?
						cas_info->config_prop.age_data[0].age_fcv : run_prop_pre->run_fcv;
	run_prop_pre->run_rech_soc = cas_info->config_prop.age_data[age_index].age_rech_soc_2st;
	run_prop_pre->run_rech_voltage = cas_info->config_prop.age_data[age_index].age_rech_voltage_2st;
	run_prop_pre->topoff_current_percent = cas_info->config_prop.abnormal_topoff;

	pr_alarm("fcc %d, fcv %d, resoc %d, topoff %d, rech_voltage %d\n",
							run_prop_pre->run_fcc,
							run_prop_pre->run_fcv,
							run_prop_pre->run_rech_soc,
							run_prop_pre->topoff_current_percent,
							run_prop_pre->run_rech_voltage);

	charge_arbitrate_update_chip_status(cas_info);

	return status;

failed_loop:
	return -EINVAL;
}

static int charge_arbitrate_c1_status_charging(struct charge_arbitrate_info *cas_info)
{
	struct cas_run_prop *run_prop_pre = NULL;
	union power_supply_propval battery_capacity = {0, };
	int age_index = 0;
	int rc = 0;

	if (!cas_info->debug_prop.capacity) {
		rc = power_supply_get_property(cas_info->battery_psy,
					POWER_SUPPLY_PROP_CAPACITY, &battery_capacity);
		if (rc < 0) {
			pr_alarm("Failed to get CAPACITY property rc=%d\n", rc);
			return rc;
		}
	} else {
		battery_capacity.intval = cas_info->debug_prop.capacity;
	}

	age_index = charge_arbitrate_get_age_data(cas_info);
	if (age_index < 0) {
		pr_alarm("Get age data failed\n");
		goto failed_loop;
	}

	run_prop_pre = &cas_info->run_prop;

	run_prop_pre->run_fcc = cas_info->config_prop.age_data[age_index].age_fcc;
	run_prop_pre->run_fcv = cas_info->config_prop.age_data[age_index].age_fcv;
	run_prop_pre->run_rech_soc = cas_info->config_prop.age_data[age_index].age_rech_soc_1st;
	run_prop_pre->run_rech_voltage = cas_info->config_prop.age_data[age_index].age_rech_voltage_1st;
	run_prop_pre->topoff_current_percent = cas_info->config_prop.normal_topoff;

	pr_alarm("fcc %d, fcv %d, resoc %d, topoff %d, rech_voltage %d\n",
							run_prop_pre->run_fcc,
							run_prop_pre->run_fcv,
							run_prop_pre->run_rech_soc,
							run_prop_pre->topoff_current_percent,
							run_prop_pre->run_rech_voltage);

	charge_arbitrate_update_chip_status(cas_info);

	charge_arbitrate_ktime_enable(cas_info);

	get_monotonic_boottime(&cas_info->run_prop.time_begin);

	cas_info->capacity_abnormal = false;

	pr_alarm("charging: C1 --->>> C2\n");

	cas_info->status = CHARGE_STATUS_CHARGE_RUNNING_C2;

	return 0;

failed_loop:
	return -EINVAL;
}

static int charge_arbitrate_c2_status_charging(struct charge_arbitrate_info *cas_info)
{
	int rc = 0;

	rc = temperature_abnormal_status_switch(cas_info);

	if (rc < 0) {
		pr_alarm("Get temperature status failed\n");
		return -EINVAL;
	} else if (rc != TEMP_SAFE_MODE_NORMAL) {
		pr_alarm("temperature abnormal: C2 --->>> C3\n");
		cas_info->status = CHARGE_STATUS_CHARGE_SAFE_MODE_C3;
	}

	return 0;
}

static int charge_arbitrate_c2_status_chargefull(struct charge_arbitrate_info *cas_info)
{
	pr_alarm("charge full: C2 --->>> R1\n");

	cas_info->status = CHARGE_STATUS_RECHARGE_START_R1;

	return 0;
}

static int charge_arbitrate_c3_status_charging(struct charge_arbitrate_info *cas_info)
{
	int rc = 0;

	rc = temperature_normal_status_switch(cas_info);
	if (rc < 0) {
		pr_alarm("Get temperature status failed\n");
		goto failed_loop;
	}

	if (rc == TEMP_SAFE_MODE_NORMAL) {
		pr_alarm("temperature normal: C3 --->>> C1\n");
		cas_info->status = CHARGE_STATUS_CHARGE_START_C1;
	} else {
		cas_info->capacity_abnormal = true;
	}

	return 0;

failed_loop:
	return -EINVAL;
}

static int charge_arbitrate_c3_status_chargefull(struct charge_arbitrate_info *cas_info)
{
	pr_alarm("charge full: C3 --->>> R1\n");

	cas_info->status = CHARGE_STATUS_RECHARGE_START_R1;

	cas_info->capacity_abnormal = true;

	return 0;
}

static int charge_arbitrate_r1_status_charging(struct charge_arbitrate_info *cas_info)
{
	struct cas_run_prop *run_prop_pre = NULL;
	int age_index = 0;

	run_prop_pre = &cas_info->run_prop;

	age_index = charge_arbitrate_get_age_data(cas_info);
	if (age_index < 0) {
		pr_alarm("Get age data failed\n");
		goto failed_loop;
	}

	run_prop_pre->run_fcc = cas_info->config_prop.age_data[age_index].age_fcc;
	run_prop_pre->run_fcv = cas_info->config_prop.age_data[age_index].age_fcv;
	run_prop_pre->run_rech_soc = cas_info->config_prop.age_data[age_index].age_rech_soc_2st;
	run_prop_pre->run_rech_voltage = cas_info->config_prop.age_data[age_index].age_rech_voltage_2st;
	run_prop_pre->topoff_current_percent = cas_info->config_prop.normal_topoff;

	pr_alarm("fcc %d, fcv %d, resoc %d, topoff %d, rech_voltage %d\n",
							run_prop_pre->run_fcc,
							run_prop_pre->run_fcv,
							run_prop_pre->run_rech_soc,
							run_prop_pre->topoff_current_percent,
							run_prop_pre->run_rech_voltage);

	charge_arbitrate_update_chip_status(cas_info);

	pr_alarm("charging: R1 --->>> R2\n");

	cas_info->status = CHARGE_STATUS_RECHARGE_RUNNING_R2;

	return 0;
failed_loop:
	return -EINVAL;
}

static int charge_arbitrate_r1_p1_status_chargefull(struct charge_arbitrate_info *cas_info)
{
	union power_supply_propval capacity_raw = {0, };
	union power_supply_propval voltage_max = {0, };
	union power_supply_propval voltage_ocv = {0, };
	union power_supply_propval charge_status = {0, };
	int rc = 0;
	int usb_online = 0;

	if (cas_info->run_prop.run_rech_soc != 0 && !cas_info->debug_prop.capacity_raw) {
		rc = power_supply_get_property(cas_info->battery_psy,
				POWER_SUPPLY_PROP_CAPACITY_RAW_ZTE, &capacity_raw);
		if (rc < 0) {
			pr_alarm("Failed to get present property rc=%d\n", rc);
			goto failed_loop;
		}
	} else {
		capacity_raw.intval = cas_info->debug_prop.capacity_raw;
	}

	if (cas_info->run_prop.run_rech_voltage != 0 && !cas_info->debug_prop.volt_max) {
		rc = power_supply_get_property(cas_info->battery_psy,
					POWER_SUPPLY_PROP_VOLTAGE_MAX, &voltage_max);
		if (rc < 0) {
			pr_alarm("Failed to get present property rc=%d\n", rc);
			goto failed_loop;
		}
	} else {
		voltage_max.intval = cas_info->debug_prop.volt_max;
	}

	if (cas_info->run_prop.run_rech_voltage != 0 && !cas_info->debug_prop.volt_ocv) {
		rc = power_supply_get_property(cas_info->battery_psy,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, &voltage_ocv);
		if (rc < 0) {
			pr_alarm("Failed to get present property rc=%d\n", rc);
			goto failed_loop;
		}
	} else {
		voltage_ocv.intval = cas_info->debug_prop.volt_ocv;
	}

	if (!cas_info->force_charger_full) {
		cas_info->force_charger_full = true;
		pr_alarm("--->>>recharge soc or recharge voltage check mode, force charge full\n");
	}

	if (!cas_info->debug_prop.charge_status) {
		rc = power_supply_get_property(cas_info->battery_psy,
					POWER_SUPPLY_PROP_STATUS, &charge_status);
		if (rc < 0) {
			pr_alarm("Failed to get present property rc=%d\n", rc);
			goto failed_loop;
		}
	} else {
		charge_status.intval = cas_info->debug_prop.charge_status;
	}

	rc = power_supply_get_prop_by_name("usb",
				POWER_SUPPLY_PROP_PRESENT, &usb_online);
	if (rc < 0) {
		pr_alarm("Failed to get present property rc=%d\n", rc);
		return 0;
	}

	switch (charge_status.intval) {
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if (!usb_online) {
			pr_alarm("<<<---Exit force charge full; usb leave found!!!\n");
			cas_info->force_charger_full = false;
			charge_arbitrate_set_input_suspend(cas_info, false);
			break;
		}
		/*no break, for check recharge*/
	case POWER_SUPPLY_STATUS_CHARGING:
		if ((cas_info->run_prop.run_rech_soc != 0)/*check soc_now less than recharge soc*/
			&& ((capacity_raw.intval * 100 / cas_info->config_prop.full_raw_soc)
											< cas_info->run_prop.run_rech_soc)) {
			cas_info->force_charger_full = false;
			charge_arbitrate_set_input_suspend(cas_info, false);
			pr_alarm("<<<---Exit force charge full; soc now: %d, recharge soc: %d\n",
								(capacity_raw.intval * 100 / cas_info->config_prop.full_raw_soc),
								cas_info->run_prop.run_rech_soc);
		} else if ((cas_info->run_prop.run_rech_voltage != 0) /*check delta voltage less than recharge voltage*/
			&& ((voltage_max.intval - voltage_ocv.intval) >
								(int)cas_info->run_prop.run_rech_voltage * 1000)) {
			cas_info->force_charger_full = false;
			charge_arbitrate_set_input_suspend(cas_info, false);
			pr_alarm("<<<---Exit force charge full; delta volt: %d, recharge volt: %d\n",
								(voltage_max.intval - voltage_ocv.intval),
								cas_info->run_prop.run_rech_voltage);
		} else {/*Don't reach recharge soc or recharge voltage, set input current to 0mA*/
			pr_alarm("charging status: %s, waiting recharge........!!!\n",
				(charge_status.intval == POWER_SUPPLY_STATUS_CHARGING) ? "charging" : "discharging");
			charge_arbitrate_set_input_suspend(cas_info, true);
		}
		break;
	case POWER_SUPPLY_STATUS_FULL:/*original process, do nothing*/
		pr_alarm("charging status: charge full, waiting charge status changed!!!\n");
		break;
	default:
		pr_alarm("### Charge Status Is Unknown ###\n");
		break;
	}

	return 0;

failed_loop:
	return -EINVAL;
}

static int charge_arbitrate_r2_status_charging(struct charge_arbitrate_info *cas_info)
{
	int rc = 0;

	rc = temperature_abnormal_status_switch(cas_info);

	if (rc < 0) {
		pr_alarm("Get temperature status failed\n");
		return -EINVAL;
	} else if (rc != TEMP_SAFE_MODE_NORMAL) {
		pr_alarm("temperature abnormal: R2 --->>> R3\n");
		cas_info->status = CHARGE_STATUS_RECHARGE_SAFE_MODE_R3;
	}

	return 0;
}

static int charge_arbitrate_r2_status_chargefull(struct charge_arbitrate_info *cas_info)
{
	pr_alarm("charge full: R2 --->>> P1\n");

	cas_info->status = CHARGE_STATUS_RECHARGE_PLUS_START_P1;

	return 0;
}

static int charge_arbitrate_r3_status_charging(struct charge_arbitrate_info *cas_info)
{
	int rc = 0;

	rc = temperature_normal_status_switch(cas_info);

	if (rc < 0) {
		pr_alarm("Get temperature status failed\n");
		goto failed_loop;
	} else if (rc == TEMP_SAFE_MODE_NORMAL) {
		pr_alarm("temperature normal: R3 --->>> R1\n");
		cas_info->status = CHARGE_STATUS_RECHARGE_START_R1;
	}

	return 0;

failed_loop:
	return -EINVAL;
}

static int charge_arbitrate_r3_status_chargefull(struct charge_arbitrate_info *cas_info)
{
	pr_alarm("charge full: R3 --->>> P1\n");

	cas_info->status = CHARGE_STATUS_RECHARGE_PLUS_START_P1;

	return 0;
}

static int charge_arbitrate_p1_status_charging(struct charge_arbitrate_info *cas_info)
{
	struct cas_run_prop *run_prop_pre = NULL;
	int age_index = 0;

	run_prop_pre = &cas_info->run_prop;

	age_index = charge_arbitrate_get_age_data(cas_info);
	if (age_index < 0) {
		pr_alarm("Get age data failed\n");
		goto failed_loop;
	}

	run_prop_pre->run_fcc = cas_info->config_prop.age_data[age_index].age_fcc;
	run_prop_pre->run_fcv = cas_info->config_prop.age_data[age_index].age_fcv;
	run_prop_pre->run_rech_soc = cas_info->config_prop.age_data[age_index].age_rech_soc_2st;
	run_prop_pre->run_rech_voltage = cas_info->config_prop.age_data[age_index].age_rech_voltage_2st;
	run_prop_pre->topoff_current_percent = cas_info->config_prop.abnormal_topoff;

	pr_alarm("fcc %d, fcv %d, resoc %d, topoff %d, rech_voltage %d\n",
							run_prop_pre->run_fcc,
							run_prop_pre->run_fcv,
							run_prop_pre->run_rech_soc,
							run_prop_pre->topoff_current_percent,
							run_prop_pre->run_rech_voltage);

	charge_arbitrate_update_chip_status(cas_info);

	pr_alarm("charge full: P1 --->>> P2\n");

	cas_info->status = CHARGE_STATUS_RECHARGE_PLUS_RUNNING_P2;

	return 0;

failed_loop:
	return -EINVAL;
}

static int charge_arbitrate_turnoff_state_machine(struct charge_arbitrate_info *cas_info)
{
	int usb_online = 0, rc = 0, age_index = 0;

	rc = power_supply_get_prop_by_name("usb",
				POWER_SUPPLY_PROP_PRESENT, &usb_online);
	if (rc < 0) {
		pr_alarm("Failed to get present property rc=%d\n", rc);
		return 0;
	}

	if (usb_online) {
		pr_alarm("Discharge Exit:  because usb is online\n");
		return 0;
	}

	pr_alarm("Discharge:  --->>> C1.\n");

	/*set run status to default*/
	age_index = charge_arbitrate_get_age_data(cas_info);
	if (age_index < 0) {
		pr_alarm("Get age data failed\n");
		goto failed_loop;
	}

	cas_info->run_prop.run_fcc = cas_info->config_prop.age_data[age_index].age_fcc;
	cas_info->run_prop.run_fcv = cas_info->config_prop.age_data[age_index].age_fcv;
	cas_info->run_prop.run_rech_soc = cas_info->config_prop.age_data[age_index].age_rech_soc_1st;
	cas_info->run_prop.run_rech_voltage = cas_info->config_prop.age_data[age_index].age_rech_voltage_1st;
	cas_info->run_prop.topoff_current_percent = cas_info->config_prop.normal_topoff;

	cas_info->status = CHARGE_STATUS_CHARGE_START_C1;

	cas_info->capacity_abnormal = false;

	cas_info->disable_cas = false;

	charge_arbitrate_ktime_disable(cas_info);

	charge_arbitrate_update_chip_status(cas_info);

failed_loop:
	return 0;
}

struct cas_status_list status_list[] = {
	{
		CHARGE_STATUS_CHARGE_START_C1,
		"CHARGE_START_C1",
		charge_arbitrate_c1_status_charging,
		charge_arbitrate_c1_status_charging,
		charge_arbitrate_turnoff_state_machine
	},

	{
		CHARGE_STATUS_CHARGE_RUNNING_C2,
		"CHARGE_RUNNING_C2",
		charge_arbitrate_c2_status_charging,
		charge_arbitrate_c2_status_chargefull,
		charge_arbitrate_turnoff_state_machine
	},

	{
		CHARGE_STATUS_CHARGE_SAFE_MODE_C3,
		"CHARGE_SAFE_MODE_C3",
		charge_arbitrate_c3_status_charging,
		charge_arbitrate_c3_status_chargefull,
		charge_arbitrate_turnoff_state_machine
	},

	{
		CHARGE_STATUS_RECHARGE_START_R1,
		"RECHARGE_START_R1",
		charge_arbitrate_r1_status_charging,
		charge_arbitrate_r1_p1_status_chargefull,
		charge_arbitrate_turnoff_state_machine
	},

	{
		CHARGE_STATUS_RECHARGE_RUNNING_R2,
		"RECHARGE_RUNNING_R2",
		charge_arbitrate_r2_status_charging,
		charge_arbitrate_r2_status_chargefull,
		charge_arbitrate_turnoff_state_machine
	},

	{
		CHARGE_STATUS_RECHARGE_SAFE_MODE_R3,
		"RECHARGE_SAFE_MODE_R3",
		charge_arbitrate_r3_status_charging,
		charge_arbitrate_r3_status_chargefull,
		charge_arbitrate_turnoff_state_machine
	},

	{
		CHARGE_STATUS_RECHARGE_PLUS_START_P1,
		"RECHARGE_PLUS_START_P1",
		charge_arbitrate_p1_status_charging,
		charge_arbitrate_r1_p1_status_chargefull,
		charge_arbitrate_turnoff_state_machine
	},

	{
		CHARGE_STATUS_RECHARGE_PLUS_RUNNING_P2,
		"RECHARGE_PLUS_RUNNING_P2",
		NULL,
		NULL,
		charge_arbitrate_turnoff_state_machine
	},

	{
		CHARGE_STATUS_MAX,
		"STATUS_ABNORMAL",
		charge_arbitrate_turnoff_state_machine,
		charge_arbitrate_turnoff_state_machine,
		charge_arbitrate_turnoff_state_machine
	},
};

static int charge_arbitrate_battery_notifier_handler(struct charge_arbitrate_info *cas_info)
{
	union power_supply_propval charge_status = {0, };
	int rc = 0;

	if (!cas_info->debug_prop.charge_status) {
		rc = power_supply_get_property(cas_info->battery_psy,
					POWER_SUPPLY_PROP_STATUS, &charge_status);
		if (rc < 0) {
			pr_alarm("Failed to get present property rc=%d\n", rc);
			return NOTIFY_DONE;
		}
	} else {
		charge_status.intval = cas_info->debug_prop.charge_status;
	}

	if (cas_info->status > CHARGE_STATUS_MAX) {
		pr_alarm("### machine state is abnormal, reset to default ###\n");
		cas_info->status = CHARGE_STATUS_MAX;
	}

	if (cas_info->debug_prop.machine_status) {
		cas_info->status = cas_info->debug_prop.machine_status;
		cas_info->debug_prop.machine_status = 0;
	}

	if (cas_info->force_charger_full) {
		charge_status.intval = POWER_SUPPLY_STATUS_FULL;
	}

	if (cas_info->disable_cas) {
		pr_alarm("### CAS is disabled, discharge ###\n");
		cas_info->status = CHARGE_STATUS_CHARGE_START_C1;
		charge_status.intval = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	switch (charge_status.intval) {
	case POWER_SUPPLY_STATUS_CHARGING:
		pr_alarm(">>> machine state %s, charging <<<\n",
					status_list[cas_info->status].status_name);
		if (status_list[cas_info->status].charging)
			status_list[cas_info->status].charging(cas_info);
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		pr_alarm(">>> machine state %s, discharge <<<\n",
					status_list[cas_info->status].status_name);
		if (status_list[cas_info->status].discharge)
			status_list[cas_info->status].discharge(cas_info);
		break;
	case POWER_SUPPLY_STATUS_FULL:
		pr_alarm(">>> machine state %s, charge full <<<\n",
					status_list[cas_info->status].status_name);
		if (status_list[cas_info->status].charge_full)
			status_list[cas_info->status].charge_full(cas_info);
		break;
	default:
		pr_alarm("### Charge Status Is Unknown ###\n");
		break;
	}

	charge_arbitrate_debug_battery_info(cas_info);

	return 0;
}

static int charge_arbitrate_notifier_switch(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct charge_arbitrate_info *cas_info = container_of(nb, struct charge_arbitrate_info, nb);
	const char *name = NULL;

	if (event != PSY_EVENT_PROP_CHANGED) {
		return NOTIFY_DONE;
	}

	if (delayed_work_pending(&cas_info->timeout_work))
		return NOTIFY_OK;

#ifdef KERNEL_ABOVE_4_1_0
	name = psy->desc->name;
#else
	name = psy->name;
#endif

	if ((strcmp(name, cas_info->config_prop.battery_phy_name) == 0)
		|| (strcmp(name, "usb") == 0)
		|| (strcmp(name, "cas") == 0)
		|| (strcmp(name, "bcl") == 0)) {
		/*pr_alarm("Notify, update status!!!\n");*/
		charge_arbitrate_ktime_reset(cas_info);
		queue_delayed_work(cas_info->timeout_workqueue, &cas_info->timeout_work, msecs_to_jiffies(0));
	}

	return NOTIFY_OK;
}


static int charge_arbitrate_register_notifier(struct charge_arbitrate_info *cas_info)
{
	int rc = 0;

	cas_info->nb.notifier_call = charge_arbitrate_notifier_switch;

	rc = power_supply_reg_notifier(&cas_info->nb);
	if (rc < 0) {
		pr_alarm("Couldn't register psy notifier rc = %d\n", rc);
		return -EINVAL;
	}

	return 0;
}

/* hrtimer function handler */
static enum hrtimer_restart charge_arbitrate_timer(struct hrtimer *timer)
{
	struct charge_arbitrate_info *cas_info =
			container_of(timer, struct charge_arbitrate_info, timeout_timer);

	hrtimer_forward_now(&cas_info->timeout_timer, cas_info->timer_interval);

	if (delayed_work_pending(&cas_info->timeout_work))
		return HRTIMER_RESTART;

	/*pr_alarm("Timeout, update status!!!\n");*/
	queue_delayed_work(cas_info->timeout_workqueue, &cas_info->timeout_work, msecs_to_jiffies(0));

	return HRTIMER_RESTART;
}

static int charge_arbitrate_check_retry(struct charge_arbitrate_info *cas_info)
{
	static int probe_count = 0;

	probe_count++;

	pr_alarm("Battery Capacity Learning Driver Retry[%d/%d]!!!\n",
							probe_count, cas_info->config_prop.retry_times);
	if (probe_count < cas_info->config_prop.retry_times) {
		queue_delayed_work(cas_info->cas_probe_wq, &cas_info->cas_probe_work, msecs_to_jiffies(500));
		return true;
	}

	return false;
}

static void charge_arbitrate_timeout_handler_work(struct work_struct *work)
{
	struct charge_arbitrate_info *cas_info =
			container_of(work, struct charge_arbitrate_info, timeout_work.work);

	charge_arbitrate_battery_notifier_handler(cas_info);
}

static void charge_arbitrate_probe_work(struct work_struct *work)
{
	struct charge_arbitrate_info *cas_info =
			container_of(work, struct charge_arbitrate_info, cas_probe_work.work);
	u64 timeout_ms = 0;
#ifdef KERNEL_ABOVE_4_1_0
	struct power_supply_config cas_psy_cfg;
#else
	int rc = 0;
#endif

	pr_alarm("Charge Arbitrate Driver Init Begin.....\n");
	atomic_set(&cas_info->init_finished, 0);

	cas_info->subsidiary_psy = power_supply_get_by_name(cas_info->config_prop.subsidiary_psy_name);
	if (!cas_info->subsidiary_psy) {
		pr_alarm("Get bms psy failed!!!!\n");
		goto subsidiary_psy_failed;
	}

	cas_info->battery_psy = power_supply_get_by_name(cas_info->config_prop.battery_phy_name);
	if (!cas_info->battery_psy) {
		pr_alarm("Get battery psy failed!!!!\n");
		goto battery_psy_failed;
	}

	cas_info->interface_psy = power_supply_get_by_name(cas_info->config_prop.interface_phy_name);
	if (!cas_info->battery_psy) {
		pr_alarm("Get battery psy failed!!!!\n");
		goto interface_psy_failed;
	}

	memset(&cas_info->run_prop, 0, sizeof(struct cas_run_prop));

	cas_info->status = CHARGE_STATUS_CHARGE_START_C1;
	cas_info->capacity_abnormal = false;

	/* Register the power supply */
#ifdef KERNEL_ABOVE_4_1_0
	memset(&cas_psy_cfg, 0, sizeof(struct power_supply_config));
	cas_psy_cfg.drv_data = cas_info;
	cas_psy_cfg.of_node = NULL;
	cas_psy_cfg.supplied_to = NULL;
	cas_psy_cfg.num_supplicants = 0;
	cas_info->cas_psy_pointer = devm_power_supply_register(cas_info->dev, &cas_psy_desc,
			&cas_psy_cfg);
	if (IS_ERR(cas_info->cas_psy_pointer)) {
		pr_alarm("failed to register cas_psy rc = %ld\n",
				PTR_ERR(cas_info->cas_psy_pointer));
		goto register_power_supply_failed;
	}
#else
	memset(&cas_info->cas_psy, 0, sizeof(struct power_supply));
	cas_info->cas_psy.name = "cas";
	cas_info->cas_psy.type = POWER_SUPPLY_TYPE_BMS;
	cas_info->cas_psy.properties = cas_psy_props;
	cas_info->cas_psy.num_properties = ARRAY_SIZE(cas_psy_props);
	cas_info->cas_psy.get_property = cas_psy_get_property;
	cas_info->cas_psy.set_property = cas_psy_set_property;
	cas_info->cas_psy.external_power_changed = cas_external_power_changed;
	cas_info->cas_psy.property_is_writeable = cas_property_is_writeable;
	cas_info->cas_psy.drv_data = cas_info;
	cas_info->cas_psy.of_node = NULL;
	cas_info->cas_psy.supplied_to = NULL;
	cas_info->cas_psy.num_supplicants = 0;

	rc = power_supply_register(cas_info->dev, &cas_info->cas_psy);
	if (rc < 0) {
		pr_alarm("failed to register cas_psy rc = %d\n", rc);
		goto register_power_supply_failed;
	}
#endif
	/*wake_lock_init(&cas_info->cas_wake_lock, WAKE_LOCK_SUSPEND, "battery_capacity_learning");*/

	mutex_init(&cas_info->data_lock);

	spin_lock_init(&cas_info->timer_lock);

	cas_info->timeout_workqueue = create_singlethread_workqueue("charge-arbitrate-service");
	INIT_DELAYED_WORK(&cas_info->timeout_work, charge_arbitrate_timeout_handler_work);

	timeout_ms = (u64)cas_info->config_prop.timeout_seconds * 1000;
	cas_info->timer_interval = ms_to_ktime(timeout_ms);
	hrtimer_init(&cas_info->timeout_timer, CLOCK_BOOTTIME, HRTIMER_MODE_REL);
	cas_info->timeout_timer.function = charge_arbitrate_timer;

	if (charge_arbitrate_register_notifier(cas_info) < 0) {
		pr_alarm("Init vendor info failed!!!!\n");
		goto register_notifier_failed;
	}

	queue_delayed_work(cas_info->timeout_workqueue, &cas_info->timeout_work, msecs_to_jiffies(0));

	atomic_set(&cas_info->init_finished, 1);
	pr_alarm("Charge Arbitrate Driver Init Finished!!!\n");

	return;

register_notifier_failed:
	mutex_destroy(&cas_info->data_lock);
	/*wake_lock_destroy(&cas_info->cas_wake_lock);*/
register_power_supply_failed:
#ifdef KERNEL_ABOVE_4_1_0
	power_supply_put(cas_info->interface_psy);
#endif
	cas_info->interface_psy = NULL;
interface_psy_failed:
#ifdef KERNEL_ABOVE_4_1_0
	power_supply_put(cas_info->battery_psy);
#endif
	cas_info->battery_psy = NULL;
battery_psy_failed:
#ifdef KERNEL_ABOVE_4_1_0
	power_supply_put(cas_info->subsidiary_psy);
#endif
	cas_info->subsidiary_psy = NULL;
subsidiary_psy_failed:
	if (charge_arbitrate_check_retry(cas_info) == true) {
		return;
	}

	kfree(cas_info->config_prop.age_data);
	devm_kfree(cas_info->dev, cas_info);

	pr_alarm("Charge Arbitrate Driver Init Failed!!!\n");
}

static int charge_arbitrate_probe(struct platform_device *pdev)
{
	struct charge_arbitrate_info *cas_info;

	pr_alarm("Charge Arbitrate Driver Probe Begin.....\n");

	cas_info = devm_kzalloc(&pdev->dev, sizeof(struct charge_arbitrate_info), GFP_KERNEL);
	if (!cas_info) {
		pr_alarm("devm_kzalloc failed!!!!\n");
		return -ENOMEM;
	}

	memset(cas_info, 0, sizeof(struct charge_arbitrate_info));

	cas_info->dev = &pdev->dev;

	atomic_set(&cas_info->init_finished, 0);

	platform_set_drvdata(pdev, cas_info);

	if (charge_arbitrate_parse_dt(cas_info) < 0) {
		pr_alarm("Parse dts failed!!!!\n");
		goto parse_dt_failed;
	}

	if (!cas_info->config_prop.turn_on) {
		pr_alarm("Charge Arbitrate Disabled,Please Config \"cas,enable\"!!!!\n");
		goto parse_dt_failed;
	}

	cas_info->cas_probe_wq = create_singlethread_workqueue("cas_probe_wq");

	INIT_DELAYED_WORK(&cas_info->cas_probe_work, charge_arbitrate_probe_work);

	queue_delayed_work(cas_info->cas_probe_wq, &cas_info->cas_probe_work, msecs_to_jiffies(1000));

	pr_alarm("Charge Arbitrate Driver Probe Finished.....\n");

	return 0;

parse_dt_failed:
	kfree(cas_info->config_prop.age_data);
	devm_kfree(cas_info->dev, cas_info);
	cas_info = NULL;

	pr_alarm("Charge Arbitrate Probe Failed!!!\n");

	return 0;
}
static int charge_arbitrate_remove(struct platform_device *pdev)
{
	struct charge_arbitrate_info *cas_info = platform_get_drvdata(pdev);

	pr_alarm("Charge Arbitrate Driver Remove Begin.....\n");

	if (cas_info == NULL) {
		goto ExitLoop;
	}

	atomic_set(&cas_info->init_finished, 0);

	power_supply_unreg_notifier(&cas_info->nb);

	mutex_destroy(&cas_info->data_lock);

#ifdef KERNEL_ABOVE_4_1_0
	power_supply_put(cas_info->subsidiary_psy);

	power_supply_put(cas_info->battery_psy);

	power_supply_unregister(cas_info->cas_psy_pointer);
#else
	power_supply_unregister(&cas_info->cas_psy);
#endif

	devm_kfree(cas_info->dev, cas_info);

	cas_info = NULL;

ExitLoop:
	pr_alarm("Charge Arbitrate Driver Remove Finished!!!\n");

	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "zte,charge-arbitrate-service", },
	{ },
};

static struct platform_driver charge_arbitrate_driver = {
	.driver		= {
		.name		= "zte,charge-arbitrate-service",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
	},
	.probe		= charge_arbitrate_probe,
	.remove		= charge_arbitrate_remove,
};

/*module_platform_driver(charge_arbitrate_driver);*/

static inline int charge_arbitrate_platform_init(void)
{
	pr_alarm("Charge Arbitrate Driver module init.....\n");

	return platform_driver_register(&charge_arbitrate_driver);
}

static inline void charge_arbitrate_platform_exit(void)
{
	pr_alarm("Charge Arbitrate Driver module exit.....\n");

	platform_driver_unregister(&charge_arbitrate_driver);
}


late_initcall_sync(charge_arbitrate_platform_init);
module_exit(charge_arbitrate_platform_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("cui.xiaochuan <cui.xiaochuan@zte.com.cn>");
MODULE_DESCRIPTION("Charge Arbitrate Service Driver");
