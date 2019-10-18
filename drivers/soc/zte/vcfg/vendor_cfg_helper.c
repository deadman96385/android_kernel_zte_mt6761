/*=========================================
*  Head Files :
* =========================================
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

/*=========================================
*  Defines :
* =========================================
*/
#define PRIVILEGE_ON     1
#define PRIVILEGE_OFF    0

/*=========================================
*  Variables :
* =========================================
*/
static int privilege_state = PRIVILEGE_OFF;

/*=========================================
*  Functions :
* =========================================
*/
static int privilege_state_set(const char *val, const struct kernel_param *kp)
{
	int rc;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("vendor_cfg_helper, %s error: %d\n", __func__, rc);
		return rc;
	}

	pr_info("vendor_cfg_helper, privilege_state is %d\n", privilege_state);

	return 0;
}

static struct kernel_param_ops param_ops_privilege_state = {
	.set = privilege_state_set,
	.get = param_get_int,
};
module_param_cb(privilege_state, &param_ops_privilege_state, &privilege_state, 0600);
MODULE_PARM_DESC(privilege_state, "enable privilege state");

int request_privilege_state(void)
{
	return privilege_state == PRIVILEGE_ON;
}
EXPORT_SYMBOL(request_privilege_state);

/* Read and write interfaces for further use */
static ssize_t vcfghelper_read(struct file *file, char __user *buf,
					size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static ssize_t vcfghelper_write(struct file *file, const char __user *buf,
					size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static int vcfghelper_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int vcfghelper_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations vcfghelper_fops = {
	.owner		= THIS_MODULE,
	.read		= vcfghelper_read,
	.write		= vcfghelper_write,
	.open		= vcfghelper_open,
	.release	= vcfghelper_release,
	.llseek		= no_llseek,
};

static struct miscdevice vcfg_helper_dev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "vcfghelper",
	.fops		= &vcfghelper_fops,
};

static int __init vendor_cfg_helper_init(void)
{
	int err;

	pr_info("vendor_cfg_helper_init\n");

	err = misc_register(&vcfg_helper_dev);
	if (err) {
		pr_err("vendor_cfg_helper dev register failed, err = %d\n", err);
		return err;
	}
	return 0;
}

static void vendor_cfg_helper_exit(void)
{
	misc_deregister(&vcfg_helper_dev);
}

fs_initcall(vendor_cfg_helper_init);
module_exit(vendor_cfg_helper_exit);

