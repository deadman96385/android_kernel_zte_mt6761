#include "zte_lcd_common.h"

#ifdef CONFIG_ZTE_LCD_REG_DEBUG
extern void zte_lcd_reg_debug_func(struct LCM_DRIVER *lcm_drv, struct LCM_UTIL_FUNCS *utils);
#endif

struct LCM_DRIVER *g_zte_ctrl_pdata = NULL;
const char *zte_get_lcd_panel_name(void)
{
	if (g_zte_ctrl_pdata == NULL)
		return "no_panel_info";
	else
		return g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name;
}

static int zte_lcd_proc_info_show(struct seq_file *m, void *v)
{
	if (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name) {
		seq_printf(m, "panel name=%s\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name);
	} else {
		seq_printf(m, "%s\n", "panel name is not detect");
	}

	if (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_init_code_version) {
		seq_printf(m, "version=%s\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_init_code_version);
	} else {
		seq_printf(m, "%s\n", "version is not detect");
	}

	return 0;
}
static int zte_lcd_proc_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, zte_lcd_proc_info_show, NULL);
}
static const struct file_operations zte_lcd_common_func_proc_fops = {
	.owner = THIS_MODULE,
	.open = zte_lcd_proc_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
static int zte_lcd_proc_info_display(struct LCM_DRIVER *lcm_drv)
{
	struct proc_dir_entry *proc_lcd_id = NULL;

	proc_lcd_id = proc_create_data("driver/lcd_id_v2", 0, NULL, &zte_lcd_common_func_proc_fops, NULL);
	if (!proc_lcd_id) {
		ZTE_LCD_ERROR("%s:create driver/lcd_id error!\n", __func__);
		return -EPERM;
	}

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name = lcm_drv->name;
	ZTE_LCD_INFO("%s panel_name = %s\n", __func__, lcm_drv->name);

	return 0;
}

#ifdef CONFIG_ZTE_LCD_CABC3_EXTREME_POWER_SAVE
static ssize_t lcd_cabc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int retval = 0;

	mutex_lock(&g_zte_ctrl_pdata->zte_lcd_ctrl->panel_sys_lock);
	retval = snprintf(buf, 32, "%d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->cabc_value);
	ZTE_LCD_INFO("%s:cabc_value: 0x%x\n", __func__, g_zte_ctrl_pdata->zte_lcd_ctrl->cabc_value);
	mutex_unlock(&g_zte_ctrl_pdata->zte_lcd_ctrl->panel_sys_lock);
	return retval;
}
static int zte_set_panel_cabc(int cabc_mode)
{
	struct sprd_dsi *dsi = dev_get_drvdata(g_zte_ctrl_pdata->zte_lcd_ctrl->panel_info->pd->intf);

	ZTE_LCD_INFO("%s:cabc_mode= 0x%x\n", __func__, cabc_mode);
	switch (cabc_mode) {
	mipi_dsi_lp_cmd_enable(dsi, true);
	case LCD_CABC_OFF_MODE:
		mipi_dsi_send_cmds(dsi, g_zte_ctrl_pdata->zte_lcd_ctrl->cabc_off_cmds);
		break;
	case LCD_CABC_LOW_MODE:
		mipi_dsi_send_cmds(dsi, g_zte_ctrl_pdata->zte_lcd_ctrl->cabc_low_cmds);
		break;
	case LCD_CABC_MEDIUM_MODE:
		mipi_dsi_send_cmds(dsi, g_zte_ctrl_pdata->zte_lcd_ctrl->cabc_medium_cmds);
		break;
	case LCD_CABC_HIGH_MODE:
		mipi_dsi_send_cmds(dsi, g_zte_ctrl_pdata->zte_lcd_ctrl->cabc_high_cmds);
		break;
	default:
		mipi_dsi_send_cmds(dsi, g_zte_ctrl_pdata->zte_lcd_ctrl->cabc_off_cmds);
		break;
	}
	return 0;
}
static ssize_t lcd_cabc_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input = 0;

	if (kstrtoint(buf, 16, &input) != 0)
		return -EINVAL;

	if (input > LCD_CABC_HIGH_MODE)
		return -EINVAL;

	mutex_lock(&g_zte_ctrl_pdata->zte_lcd_ctrl->panel_sys_lock);
	g_zte_ctrl_pdata->zte_lcd_ctrl->cabc_value = input;
	zte_set_panel_cabc(input);
	mutex_unlock(&g_zte_ctrl_pdata->zte_lcd_ctrl->panel_sys_lock);
	return count;
}
static DEVICE_ATTR(lcd_cabc, 0664,  lcd_cabc_show, lcd_cabc_store);
static struct attribute *lcd_cabc_attributes[] = {
	&dev_attr_lcd_cabc.attr,
	NULL
};
static struct attribute_group lcd_cabc_attribute_group = {
	.attrs = lcd_cabc_attributes
};
int zte_create_cabc_sys(struct sprd_dispc *dispc)
{
	int err;
	struct kobject *lcd_cabc_kobj;

	lcd_cabc_kobj = kobject_create_and_add("cabc", g_zte_ctrl_pdata->zte_lcd_ctrl->kobj);
	if (!lcd_cabc_kobj) {
		err = -EINVAL;
		ZTE_LCD_ERROR("%s:ERROR Unable to create lcd_cabc_kobj.\n", __func__);
		return -EIO;
	}
	err = sysfs_create_group(lcd_cabc_kobj, &lcd_cabc_attribute_group);
	if (err != 0) {
		ZTE_LCD_ERROR("%s:ERROR lcd_cabc_kobj failed.\n", __func__);
		kobject_put(lcd_cabc_kobj);
		return -EIO;
	}
	ZTE_LCD_INFO("%s:succeeded.\n", __func__);
	return err;
}
#endif
/********************extreme power save mode cabc end*****************/

/********************lcd common function start*****************/

#ifdef CONFIG_ZTE_LCD_TP_GESTURE_POWER_OFF_SEQ
static int of_parse_tp_gesture_power_off_seq(struct device_node *node,
				struct panel_info *panel){
	struct property *prop = NULL;
	int bytes = 0;
	unsigned int *p = NULL;
	int rc = 0;

	g_zte_ctrl_pdata->zte_lcd_ctrl->have_tp_gesture_power_off_seq =
			of_property_read_bool(node, "zte,have-tp-gesture-power_off-sequence");
	if (g_zte_ctrl_pdata->zte_lcd_ctrl->have_tp_gesture_power_off_seq) {
		prop = of_find_property(node, "power-off-tp-gesture-sequence", &bytes);
		if (!prop) {
			pr_err("error: power-off-tp-gesture-sequence property not found");
			return -EINVAL;
		}

		p = kzalloc(bytes, GFP_KERNEL);
		if (!p)
			return -ENOMEM;
		rc = of_property_read_u32_array(node,
				"power-off-tp-gesture-sequence", p, bytes / 4);
		if (rc) {
			pr_err("get power-off-sequence failed\n");
			kfree(p);
			return -EINVAL;
		}

		panel->pwr_off_tp_gesture_seq.items = bytes / 12;
		panel->pwr_off_tp_gesture_seq.timing = (struct gpio_timing *)p;

	}
	return 0;
}
#endif

static ssize_t zte_show_esd_num(struct device *d, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "esd num:0x%x\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_esd_num);
}
static DEVICE_ATTR(esd_num, 0660, zte_show_esd_num, NULL);

static struct attribute *zte_lcd_fs_attrs[] = {
	&dev_attr_esd_num.attr,
	NULL,
};

static struct attribute_group zte_lcd_attrs_group = {
	.attrs = zte_lcd_fs_attrs,
};

static void zte_lcd_common_init(struct LCM_DRIVER *lcm_drv)
{
	int ret = 0;

	g_zte_ctrl_pdata->zte_lcd_ctrl = kzalloc(sizeof(struct zte_lcd_ctrl_data), GFP_KERNEL);
	if (!g_zte_ctrl_pdata->zte_lcd_ctrl) {
		ZTE_LCD_ERROR("no mem to save zte_lcd_ctrl_data info: kzalloc fail\n");
		return;
	}

	ZTE_LCD_INFO("%s:alloc zte_lcd_ctrl_data success!\n", __func__);

	/*create /sys/lcd_sys/ path to add other lcd ctrl point*/
	g_zte_ctrl_pdata->zte_lcd_ctrl->kobj = kobject_create_and_add("lcd_sys", NULL);
	if (!g_zte_ctrl_pdata->zte_lcd_ctrl->kobj) {
		ZTE_LCD_ERROR("%s:create lcd_sys error!\n", __func__);
	} else {
		ret = sysfs_create_group(g_zte_ctrl_pdata->zte_lcd_ctrl->kobj, &zte_lcd_attrs_group);
		if (ret)
			ZTE_LCD_ERROR("sysfs group creation failed, rc=%d\n", ret);
	}

	/*this assignment is must bu in the end of g_zte_ctrl_pdata->zte_lcd_ctrl = kzalloc() */

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_powerdown_for_shutdown = false;

}
void zte_lcd_common_func(struct LCM_DRIVER *lcm_drv, struct LCM_UTIL_FUNCS *utils)
{
	if (g_zte_ctrl_pdata) {
		return;
	}

	g_zte_ctrl_pdata = lcm_drv;

	zte_lcd_common_init(lcm_drv);

	zte_lcd_proc_info_display(lcm_drv);
#ifdef CONFIG_ZTE_LCD_REG_DEBUG
		zte_lcd_reg_debug_func(lcm_drv, utils);
#endif

#ifdef CONFIG_ZTE_LCD_CABC3_EXTREME_POWER_SAVE
	mutex_init(&g_zte_ctrl_pdata->zte_lcd_ctrl->panel_sys_lock);
	zte_create_cabc_sys(g_zte_ctrl_pdata);
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_set_cabc_mode = zte_set_panel_cabc;
#endif

}
