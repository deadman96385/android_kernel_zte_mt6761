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

#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>

#include <mt-plat/charger_class.h>

#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
#include <linux/pmic-voter.h>
#include <linux/power_supply.h>

#define MTK_SETTING_VOTER			"MTK_SETTING_VOTER"
#define CAS_SETTING_VOTER			"CAS_SETTING_VOTER"
#define ADB_SETTING_VOTER			"ADB_SETTING_VOTER"
#define CHARGER_DEVICE_NMAE			"primary_chg"


#define cas_debug(fmt, args...)	pr_info("CAS: %s(): "fmt, __func__, ## args)

struct charge_votable_struct {
	/*struct charger_device *chg1_dev;*/
	struct power_supply	*interface_psy;
	struct votable		*fcc_votable;
	struct votable		*fcv_votable;
	struct votable		*topoff_votable;
	struct votable		*recharge_soc_votable;
	struct votable		*recharge_voltage_votable;
	struct votable		*usb_icl_votable;
	struct votable		*charging_enable_votable;
	atomic_t			init_finished;
};

static struct charge_votable_struct charge_votable_data;
#endif

static struct class *charger_class;

static ssize_t charger_show_name(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct charger_device *chg_dev = to_charger_device(dev);

	return snprintf(buf, 20, "%s\n",
		       chg_dev->props.alias_name ?
		       chg_dev->props.alias_name : "anonymous");
}

static int charger_suspend(struct device *dev, pm_message_t state)
{
	struct charger_device *chg_dev = to_charger_device(dev);

	if (chg_dev->ops->suspend)
		return chg_dev->ops->suspend(chg_dev, state);

	return 0;
}

static int charger_resume(struct device *dev)
{
	struct charger_device *chg_dev = to_charger_device(dev);

	if (chg_dev->ops->resume)
		return chg_dev->ops->resume(chg_dev);

	return 0;
}

static void charger_device_release(struct device *dev)
{
	struct charger_device *chg_dev = to_charger_device(dev);

	kfree(chg_dev);
}

int charger_dev_enable(struct charger_device *chg_dev, bool en)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	if (get_client_vote(charge_votable_data.charging_enable_votable,
			MTK_SETTING_VOTER) != en) {
		vote(charge_votable_data.charging_enable_votable, MTK_SETTING_VOTER, en, 0);
	} else {
		rerun_election(charge_votable_data.charging_enable_votable);
	}
	return 0;
#else
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->enable)
		return chg_dev->ops->enable(chg_dev, en);
#endif

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_enable);

int charger_dev_is_enabled(struct charger_device *chg_dev, bool *en)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	*en = get_client_vote(charge_votable_data.charging_enable_votable, MTK_SETTING_VOTER);
	cas_debug("MTK get enable %d\n", *en);
	return 0;
#else
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->is_enabled)
		return chg_dev->ops->is_enabled(chg_dev, en);
#endif

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_is_enabled);

int charger_dev_plug_in(struct charger_device *chg_dev)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->plug_in)
		return chg_dev->ops->plug_in(chg_dev);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_plug_in);

int charger_dev_plug_out(struct charger_device *chg_dev)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->plug_out)
		return chg_dev->ops->plug_out(chg_dev);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_plug_out);

int charger_dev_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->event)
		return chg_dev->ops->event(chg_dev, event, args);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_do_event);

int charger_dev_set_charging_current(struct charger_device *chg_dev, u32 uA)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	if (get_client_vote(charge_votable_data.fcc_votable, MTK_SETTING_VOTER) != uA)
		vote(charge_votable_data.fcc_votable, MTK_SETTING_VOTER, true, uA);
	else
		rerun_election(charge_votable_data.fcc_votable);
	return 0;
#else
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->set_charging_current)
		return chg_dev->ops->set_charging_current(chg_dev, uA);
#endif

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_set_charging_current);

int charger_dev_get_charging_current(struct charger_device *chg_dev, u32 *uA)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	*uA = get_client_vote(charge_votable_data.fcc_votable, MTK_SETTING_VOTER);
	cas_debug("MTK get fcc %d\n", *uA);
	return 0;
#else
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->get_charging_current)
		return chg_dev->ops->get_charging_current(chg_dev, uA);
#endif

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_charging_current);

int charger_dev_get_min_charging_current(struct charger_device *chg_dev,
					 u32 *uA)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->get_min_charging_current)
		return chg_dev->ops->get_min_charging_current(chg_dev, uA);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_min_charging_current);

int charger_dev_enable_chip(struct charger_device *chg_dev, bool en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->enable_chip)
		return chg_dev->ops->enable_chip(chg_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_enable_chip);

int charger_dev_is_chip_enabled(struct charger_device *chg_dev, bool *en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->is_chip_enabled)
		return chg_dev->ops->is_chip_enabled(chg_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_is_chip_enabled);

int charger_dev_enable_direct_charging(struct charger_device *chg_dev, bool en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->enable_direct_charging)
		return chg_dev->ops->enable_direct_charging(chg_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_enable_direct_charging);

int charger_dev_kick_direct_charging_wdt(struct charger_device *chg_dev)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->kick_direct_charging_wdt)
		return chg_dev->ops->kick_direct_charging_wdt(chg_dev);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_kick_direct_charging_wdt);

int charger_dev_get_vbus(struct charger_device *chg_dev, u32 *vbus)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->get_vbus_adc)
		return chg_dev->ops->get_vbus_adc(chg_dev, vbus);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_vbus);

int charger_dev_get_ibus(struct charger_device *chg_dev, u32 *ibus)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->get_ibus_adc)
		return chg_dev->ops->get_ibus_adc(chg_dev, ibus);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_ibus);

int charger_dev_get_temperature(struct charger_device *chg_dev, int *tchg_min,
		int *tchg_max)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->get_tchg_adc)
		return chg_dev->ops->get_tchg_adc(chg_dev, tchg_min,
						  tchg_max);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_temperature);

int charger_dev_set_input_current(struct charger_device *chg_dev, u32 uA)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	if (get_client_vote(charge_votable_data.usb_icl_votable, MTK_SETTING_VOTER) != uA)
		vote(charge_votable_data.usb_icl_votable, MTK_SETTING_VOTER, true, uA);
	else
		rerun_election(charge_votable_data.usb_icl_votable);
	return 0;
#else
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->set_input_current)
		return chg_dev->ops->set_input_current(chg_dev, uA);
#endif

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_set_input_current);

int charger_dev_get_input_current(struct charger_device *chg_dev, u32 *uA)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	*uA = get_client_vote(charge_votable_data.usb_icl_votable, MTK_SETTING_VOTER);
	cas_debug("MTK get icl %d\n", *uA);
	return 0;
#else
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->get_input_current)
		return chg_dev->ops->get_input_current(chg_dev, uA);
#endif

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_input_current);

int charger_dev_set_ship_mode(struct charger_device *charger_dev, bool en)
{
	if (charger_dev != NULL && charger_dev->ops != NULL && charger_dev->ops->set_ship_mode)
		return charger_dev->ops->set_ship_mode(charger_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_set_ship_mode);

int charger_dev_get_ship_mode(struct charger_device *charger_dev, bool *en)
{
	if (charger_dev != NULL && charger_dev->ops != NULL && charger_dev->ops->get_ship_mode)
		return charger_dev->ops->get_ship_mode(charger_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_ship_mode);

int charger_dev_get_min_input_current(struct charger_device *chg_dev, u32 *uA)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->get_min_input_current)
		return chg_dev->ops->get_min_input_current(chg_dev, uA);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_min_input_current);

int charger_dev_set_eoc_current(struct charger_device *chg_dev, u32 uA)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	if (get_client_vote(charge_votable_data.topoff_votable, MTK_SETTING_VOTER) != uA)
		vote(charge_votable_data.topoff_votable, MTK_SETTING_VOTER, true, uA);
	else
		rerun_election(charge_votable_data.topoff_votable);
	return 0;
#else
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->set_eoc_current)
		chg_dev->ops->set_eoc_current(chg_dev, uA);
#endif

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_set_eoc_current);

int charger_dev_get_eoc_current(struct charger_device *chg_dev, u32 *uA)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	*uA = get_client_vote(charge_votable_data.topoff_votable, MTK_SETTING_VOTER);
	cas_debug("MTK get topoff %d\n", *uA);
	return 0;
#else
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->get_eoc_current)
		return chg_dev->ops->get_eoc_current(chg_dev, uA);
#endif

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_eoc_current);

int charger_dev_kick_wdt(struct charger_device *chg_dev)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->kick_wdt)
		return chg_dev->ops->kick_wdt(chg_dev);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_kick_wdt);

int charger_dev_set_constant_voltage(struct charger_device *chg_dev, u32 uV)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	if (get_client_vote(charge_votable_data.fcv_votable, MTK_SETTING_VOTER) != uV)
		vote(charge_votable_data.fcv_votable, MTK_SETTING_VOTER, true, uV);
	else
		rerun_election(charge_votable_data.fcv_votable);
	return 0;
#else
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->set_constant_voltage)
		return chg_dev->ops->set_constant_voltage(chg_dev, uV);
#endif

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_set_constant_voltage);

int charger_dev_get_constant_voltage(struct charger_device *chg_dev, u32 *uV)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	*uV = get_effective_result(charge_votable_data.fcv_votable);
	cas_debug("get fcv %d\n", *uV);
	return 0;
#else
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->get_constant_voltage)
		return chg_dev->ops->get_constant_voltage(chg_dev, uV);
#endif

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_constant_voltage);

#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
int charger_dev_set_recharge_voltage(struct charger_device *chg_dev, u32 uV)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	if (get_client_vote(charge_votable_data.recharge_voltage_votable, MTK_SETTING_VOTER) != uV)
		vote(charge_votable_data.recharge_voltage_votable, MTK_SETTING_VOTER, true, uV);
	else
		rerun_election(charge_votable_data.recharge_voltage_votable);
	return 0;
#else
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->set_recharge_voltage)
		return chg_dev->ops->set_recharge_voltage(chg_dev, uV);
#endif

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_set_recharge_voltage);

int charger_dev_get_recharge_voltage(struct charger_device *chg_dev, u32 *uV)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	*uV = get_client_vote(charge_votable_data.recharge_voltage_votable, MTK_SETTING_VOTER);
	cas_debug("MTK get recharge voltage %d\n", *uV);
	return 0;
#else
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->get_recharge_voltage)
		return chg_dev->ops->get_recharge_voltage(chg_dev, uV);
#endif

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_recharge_voltage);
#endif
int charger_dev_dump_registers(struct charger_device *chg_dev)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->dump_registers)
		return chg_dev->ops->dump_registers(chg_dev);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_dump_registers);

int charger_dev_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->is_charging_done)
		return chg_dev->ops->is_charging_done(chg_dev, done);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_is_charging_done);

int charger_dev_enable_vbus_ovp(struct charger_device *chg_dev, bool en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->enable_vbus_ovp)
		return chg_dev->ops->enable_vbus_ovp(chg_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_enable_vbus_ovp);

int charger_dev_set_mivr(struct charger_device *chg_dev, u32 uV)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->set_mivr)
		return chg_dev->ops->set_mivr(chg_dev, uV);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_set_mivr);

int charger_dev_enable_powerpath(struct charger_device *chg_dev, bool en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->enable_powerpath)
		return chg_dev->ops->enable_powerpath(chg_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_enable_powerpath);

int charger_dev_is_powerpath_enabled(struct charger_device *chg_dev, bool *en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->is_powerpath_enabled)
		return chg_dev->ops->is_powerpath_enabled(chg_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_is_powerpath_enabled);

int charger_dev_enable_safety_timer(struct charger_device *chg_dev, bool en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->enable_safety_timer)
		return chg_dev->ops->enable_safety_timer(chg_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_enable_safety_timer);

int charger_dev_is_safety_timer_enabled(struct charger_device *chg_dev,
					bool *en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->is_safety_timer_enabled)
		return chg_dev->ops->is_safety_timer_enabled(chg_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_is_safety_timer_enabled);

int charger_dev_enable_termination(struct charger_device *chg_dev, bool en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->enable_termination)
		return chg_dev->ops->enable_termination(chg_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_enable_termination);

int charger_dev_get_mivr_state(struct charger_device *chg_dev, bool *in_loop)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->get_mivr_state)
		return chg_dev->ops->get_mivr_state(chg_dev, in_loop);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_mivr_state);

int charger_dev_send_ta_current_pattern(struct charger_device *chg_dev,
					bool is_increase)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->send_ta_current_pattern)
		return chg_dev->ops->send_ta_current_pattern(chg_dev,
							     is_increase);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_send_ta_current_pattern);

int charger_dev_send_ta20_current_pattern(struct charger_device *chg_dev,
					  u32 uV)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->send_ta20_current_pattern)
		return chg_dev->ops->send_ta20_current_pattern(chg_dev, uV);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_send_ta20_current_pattern);

int charger_dev_reset_ta(struct charger_device *chg_dev)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->reset_ta)
		return chg_dev->ops->reset_ta(chg_dev);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_reset_ta);

int charger_dev_set_pe20_efficiency_table(struct charger_device *chg_dev)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->set_pe20_efficiency_table)
		return chg_dev->ops->set_pe20_efficiency_table(chg_dev);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_set_pe20_efficiency_table);

int charger_dev_enable_cable_drop_comp(struct charger_device *chg_dev, bool en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->enable_cable_drop_comp)
		return chg_dev->ops->enable_cable_drop_comp(chg_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_enable_cable_drop_comp);

int charger_dev_set_direct_charging_ibusoc(struct charger_device *chg_dev,
					   u32 uA)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->set_direct_charging_ibusoc)
		return chg_dev->ops->set_direct_charging_ibusoc(chg_dev,
								uA);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_set_direct_charging_ibusoc);

int charger_dev_set_direct_charging_vbusov(struct charger_device *chg_dev,
					   u32 uV)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->set_direct_charging_vbusov)
		return chg_dev->ops->set_direct_charging_vbusov(chg_dev,
								uV);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_set_direct_charging_vbusov);

int charger_dev_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->enable_chg_type_det)
		return chg_dev->ops->enable_chg_type_det(chg_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_enable_chg_type_det);

int charger_dev_enable_otg(struct charger_device *chg_dev, bool en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->enable_otg)
		return chg_dev->ops->enable_otg(chg_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_enable_otg);

int charger_dev_enable_discharge(struct charger_device *chg_dev, bool en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->enable_discharge)
		return chg_dev->ops->enable_discharge(chg_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_enable_discharge);

int charger_dev_set_boost_current_limit(struct charger_device *chg_dev, u32 uA)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->set_boost_current_limit)
		return chg_dev->ops->set_boost_current_limit(chg_dev, uA);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_set_boost_current_limit);

int charger_dev_get_zcv(struct charger_device *chg_dev, u32 *uV)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->get_zcv)
		return chg_dev->ops->get_zcv(chg_dev, uV);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_zcv);

int charger_dev_run_aicl(struct charger_device *chg_dev, u32 *uA)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->run_aicl)
		return chg_dev->ops->run_aicl(chg_dev, uA);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_run_aicl);

int charger_dev_reset_eoc_state(struct charger_device *chg_dev)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->reset_eoc_state)
		return chg_dev->ops->reset_eoc_state(chg_dev);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_reset_eoc_state);

int charger_dev_safety_check(struct charger_device *chg_dev)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->safety_check)
		return chg_dev->ops->safety_check(chg_dev);

	return -ENOTSUPP;
}

int charger_dev_notify(struct charger_device *chg_dev, int event)
{
	return srcu_notifier_call_chain(
		&chg_dev->evt_nh, event, &chg_dev->noti);
}

int charger_dev_get_fod_status(struct charger_device *charger_dev, u8 *status)
{
	if (charger_dev != NULL && charger_dev->ops != NULL &&
					       charger_dev->ops->get_fod_status)
		return charger_dev->ops->get_fod_status(charger_dev, status);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_get_fod_status);

int charger_dev_enable_fod_oneshot(struct charger_device *charger_dev, bool en)
{
	if (charger_dev != NULL && charger_dev->ops != NULL &&
					   charger_dev->ops->enable_fod_oneshot)
		return charger_dev->ops->enable_fod_oneshot(charger_dev, en);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_enable_fod_oneshot);

int charger_dev_is_typec_ot(struct charger_device *charger_dev, bool *ot)
{
	if (charger_dev != NULL && charger_dev->ops != NULL &&
						  charger_dev->ops->is_typec_ot)
		return charger_dev->ops->is_typec_ot(charger_dev, ot);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(charger_dev_is_typec_ot);

static DEVICE_ATTR(name, 0444, charger_show_name, NULL);

static struct attribute *charger_class_attrs[] = {
	&dev_attr_name.attr,
	NULL,
};

static const struct attribute_group charger_group = {
	.attrs = charger_class_attrs,
};

static const struct attribute_group *charger_groups[] = {
	&charger_group,
	NULL,
};

int register_charger_device_notifier(struct charger_device *chg_dev,
				struct notifier_block *nb)
{
	int ret;

	ret = srcu_notifier_chain_register(&chg_dev->evt_nh, nb);
	return ret;
}
EXPORT_SYMBOL(register_charger_device_notifier);

int unregister_charger_device_notifier(struct charger_device *chg_dev,
				struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&chg_dev->evt_nh, nb);
}
EXPORT_SYMBOL(unregister_charger_device_notifier);

/**
 * charger_device_register - create and register a new object of
 *   charger_device class.
 * @name: the name of the new object
 * @parent: a pointer to the parent device
 * @devdata: an optional pointer to be stored for private driver use.
 * The methods may retrieve it by using charger_get_data(charger_dev).
 * @ops: the charger operations structure.
 *
 * Creates and registers new charger device. Returns either an
 * ERR_PTR() or a pointer to the newly allocated device.
 */
struct charger_device *charger_device_register(const char *name,
		struct device *parent, void *devdata,
		const struct charger_ops *ops,
		const struct charger_properties *props)
{
	struct charger_device *chg_dev;
	static struct lock_class_key key;
	struct srcu_notifier_head *head;
	int rc;

	pr_debug("charger_device_register: name=%s\n", name);
	chg_dev = kzalloc(sizeof(*chg_dev), GFP_KERNEL);
	if (!chg_dev)
		return ERR_PTR(-ENOMEM);

	mutex_init(&chg_dev->ops_lock);
	chg_dev->dev.class = charger_class;
	chg_dev->dev.parent = parent;
	chg_dev->dev.release = charger_device_release;
	dev_set_name(&chg_dev->dev, name);
	dev_set_drvdata(&chg_dev->dev, devdata);
	head = &chg_dev->evt_nh;
	srcu_init_notifier_head(head);
	/* Rename srcu's lock to avoid LockProve warning */
	lockdep_init_map(&(&head->srcu)->dep_map, name, &key, 0);

	/* Copy properties */
	if (props) {
		memcpy(&chg_dev->props, props,
		       sizeof(struct charger_properties));
	}
	rc = device_register(&chg_dev->dev);
	if (rc) {
		kfree(chg_dev);
		return ERR_PTR(rc);
	}
	chg_dev->ops = ops;
	return chg_dev;
}
EXPORT_SYMBOL(charger_device_register);

/**
 * charger_device_unregister - unregisters a switching charger device
 * object.
 * @charger_dev: the switching charger device object to be unregistered
 * and freed.
 *
 * Unregisters a previously registered via charger_device_register object.
 */
void charger_device_unregister(struct charger_device *chg_dev)
{
	if (!chg_dev)
		return;

	mutex_lock(&chg_dev->ops_lock);
	chg_dev->ops = NULL;
	mutex_unlock(&chg_dev->ops_lock);
	device_unregister(&chg_dev->dev);
}
EXPORT_SYMBOL(charger_device_unregister);


static int charger_match_device_by_name(struct device *dev,
	const void *data)
{
	const char *name = data;

	return strcmp(dev_name(dev), name) == 0;
}

struct charger_device *get_charger_by_name(const char *name)
{
	struct device *dev;

	if (!name)
		return (struct charger_device *)NULL;
	dev = class_find_device(charger_class, NULL, name,
				charger_match_device_by_name);

	return dev ? to_charger_device(dev) : NULL;

}
EXPORT_SYMBOL(get_charger_by_name);

static void __exit charger_class_exit(void)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	atomic_set(&charge_votable_data.init_finished, 0);
	destroy_votable(charge_votable_data.fcc_votable);
	destroy_votable(charge_votable_data.fcv_votable);
	destroy_votable(charge_votable_data.topoff_votable);
	destroy_votable(charge_votable_data.recharge_soc_votable);
	destroy_votable(charge_votable_data.recharge_voltage_votable);
	destroy_votable(charge_votable_data.usb_icl_votable);
	destroy_votable(charge_votable_data.charging_enable_votable);
#endif

	class_destroy(charger_class);
}

#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
static int charger_fcc_vote_callback(struct votable *votable,
			void *data, int max_fcc_ua, const char *client)
{
	/*struct charge_votable_struct *charge_votable_data = data;*/
	struct charger_device *charger_dev = get_charger_by_name(CHARGER_DEVICE_NMAE);

	pr_info("VOTE: client: %s max_fcc_ua: %d\n", client, max_fcc_ua);

	if (charger_dev != NULL && charger_dev->ops != NULL && charger_dev->ops->set_charging_current)
		return charger_dev->ops->set_charging_current(charger_dev, max_fcc_ua);

	return 0;
}

static int charger_fcv_vote_callback(struct votable *votable,
			void *data, int max_fcv_uv, const char *client)
{
	struct charger_device *charger_dev = get_charger_by_name(CHARGER_DEVICE_NMAE);

	pr_info("VOTE: client: %s max_fcv_uv: %d\n", client, max_fcv_uv);

	if (charger_dev != NULL && charger_dev->ops != NULL && charger_dev->ops->set_constant_voltage)
		return charger_dev->ops->set_constant_voltage(charger_dev, max_fcv_uv);

	return 0;
}

static int charger_topoff_vote_callback(struct votable *votable,
			void *data, int min_topoff_ua, const char *client)
{
	struct charger_device *charger_dev = get_charger_by_name(CHARGER_DEVICE_NMAE);

	pr_info("VOTE: client: %s min_topoff_ua: %d\n", client, min_topoff_ua);

	if (charger_dev != NULL && charger_dev->ops != NULL && charger_dev->ops->set_eoc_current)
		charger_dev->ops->set_eoc_current(charger_dev, min_topoff_ua);

	return 0;
}

static int charger_recharge_soc_vote_callback(struct votable *votable,
			void *data, int recharge_soc, const char *client)
{

	return 0;
}

static int charger_recharge_voltage_vote_callback(struct votable *votable,
			void *data, int recharge_voltage, const char *client)
{
	struct charger_device *charger_dev = get_charger_by_name(CHARGER_DEVICE_NMAE);

	pr_info("VOTE: client: %s recharge_voltage: %d\n", client, recharge_voltage);

	if (charger_dev != NULL && charger_dev->ops != NULL && charger_dev->ops->set_recharge_voltage)
		return charger_dev->ops->set_recharge_voltage(charger_dev, recharge_voltage);
	return 0;
}

static int charger_usb_icl_vote_callback(struct votable *votable,
			void *data, int max_icl_ua, const char *client)
{
	struct charger_device *charger_dev = get_charger_by_name(CHARGER_DEVICE_NMAE);

	pr_info("VOTE: client: %s enable: %d\n", client, max_icl_ua);

	if (charger_dev != NULL && charger_dev->ops != NULL && charger_dev->ops->set_input_current)
		return charger_dev->ops->set_input_current(charger_dev, max_icl_ua);

	return 0;
}

static int charger_charging_enable_vote_callback(struct votable *votable,
			void *data, int enable, const char *client)
{
	struct charger_device *charger_dev = get_charger_by_name(CHARGER_DEVICE_NMAE);

	pr_info("VOTE: client: %s enable: %d\n", client, enable);

	if (charger_dev != NULL && charger_dev->ops != NULL && charger_dev->ops->enable)
		charger_dev->ops->enable(charger_dev, enable);

	return 0;
}

static int interface_psy_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *pval)
{
	struct charge_votable_struct *pdata = power_supply_get_drvdata(psy);
	int rc = 0;

	if (!pdata || !atomic_read(&pdata->init_finished)) {
		cas_debug("interface Uninitialized!!!\n");
		return -ENODATA;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		pval->intval = get_client_vote(pdata->fcc_votable, CAS_SETTING_VOTER);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pval->intval = get_client_vote(pdata->fcv_votable, CAS_SETTING_VOTER);
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		pval->intval = get_client_vote(pdata->usb_icl_votable, CAS_SETTING_VOTER);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		pval->intval = get_client_vote(pdata->topoff_votable, CAS_SETTING_VOTER);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		pval->intval = get_client_vote(pdata->charging_enable_votable, CAS_SETTING_VOTER);
		break;
	case POWER_SUPPLY_PROP_RECHARGE_SOC:
		pval->intval = get_client_vote(pdata->recharge_voltage_votable, CAS_SETTING_VOTER);
		break;
	default:
		cas_debug("interface unsupported property %d\n", psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int interface_psy_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *pval)
{
	struct charge_votable_struct *pdata = power_supply_get_drvdata(psy);
	int rc = 0;

	if (!pdata || !atomic_read(&pdata->init_finished)) {
		cas_debug("interface Uninitialized!!!\n");
		return -ENODATA;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (pval->intval > 0)
			vote(pdata->fcc_votable, CAS_SETTING_VOTER, true, pval->intval);
		else
			vote(pdata->fcc_votable, CAS_SETTING_VOTER, false, 0);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (pval->intval > 0)
			vote(pdata->fcv_votable, CAS_SETTING_VOTER, true, pval->intval);
		else
			vote(pdata->fcv_votable, CAS_SETTING_VOTER, false, 0);
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		vote(pdata->usb_icl_votable, CAS_SETTING_VOTER,
								!!pval->intval, pval->intval ? 100000 : 0);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		if (pval->intval > 0)
			vote(pdata->topoff_votable, CAS_SETTING_VOTER, true, pval->intval * 1000);
		else
			vote(pdata->topoff_votable, CAS_SETTING_VOTER, false, 0);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		if (pval->intval > 0)
			vote(pdata->charging_enable_votable, CAS_SETTING_VOTER, true, 0);
		else
			vote(pdata->charging_enable_votable, CAS_SETTING_VOTER, false, 0);
		break;
	case POWER_SUPPLY_PROP_RECHARGE_SOC:
		if (pval->intval > 0)
			vote(pdata->recharge_voltage_votable, CAS_SETTING_VOTER, true, pval->intval);
		else
			vote(pdata->recharge_voltage_votable, CAS_SETTING_VOTER, false, 0);
		break;
	default:
		cas_debug("interface unsupported property %d\n", psp);
		rc = -EINVAL;
		break;
	}


	return rc;
}

static int interface_property_is_writeable(struct power_supply *psy,
					enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_RECHARGE_SOC:
		return 1;
	default:
		break;
	}

	return 0;
}

static void interface_external_power_changed(struct power_supply *psy)
{
	cas_debug("power supply changed\n");
}

static enum power_supply_property interface_psy_props[] = {
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_RECHARGE_SOC,
};

static const struct power_supply_desc interface_psy_desc = {
	.name = "interface",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = interface_psy_props,
	.num_properties = ARRAY_SIZE(interface_psy_props),
	.get_property = interface_psy_get_property,
	.set_property = interface_psy_set_property,
	.external_power_changed = interface_external_power_changed,
	.property_is_writeable = interface_property_is_writeable,
};
#endif

static int __init charger_class_init(void)
{
#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	struct power_supply_config interface_psy_cfg;
	int rc = 0;
#endif

	charger_class = class_create(THIS_MODULE, "switching_charger");
	if (IS_ERR(charger_class)) {
		pr_notice("Unable to create charger class; errno = %ld\n",
			PTR_ERR(charger_class));
		return PTR_ERR(charger_class);
	}
	charger_class->dev_groups = charger_groups;
	charger_class->suspend = charger_suspend;
	charger_class->resume = charger_resume;

#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
	atomic_set(&charge_votable_data.init_finished, 0);

	/* Register the power supply */
	interface_psy_cfg.drv_data = &charge_votable_data;
	interface_psy_cfg.of_node = NULL;
	interface_psy_cfg.supplied_to = NULL;
	interface_psy_cfg.num_supplicants = 0;
	charge_votable_data.interface_psy = power_supply_register(NULL, &interface_psy_desc,
					&interface_psy_cfg);
	if (IS_ERR(charge_votable_data.interface_psy)) {
		rc = PTR_ERR(charge_votable_data.interface_psy);
		cas_debug("failed to register bcl_psy rc = %ld\n",
				PTR_ERR(charge_votable_data.interface_psy));
		goto register_power_supply_failed;
	}
/*
	charge_votable_data.chg1_dev = get_charger_by_name("primary_chg");
	if (charge_votable_data.chg1_dev)
		cas_debug("Found primary charger [%s]\n", charge_votable_data.chg1_dev->props.alias_name);
	else
		cas_debug("*** Error : can't find primary charger [%s]***\n", "primary_chg");
*/
	charge_votable_data.fcc_votable = create_votable("FCC", VOTE_MIN,
					charger_fcc_vote_callback,
					&charge_votable_data);
	if (IS_ERR(charge_votable_data.fcc_votable)) {
		rc = PTR_ERR(charge_votable_data.fcc_votable);
		cas_debug("create %s failed\n", "fcc_votable");
		goto destroy_votable;
	}

	charge_votable_data.fcv_votable = create_votable("FCV", VOTE_MIN,
					charger_fcv_vote_callback,
					&charge_votable_data);
	if (IS_ERR(charge_votable_data.fcv_votable)) {
		rc = PTR_ERR(charge_votable_data.fcv_votable);
		cas_debug("create %s failed\n", "fcv_votable");
		goto destroy_votable;
	}

	charge_votable_data.topoff_votable = create_votable("TOPOFF", VOTE_MAX,
					charger_topoff_vote_callback,
					&charge_votable_data);
	if (IS_ERR(charge_votable_data.topoff_votable)) {
		rc = PTR_ERR(charge_votable_data.topoff_votable);
		cas_debug("create %s failed\n", "topoff_votable");
		goto destroy_votable;
	}

	charge_votable_data.recharge_soc_votable = create_votable("RECH_SOC", VOTE_MIN,
					charger_recharge_soc_vote_callback,
					&charge_votable_data);
	if (IS_ERR(charge_votable_data.recharge_soc_votable)) {
		rc = PTR_ERR(charge_votable_data.recharge_soc_votable);
		cas_debug("create %s failed\n", "recharge_soc_votable");
		goto destroy_votable;
	}

	charge_votable_data.recharge_voltage_votable = create_votable("RECH_VOLTAGE", VOTE_MAX,
					charger_recharge_voltage_vote_callback,
					&charge_votable_data);
	if (IS_ERR(charge_votable_data.recharge_voltage_votable)) {
		rc = PTR_ERR(charge_votable_data.recharge_voltage_votable);
		cas_debug("create %s failed\n", "recharge_voltage_votable");
		goto destroy_votable;
	}

	charge_votable_data.usb_icl_votable = create_votable("USB_ICL", VOTE_MIN,
					charger_usb_icl_vote_callback,
					&charge_votable_data);
	if (IS_ERR(charge_votable_data.usb_icl_votable)) {
		rc = PTR_ERR(charge_votable_data.usb_icl_votable);
		cas_debug("create %s failed\n", "usb_icl_votable");
		goto destroy_votable;
	}

	charge_votable_data.charging_enable_votable = create_votable("CHARGING_ENABLE", VOTE_SET_ANY,
					charger_charging_enable_vote_callback,
					&charge_votable_data);
	if (IS_ERR(charge_votable_data.charging_enable_votable)) {
		rc = PTR_ERR(charge_votable_data.charging_enable_votable);
		cas_debug("create %s failed\n", "charging_enable_votable");
		goto destroy_votable;
	}
	atomic_set(&charge_votable_data.init_finished, 1);
#endif

	return 0;

#ifdef CONFIG_VENDOR_CHARGE_ARBITRATE_SERVICE
destroy_votable:
	atomic_set(&charge_votable_data.init_finished, 0);
	destroy_votable(charge_votable_data.fcc_votable);
	destroy_votable(charge_votable_data.fcv_votable);
	destroy_votable(charge_votable_data.topoff_votable);
	destroy_votable(charge_votable_data.recharge_soc_votable);
	destroy_votable(charge_votable_data.recharge_voltage_votable);
	destroy_votable(charge_votable_data.usb_icl_votable);
	destroy_votable(charge_votable_data.charging_enable_votable);
register_power_supply_failed:

	return rc;
#endif
}

subsys_initcall(charger_class_init);
module_exit(charger_class_exit);

MODULE_DESCRIPTION("Switching Charger Class Device");
MODULE_AUTHOR("Patrick Chang <patrick_chang@richtek.com>");
MODULE_VERSION("1.0.0_G");
MODULE_LICENSE("GPL");
