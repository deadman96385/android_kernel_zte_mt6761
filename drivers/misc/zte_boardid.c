#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h> /* misc_register */
#include <linux/fs.h> /* struct file_operations */
#include <linux/device.h>

MODULE_LICENSE("Dual BSD/GPL");

#define debug(fmt, ...) \
	pr_debug("zte_boardid: " fmt, ##__VA_ARGS__)

#define MAX_SIZE 50

extern int sec_schip_enabled(void);

static int boardid = -1;
static int recovery = -1;

static int __init boardid_setup(char *str)
{
	long value;
	int err;

	err = kstrtol(str, 0, &value);
	if (err)
		return err;
	boardid = value;
	return 1;
}
__setup("androidboot.zte.hardinfo=", boardid_setup);

static int __init recoveryid_setup(char *str)
{
	long value;
	int err;

	err = kstrtol(str, 0, &value);
	if (err)
		return err;
	recovery = value;
	return 1;
}
__setup("recovery_id=", recoveryid_setup);

int zte_get_boardid(void)
{
	return boardid;
}
EXPORT_SYMBOL(zte_get_boardid);

static int boot_reason = -1;
static int __init boot_reason_setup(char *str)
{
	long value;
	int err;

	err = kstrtol(str, 0, &value);
	if (err)
		return err;
	boot_reason = value;
	return 1;
}
__setup("boot_reason=", boot_reason_setup);
int zte_get_boot_reason(void)
{
	return boot_reason;
}
EXPORT_SYMBOL(zte_get_boot_reason);
static ssize_t zte_boardid_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, MAX_SIZE, "%d\n", boardid);
}

static ssize_t zte_secstate_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int ret = 0;

	ret = sec_schip_enabled();
	pr_debug("sec_schip_enabled(%d)\n", ret);
	return snprintf(buf, MAX_SIZE, "%d\n", ret);
}

static int zte_boot_mod = 0;
static int __init boot_mod_setup(char *str)
{
	long value;
	int err;

	err = kstrtol(str, 0, &value);
	if (err)
		return err;
	zte_boot_mod = value;
	return 1;
}
__setup("zte_boot_mode=", boot_mod_setup);

static ssize_t zte_bootmod_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, MAX_SIZE, "%d\n", zte_boot_mod);
}

static ssize_t zte_recovery_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, MAX_SIZE, "%d\n", recovery);
}
static int zte_ddrsize_GB = 0;
static int __init ddrsize_setup(char *str)
{
	long value;
	int err;

	err = kstrtol(str, 0, &value);
	if (err)
		return err;
	zte_ddrsize_GB = value;
	return 1;
}
__setup("ddrsize_GB=", ddrsize_setup);

static ssize_t zte_ddrsize_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, MAX_SIZE, "%d\n", zte_ddrsize_GB);
}

static int workque_flag = 0;
static ssize_t zte_wq_debug_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	workque_flag = 1;
	return snprintf(buf, MAX_SIZE, "%d\n", workque_flag);
}

static DEVICE_ATTR(id, S_IRUGO, zte_boardid_show, NULL);
static DEVICE_ATTR(secstate, S_IRUGO, zte_secstate_show, NULL);
static DEVICE_ATTR(bootmod, S_IRUGO, zte_bootmod_show, NULL);
static DEVICE_ATTR(recovery, S_IRUGO, zte_recovery_show, NULL);
static DEVICE_ATTR(ddrsize, S_IRUGO, zte_ddrsize_show, NULL);
static DEVICE_ATTR(wq_debug, S_IRUGO, zte_wq_debug_show, NULL);

static struct attribute *mvd_attrs[] = {
	&dev_attr_id.attr,
	&dev_attr_secstate.attr,
	&dev_attr_bootmod.attr,
	&dev_attr_recovery.attr,
	&dev_attr_ddrsize.attr,
	&dev_attr_wq_debug.attr,
	NULL,
};

static struct attribute_group mvd_attr_group = {
	.attrs = mvd_attrs,
};

static int zte_boardid_misc_open(struct inode *inode, struct file *file)
{
	debug("open\n");
	return 0;
}

static long zte_boardid_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	debug("ioctl\n");

	return 0;
}

static struct file_operations zte_boardid_misc_fops = {
	.owner = THIS_MODULE,
	.open = zte_boardid_misc_open,
	.unlocked_ioctl = zte_boardid_misc_ioctl,
};

static struct miscdevice zte_boardid_misc_dev[] = {
	{
		.minor    = MISC_DYNAMIC_MINOR,
		.name    = "zte_boardid",
		.fops    = &zte_boardid_misc_fops,
	}
};

static int __init zte_boardid_init(void)
{
	int ret = 0;

	debug("zte_boardid init\n");

	ret = misc_register(&zte_boardid_misc_dev[0]);
	if (ret) {
		debug("fail to register misc driver: %d\n", ret);
		goto register_fail;
	}

	/*ret = device_create_file(zte_boardid_misc_dev[0].this_device, &dev_attr_id);*/
	ret  = sysfs_create_group(&zte_boardid_misc_dev[0].this_device->kobj, &mvd_attr_group);
	if (ret) {
		debug("fail to create file: %d\n", ret);
		goto register_fail;
	}
	ret = sec_schip_enabled();
	pr_debug("sec_schip_enabled(%d)\n", ret);

	return 0;

register_fail:
	return ret;
}

static void __exit zte_boardid_exit(void)
{
	debug("zte_boardid exit\n");
	sysfs_remove_group(&zte_boardid_misc_dev[0].this_device->kobj, &mvd_attr_group);
	misc_deregister(&zte_boardid_misc_dev[0]);
}

module_init(zte_boardid_init);
module_exit(zte_boardid_exit);
