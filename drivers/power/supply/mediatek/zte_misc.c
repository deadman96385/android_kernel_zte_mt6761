/*
 * Driver for zte misc functions
 * function1: used for translate hardware GPIO to SYS GPIO number
 * function2: update fingerprint status to kernel from fingerprintd,2016/01/18
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include "zte_misc.h"

struct zte_gpio_info {
	int sys_num;
	const char *name;
};

#define MAX_SUPPORT_GPIOS 16
struct zte_gpio_info zte_gpios[MAX_SUPPORT_GPIOS];

static const struct of_device_id zte_misc_of_match[] = {
	{ .compatible = "zte-misc", },
	{ },
};
MODULE_DEVICE_TABLE(of, zte_misc_of_match);

#define CHARGER_BUF_SIZE 0x32
int get_sysnumber_byname(char *name)
{
	int i;

	for (i = 0; i < MAX_SUPPORT_GPIOS; i++) {
		if (zte_gpios[i].name) {
			if (!strcmp(zte_gpios[i].name, name))
				return zte_gpios[i].sys_num;
		}
	}
	return 0;
}
/*
static int get_devtree_pdata(struct device *dev)
{
	struct device_node *node, *pp;
	int count = -1;

	pr_info("zte_misc: translate hardware pin to system pin\n");
	node = dev->of_node;
	if (node == NULL)
		return -ENODEV;

	pp = NULL;
	while ((pp = of_get_next_child(node, pp))) {
		if (!of_find_property(pp, "label", NULL)) {
			dev_warn(dev, "Found without labels\n");
			continue;
		}
		count++;
		zte_gpios[count].name = kstrdup(of_get_property(pp, "label", NULL),
								GFP_KERNEL);
		zte_gpios[count].sys_num = of_get_gpio(pp, 0);
		pr_info("zte_misc: sys_number=%d name=%s\n", zte_gpios[count].sys_num, zte_gpios[count].name);
	}
	return 0;
}
*/


/*************************************************
fingerprint id detector
1. Old Methods
In MSM8996, Goodix and Synaptics.
Goodix id pin NO PULL;
Synaptics id pin PULL UP.
set id pin to pull up, then check the pin status:
0 for Goodix, 1 for Synaptics
2. New Method:
a. Read fingerprint id from LK
b. Pass the id by cmdline
**************************************************/
typedef enum fingerprint_chipid_type {
	FINGERPRINT_CHIPID_GOODIX_GF3118M = 1,
	FINGERPRINT_CHIPID_GOODIX_GF5206 = 2,
	FINGERPRINT_CHIPID_GOODIX_GF5208 = 3,
	FINGERPRINT_CHIPID_GOODIX_GF3238 = 4,
	FINGERPRINT_CHIPID_GOODIX_GF3258 = 5,
	FINGERPRINT_CHIPID_GOODIX_GF3658 = 6,

	FINGERPRINT_CHIPID_SYNAFP = 32,

	FINGERPRINT_CHIPID_FPC = 64,
	FINGERPRINT_CHIPID_FPC1028 = 65,

	FINGERPRINT_CHIPID_SILEAD = 128,

	FINGERPRINT_CHIPID_MAX = 256,

} fingerprint_chipid_type_t;

static unsigned long fingerprint_hw = FINGERPRINT_CHIPID_GOODIX_GF3118M;
module_param(fingerprint_hw, ulong, 0644);
/*
 * Get the fingerprint_id value
 */
static int __init fingerprint_id_setup(char *str)
{
	int ret = 0;

	pr_info("%s @str:%s\n", __func__, str);

	/* fingerprint_hw = simple_strtoul(str, NULL, 10);*/
	ret = kstrtoul(str, 10, &fingerprint_hw);
	if (ret != 0) {
		pr_info("%s, kstrtoul error\n", __func__);
	}
	pr_info("fingerprint_id =%lu\n", fingerprint_hw);


	return 0;
}
__setup("fingerprint_id=", fingerprint_id_setup);


static int zte_misc_fingerprint_hw_check(struct device *dev)
{
	char *fingerprint_id_name;

	switch (fingerprint_hw) {
	case FINGERPRINT_CHIPID_GOODIX_GF3118M:
		fingerprint_id_name = "GF3118M";
		break;
	case FINGERPRINT_CHIPID_GOODIX_GF5206:
		fingerprint_id_name = "GF5206";
		break;
	case FINGERPRINT_CHIPID_GOODIX_GF5208:
		fingerprint_id_name = "GF5208";
		break;
	case FINGERPRINT_CHIPID_GOODIX_GF3238:
		fingerprint_id_name = "GF3238";
		break;
	case FINGERPRINT_CHIPID_GOODIX_GF3258:
		fingerprint_id_name = "GF3258";
		break;
	case FINGERPRINT_CHIPID_GOODIX_GF3658:
		fingerprint_id_name = "GF3658";
		break;
	case FINGERPRINT_CHIPID_FPC:
		fingerprint_id_name = "FPC1140";
		break;
	case FINGERPRINT_CHIPID_FPC1028:
		fingerprint_id_name = "FPC1028";
		break;
	case FINGERPRINT_CHIPID_SILEAD:
		fingerprint_id_name = "SILEAD";
		break;
	case FINGERPRINT_CHIPID_SYNAFP:
		fingerprint_id_name = "SYNAFP";
		break;
	default:
		fingerprint_hw = FINGERPRINT_CHIPID_GOODIX_GF3118M;
		fingerprint_id_name = "NONE";
		break;
	}

	pr_info("%s, fingerprint_id is :%lu name:%s\n", __func__, fingerprint_hw, fingerprint_id_name);

	return 0;
}

bool is_goodix_fp(void)
{
	return (fingerprint_hw == FINGERPRINT_CHIPID_GOODIX_GF3118M);
}

bool is_goodix_milan_fp(void)
{
	return ((fingerprint_hw == FINGERPRINT_CHIPID_GOODIX_GF3238)
		|| (fingerprint_hw == FINGERPRINT_CHIPID_GOODIX_GF3258));
}


bool is_goodix_fp_gf3258(void)
{
	return (fingerprint_hw == FINGERPRINT_CHIPID_GOODIX_GF3258);
}

bool is_goodix_fp_gf3658(void)
{
	return (fingerprint_hw == FINGERPRINT_CHIPID_GOODIX_GF3658);
}

bool is_synafp_fp(void)
{
	return (fingerprint_hw == FINGERPRINT_CHIPID_SYNAFP);
}

bool is_fpc_fp(void)
{
	return (fingerprint_hw == FINGERPRINT_CHIPID_FPC);
}
bool is_fpc1028_fp(void)
{
	return (fingerprint_hw == FINGERPRINT_CHIPID_FPC1028);
}
bool is_silead_fp(void)
{
	return (fingerprint_hw == FINGERPRINT_CHIPID_SILEAD);
}

/*************************************************
 * Fingerprint Status Updater
 *************************************************/
enum fingerprint_msg_type {
	FINGERPRINT_ERROR = -1,
	FINGERPRINT_ACQUIRED = 1,
	FINGERPRINT_TEMPLATE_ENROLLING = 3,
	FINGERPRINT_TEMPLATE_REMOVED = 4,
	FINGERPRINT_AUTHENTICATED = 5,
	FINGERPRINT_DETECTED = 6,
	FINGERPRINT_REMOVED = 7,
};

static int fp_msg_disable = 0;
static int fp_msg_type = FINGERPRINT_ERROR;

/* extern void fb_blank_update_oem(void); defined in drivers\video\fbdev\core\fbmem.c*/
static int fp_msg_type_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}
	pr_info("DBG set fp_msg_type to %d, fp_msg_disable=%d\n", fp_msg_type, fp_msg_disable);
	if (fp_msg_disable == 1)
		return 0;

	if (fp_msg_type == FINGERPRINT_DETECTED) {
		pr_info("DBG fp_msg_set acquired\n");
		/* fb_blank_update_oem(); */
	}
	return 0;
}
module_param_call(fp_msg_type, fp_msg_type_set, param_get_int,
			&fp_msg_type, 0644);
module_param(fp_msg_disable, int, 0644);

static int shipmode_zte = -1;
static int zte_misc_control_shipmode(const char *val, const struct kernel_param *kp)
{
	struct power_supply	*batt_psy;
	int rc;
	const union power_supply_propval enable = {0,};

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}

	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		if (shipmode_zte == 0) {
			rc = batt_psy->desc->set_property(batt_psy,
					POWER_SUPPLY_PROP_SET_SHIP_MODE, &enable);
			if (rc) {
				pr_err("Warnning:ship enter error %d\n", rc);
			}
			pr_info("%s: enter into shipmode 10s later\n", __func__);
		} else {
			pr_info("%s: shipmode. Wrong value.Doing nothing\n", __func__);
		}
	} else
		pr_err("%s: batt_psy is NULL\n", __func__);

	return 0;
}
static int zte_misc_get_shipmode_node(char *val, const struct kernel_param *kp)
{
	struct power_supply	*batt_psy;
	int rc;
	union power_supply_propval pval = {0,};

	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		rc = batt_psy->desc->get_property(batt_psy,
					POWER_SUPPLY_PROP_SET_SHIP_MODE, &pval);
		if (rc) {
			pr_err("Get shipmode node error: %d\n", rc);
			return snprintf(val, CHARGER_BUF_SIZE, "%d", -1);
		}
		pr_info("%s: shipmode status %d\n", __func__, pval.intval);
		shipmode_zte = pval.intval;
	} else
		pr_err("%s: batt_psy is NULL\n", __func__);

	return snprintf(val, CHARGER_BUF_SIZE, "%d", pval.intval);
}

module_param_call(shipmode_zte, zte_misc_control_shipmode, zte_misc_get_shipmode_node,
					&shipmode_zte, 0644);

struct charging_policy_ops *g_charging_policy_ops = NULL;

static int demo_charging_policy = 0;
static int demo_charging_policy_set(const char *val, const struct kernel_param *kp)
{
	int rc;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}

	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_demo_sts_set)
		g_charging_policy_ops->charging_policy_demo_sts_set(g_charging_policy_ops, demo_charging_policy);
	else
		pr_err("can not set demo charging policy\n");

	pr_info("demo_charging_policy is %d\n", demo_charging_policy);

	return 0;
}
static int demo_charging_policy_get(char *val, const struct kernel_param *kp)
{
	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_demo_sts_get)
		demo_charging_policy = g_charging_policy_ops->charging_policy_demo_sts_get(g_charging_policy_ops);
	else
		pr_err("can not get demo charging policy\n");

	pr_info("demo_charging_policy is %d\n", demo_charging_policy);
	return snprintf(val, 0x2, "%d",  demo_charging_policy);
}
module_param_call(demo_charging_policy, demo_charging_policy_set,
	demo_charging_policy_get, &demo_charging_policy, 0644);

static int expired_policy_en;
static int expired_policy_en_set(const char *val, const struct kernel_param *kp)
{
	int rc;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}

	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_expired_mode_set)
		g_charging_policy_ops->charging_policy_expired_mode_set(g_charging_policy_ops,
			expired_policy_en);
	else
		pr_err("can not set expired_policy_en\n");

	pr_info("expired_policy_en is %d\n", expired_policy_en);

	return 0;
}

static int expired_policy_en_get(char *val, const struct kernel_param *kp)
{
	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_expired_mode_get)
		expired_policy_en =
			g_charging_policy_ops->charging_policy_expired_mode_get(g_charging_policy_ops);
	else
		pr_err("can not get expired_policy_en\n");

	pr_info("expired_policy_en is %d\n", expired_policy_en);
	return snprintf(val, 0x20, "%d",  expired_policy_en);
}
module_param_call(expired_policy_en, expired_policy_en_set,
	expired_policy_en_get, &expired_policy_en, 0644);


static int expired_charging_policy = 0;
static int expired_charging_policy_set(const char *val, const struct kernel_param *kp)
{
	int rc;

	pr_info("expired_charging_policy_set.\n");
	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}
	pr_info("expired_charging_policy is %d\n", expired_charging_policy);

	return 0;
}
static int expired_charging_policy_get(char *val, const struct kernel_param *kp)
{
	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_expired_sts_get)
		expired_charging_policy = g_charging_policy_ops->charging_policy_expired_sts_get(g_charging_policy_ops);
	else
		pr_err("can not get expired charging policy\n");

	pr_info("expired_charging_policy is %d\n", expired_charging_policy);
	return snprintf(val, 0x2, "%d",  expired_charging_policy);
}
module_param_call(expired_charging_policy, expired_charging_policy_set,
	expired_charging_policy_get, &expired_charging_policy, 0644);

static int charging_time_sec = 0;
static int expired_charging_policy_sec_set(const char *val, const struct kernel_param *kp)
{
	int rc;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}

	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_expired_sec_set)
		g_charging_policy_ops->charging_policy_expired_sec_set(g_charging_policy_ops,
			charging_time_sec);
	else
		pr_err("can not set expired charging policy sec\n");

	pr_info("expired_charging_policy_sec_set is %d\n", charging_time_sec);

	return 0;
}
static int expired_charging_policy_sec_get(char *val, const struct kernel_param *kp)
{
	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_expired_sec_get)
		charging_time_sec =
			g_charging_policy_ops->charging_policy_expired_sec_get(g_charging_policy_ops);
	else
		pr_err("can not get expired charging policy\n");

	pr_info("expired_charging_policy is %d\n", charging_time_sec);
	return snprintf(val, 0x20, "%d",  charging_time_sec);
}
module_param_call(charging_time_sec, expired_charging_policy_sec_set,
	expired_charging_policy_sec_get, &charging_time_sec, 0644);

static int policy_lowtemp_en;
static int policy_lowtemp_en_set(const char *val, const struct kernel_param *kp)
{
	int rc;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}

	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_lowtemp_en_set)
		g_charging_policy_ops->charging_policy_lowtemp_en_set(g_charging_policy_ops,
			policy_lowtemp_en);
	else
		pr_err("can not set policy_lowtemp_en\n");

	pr_info("policy_lowtemp_en is %d\n", policy_lowtemp_en);

	return 0;
}

static int policy_lowtemp_en_get(char *val, const struct kernel_param *kp)
{
	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_lowtemp_en_get)
		policy_lowtemp_en =
			g_charging_policy_ops->charging_policy_lowtemp_en_get(g_charging_policy_ops);
	else
		pr_err("can not get policy_lowtemp_en\n");

	pr_info("policy_lowtemp_en is %d\n", policy_lowtemp_en);
	return snprintf(val, 0x20, "%d",  policy_lowtemp_en);
}
module_param_call(policy_lowtemp_en, policy_lowtemp_en_set,
	policy_lowtemp_en_get, &policy_lowtemp_en, 0644);

static int policy_cap_min;
static int policy_cap_min_set(const char *val, const struct kernel_param *kp)
{
	int rc;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}

	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_cap_min_set)
		g_charging_policy_ops->charging_policy_cap_min_set(g_charging_policy_ops,
			policy_cap_min);
	else
		pr_err("can not set policy_cap_min\n");

	pr_info("policy_cap_min is %d\n", policy_cap_min);

	return 0;
}

static int policy_cap_min_get(char *val, const struct kernel_param *kp)
{
	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_cap_min_get)
		policy_cap_min =
			g_charging_policy_ops->charging_policy_cap_min_get(g_charging_policy_ops);
	else
		pr_err("can not get policy_cap_min\n");

	pr_info("policy_cap_min is %d\n", policy_cap_min);
	return snprintf(val, CHARGER_BUF_SIZE, "%d",  policy_cap_min);
}

module_param_call(policy_cap_min, policy_cap_min_set,
	policy_cap_min_get, &policy_cap_min, 0644);

static int policy_cap_max;
static int policy_cap_max_set(const char *val, const struct kernel_param *kp)
{
	int rc;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}

	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_cap_max_set)
		g_charging_policy_ops->charging_policy_cap_max_set(g_charging_policy_ops,
			policy_cap_max);
	else
		pr_err("can not set policy_cap_max\n");

	pr_info("policy_cap_max is %d\n", policy_cap_max);

	return 0;
}

static int policy_cap_max_get(char *val, const struct kernel_param *kp)
{
	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_cap_max_get)
		policy_cap_max =
			g_charging_policy_ops->charging_policy_cap_max_get(g_charging_policy_ops);
	else
		pr_err("can not get policy_cap_max\n");

	pr_info("policy_cap_max is %d\n", policy_cap_max);
	return snprintf(val, CHARGER_BUF_SIZE, "%d",  policy_cap_max);
}

module_param_call(policy_cap_max, policy_cap_max_set,
	policy_cap_max_get, &policy_cap_max, 0644);


int zte_misc_register_charging_policy_ops(struct charging_policy_ops *ops)
{
	int ret = 0;

	pr_info("%s.\n", __func__);
	g_charging_policy_ops = ops;

	return ret;
}

/* Emode function to enable/disable 0% shutdown  */
int enable_to_shutdown = 1;
static int set_enable_to_shutdown(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	pr_warn("set_enable_to_shutdown to %d\n", enable_to_shutdown);
	return 0;
}

module_param_call(enable_to_shutdown, set_enable_to_shutdown, param_get_int,
			&enable_to_shutdown, 0644);

int enable_to_dump_reg = 0;
static int set_enable_to_dump_reg(const char *val, const struct kernel_param *kp)
{
	int ret = 0;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	pr_warn("enable_to_dump_reg %d\n", enable_to_dump_reg);
	return 0;
}

module_param_call(enable_to_dump_reg, set_enable_to_dump_reg, param_get_int,
			&enable_to_dump_reg, 0644);

int disable_power_off = 0;
static int set_disable_power_off(const char *val, const struct kernel_param *kp)
{
	int ret = 0;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	pr_warn("disable_power_off %d\n", disable_power_off);
	return 0;
}

module_param_call(disable_power_off, set_disable_power_off, param_get_int,
			&disable_power_off, 0644);

static int zte_misc_charging_enabled;
static int zte_misc_control_charging(const char *val, const struct kernel_param *kp)
{
	struct power_supply *batt_psy;
	int rc;
	const union power_supply_propval enable = {1,};
	const union power_supply_propval disable = {0,};

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}

	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		if (zte_misc_charging_enabled != 0) {
			rc = batt_psy->desc->set_property(batt_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &enable);
			pr_info("%s: enable charging\n", __func__);
		} else {
			rc = batt_psy->desc->set_property(batt_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &disable);
			pr_info("%s: disable charging\n", __func__);
		}
		if (rc)
			pr_err("battery does not export CHARGING_ENABLED: %d\n", rc);
	} else
		pr_err("%s: batt_psy is NULL\n", __func__);

	return 0;
}

static int zte_misc_get_charging_enabled_node(char *val, const struct kernel_param *kp)
{
	struct power_supply *batt_psy;
	int rc;
	union power_supply_propval pval = {0,};

	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		rc = batt_psy->desc->get_property(batt_psy,
		POWER_SUPPLY_PROP_CHARGING_ENABLED, &pval);
		pr_info("%s: enable charging status %d\n", __func__, pval.intval);
		if (rc) {
			pr_err("battery charging_enabled node is not exist: %d\n", rc);
			return  snprintf(val, CHARGER_BUF_SIZE, "%d", -1);
		}
		zte_misc_charging_enabled = pval.intval;
	} else
		pr_err("%s: batt_psy is NULL\n", __func__);

	return  snprintf(val, CHARGER_BUF_SIZE, "%d", pval.intval);
}

module_param_call(charging_enabled, zte_misc_control_charging, zte_misc_get_charging_enabled_node,
			&zte_misc_charging_enabled, 0644);

enum charger_types_oem charge_type_oem = CHARGER_TYPE_DEFAULT;
module_param_named(
	charge_type_oem, charge_type_oem, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
);

int charge_remain_time;
module_param_named(
	charge_remain_time, charge_remain_time, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
);

static int design_capacity = 4000;
static int zte_misc_get_design_capacity(char *val, const struct kernel_param *kp)
{
	int rc = 0;
	int zte_design_capacity = 0;
	struct power_supply *batt_psy;
	union power_supply_propval prop = {0,};

	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		rc = batt_psy->desc->get_property(batt_psy,
				POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &prop);
		if (rc)
			pr_err("failed to battery design capacity, error:%d.\n", rc);
		else {
			/* convert unit from uAh to mAh if need*/
			if (prop.intval >= 1000000) {
				prop.intval /= 1000;
			}
			zte_design_capacity = prop.intval;
			pr_info("battery design capacity = %dmAh\n",
				zte_design_capacity);
		}
	} else
		pr_err("batt_psy is NULL.\n");

	return  snprintf(val, CHARGER_BUF_SIZE, "%d", zte_design_capacity);
}
module_param_call(design_capacity, NULL, zte_misc_get_design_capacity,
			&design_capacity, 0644);

static int zte_misc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("%s: e\n", __func__);
/*
	int error;

	pr_info("%s: e\n", __func__);
	error = get_devtree_pdata(dev);
	if (error)
		return error;
  */
	zte_misc_fingerprint_hw_check(dev);
	/* zte_batt_switch_init(); */

	pr_info("%s ----\n", __func__);
	return 0;
}

static int  zte_misc_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver zte_misc_device_driver = {
	.probe		= zte_misc_probe,
	.remove		= zte_misc_remove,
	.driver		= {
		.name	= "zte-misc",
		.owner	= THIS_MODULE,
		.of_match_table = zte_misc_of_match,
	}
};

int __init zte_misc_init(void)
{
	return platform_driver_register(&zte_misc_device_driver);
}

static void __exit zte_misc_exit(void)
{
	platform_driver_unregister(&zte_misc_device_driver);
}
fs_initcall(zte_misc_init);
module_exit(zte_misc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Misc driver for zte");
MODULE_ALIAS("platform:zte-misc");
