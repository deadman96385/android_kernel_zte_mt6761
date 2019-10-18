/***********************
 * file : tpd_fw.c
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include "tpd_sys.h"

#define	LCD_PROC_FILE	"driver/lcd_id"

static struct proc_dir_entry *lcd_proc_entry;

struct tpd_classdev_t tpd_fw_cdev;
struct tp_rwreg_t tp_reg_rw;
static struct class *tsp_fw_class;
struct proc_dir_entry *tpd_proc_dir = NULL;

struct tpvendor_t synaptics_vendor_l[] = {
	{0x31, "TPK"},
	{0x32, "Truly"},
	{0x33, "Success"},
	{0x34, "Ofilm"},
	{0x35, "Lead"},
	{0x36, "Wintek"},
	{0x37, "Laibao"},
	{0x38, "CMI"},
	{0x39, "Ecw"},
	{0x41, "Goworld"},
	{0x42, "BaoMing"},
	{0x43, "Eachopto"},
	{0x44, "Mutto"},
	{0x45, "Junda"},
	{0x46, "BOE"},
	{0x47, "TianMa"},
	{0x48, "Samsung"},
	{0x49, "DiJing"},
	{0x50, "LCE"},
	{0x52, "HeLiTai"},
	{0x53, "JDI"},
	{0x54, "HuaXingDa"},
	{0x55, "Toptouch"},
	{0x56, "GVO"},
	{0x57, "Wally_panel"},
	{0x58, "zgsaihua"},
	{VENDOR_END, "Unknown"},
};

struct tpvendor_t focal_vendor_l[] = {
	{0x11, "TeMeiKe"},
	{0x15, "ChuangWei"},
	{0x51, "Ofilm"},
	{0x55, "LaiBao"},
	{0x57, "Goworld"},
	{0x5a, "Truly"},
	{0x5c, "TPK"},
	{0x5d, "BaoMing"},
	{0x5f, "Success"},
	{0x60, "Lead"},
	{0x67, "DiJing"},
	{0x80, "Eachopto"},
	{0x82, "HeLiTai"},
	{0x85, "JunDa"},
	{0x87, "LianChuang"},
	{0xda, "DiJingDA"},
	{0xf0, "TongXingDa"},
	{VENDOR_END, "Unknown"},
};

struct tpvendor_t cypress_vendor_l[] = {
	{0x01, "TPK"},
	{0x02, "Truly"},
	{0x03, "Success"},
	{0x04, "Ofilm"},
	{0x05, "Lead"},
	{0x06, "Wintek"},
	{0x07, "Laibao"},
	{0x08, "CMI"},
	{0x09, "Ecw"},
	{0x0a, "Goworld"},
	{0x0b, "BaoMing"},
	{0x0c, "Eachopto"},
	{0x0d, "Mutto"},
	{0x0e, "Junda"},
	{VENDOR_END, "Unknown"},
};

struct tpvendor_t atmel_vendor_l[] = {
	{0x06, "Wintek"},
	{0x07, "Laibao"},
	{0x08, "CMI"},
	{0x09, "Ecw"},
	{VENDOR_END, "Unknown"},
};
static int get_chip_vendor(struct tpvendor_t *vendor_l, int count, int vendor_id, char *vendor_name)
{
	int i = 0;
	int vendorname_len = 0;

	pr_info("%s: count: %d.\n", __func__, count);

	for (i = 0; i < count; i++) {
		if (vendor_l[i].vendor_id == vendor_id || VENDOR_END == vendor_l[i].vendor_id) {
			vendorname_len = strlen(vendor_l[i].vendor_name);
			vendorname_len = vendorname_len >= MAX_VENDOR_NAME_LEN ?
				MAX_VENDOR_NAME_LEN - 1 : vendorname_len;
			strlcpy(vendor_name,  vendor_l[i].vendor_name, vendorname_len + 1);
			break;
		}
	}

	return 0;
}

static void tpd_get_tp_module_name(struct tpd_classdev_t *cdev)
{
	int size = 0;

	if (cdev == NULL) {
		pr_err("tpd: %s cdev is NULL.\n", __func__);
		return;
	}
	switch (cdev->ic_tpinfo.chip_model_id) {
	case TS_CHIP_SYNAPTICS:
		size = ARRAY_SIZE(synaptics_vendor_l);
		get_chip_vendor(synaptics_vendor_l, size, cdev->ic_tpinfo.module_id, cdev->ic_tpinfo.vendor_name);
		break;
	case TS_CHIP_ATMEL:
		size = ARRAY_SIZE(atmel_vendor_l);
		get_chip_vendor(atmel_vendor_l, size, cdev->ic_tpinfo.module_id, cdev->ic_tpinfo.vendor_name);
		break;
	case TS_CHIP_CYTTSP:
		size =  ARRAY_SIZE(cypress_vendor_l);
		get_chip_vendor(cypress_vendor_l, size, cdev->ic_tpinfo.module_id, cdev->ic_tpinfo.vendor_name);
		break;
	case TS_CHIP_FOCAL:
		size = ARRAY_SIZE(focal_vendor_l);
		get_chip_vendor(focal_vendor_l, size, cdev->ic_tpinfo.module_id, cdev->ic_tpinfo.vendor_name);
		break;
	case TS_CHIP_GOODIX:
	case TS_CHIP_MELFAS:
	case TS_CHIP_MSTAR:
	case TS_CHIP_ILITEK:
	case TS_CHIP_TLSC:
		break;
	default:
		pr_info("fun:%s chip_model_id Unknown.\n", __func__);
		break;
	}
	pr_info("fun:%s module name:%s.\n", __func__, cdev->ic_tpinfo.vendor_name);
}

static ssize_t tsp_fw_ic_tpinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);

	if (cdev->get_tpinfo) {
		cdev->get_tpinfo(cdev);
	}

	return snprintf(buf, 64, "%u 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x %s\n",
		cdev->ic_tpinfo.chip_part_id,   cdev->ic_tpinfo.chip_model_id,
		cdev->ic_tpinfo.chip_ver,        cdev->ic_tpinfo.module_id,
		cdev->ic_tpinfo.firmware_ver, cdev->ic_tpinfo.config_ver,
		cdev->ic_tpinfo.i2c_type,        cdev->ic_tpinfo.i2c_addr,
		cdev->ic_tpinfo.tp_name);
}

static ssize_t tsp_fw_ic_tpinfo_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

/**for tpd test*/
static ssize_t tsp_test_save_file_path_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_save_filepath) {
		retval = cdev->tpd_test_get_save_filepath(cdev, buf);
	}

	return retval;
}

static ssize_t tsp_test_save_file_path_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_set_save_filepath) {
		retval = cdev->tpd_test_set_save_filepath(cdev, buf);
	}

	return count;
}

static ssize_t tsp_test_save_file_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_save_filename) {
		retval = cdev->tpd_test_get_save_filename(cdev, buf);
	}

	return retval;
}

static ssize_t tsp_test_save_file_name_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_set_save_filename) {
		retval = cdev->tpd_test_set_save_filename(cdev, buf);
	}

	return count;
}

static ssize_t tsp_test_ini_file_path_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_ini_filepath) {
		retval = cdev->tpd_test_get_ini_filepath(cdev, buf);
	}

	return retval;
}

static ssize_t tsp_test_ini_file_path_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_set_ini_filepath) {
		retval = cdev->tpd_test_set_ini_filepath(cdev, buf);
	}

	return count;
}

static ssize_t tsp_test_filename_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_filename) {
		retval = cdev->tpd_test_get_filename(cdev, buf);
	}

	return retval;
}

static ssize_t tsp_test_filename_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_set_filename) {
		retval = cdev->tpd_test_set_filename(cdev, buf);
	}

	return count;
}

static ssize_t tsp_test_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_cmd) {
		retval = cdev->tpd_test_get_cmd(cdev, buf);
	}

	return retval;
}

static ssize_t tsp_test_cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_set_cmd) {
		retval = cdev->tpd_test_set_cmd(cdev, buf);
	}

	return count;
}

static ssize_t tsp_test_node_data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_node_data) {
		retval = cdev->tpd_test_get_node_data(cdev, buf);
	}

	return retval;
}

static ssize_t tsp_test_node_data_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_set_node_data_type) {
		retval = cdev->tpd_test_set_node_data_type(cdev, buf);
	}

	return count;
}
static ssize_t tsp_test_channel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_channel_info) {
		retval = cdev->tpd_test_get_channel_info(cdev, buf);
	}

	return retval;
}
static ssize_t tsp_test_result_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_result) {
		retval = cdev->tpd_test_get_result(cdev, buf);
	}

	return retval;
}

static DEVICE_ATTR(tpinfo, 0644, tsp_fw_ic_tpinfo_show, tsp_fw_ic_tpinfo_store);
/**for tpd test*/
static DEVICE_ATTR(tpd_test_result, S_IRUGO|S_IRUSR, tsp_test_result_show, NULL);
static DEVICE_ATTR(tpd_test_channel_setting, S_IRUGO|S_IRUSR, tsp_test_channel_show, NULL);
static DEVICE_ATTR(tpd_test_save_file_path, S_IRUGO|S_IWUSR,
						tsp_test_save_file_path_show, tsp_test_save_file_path_store);
static DEVICE_ATTR(tpd_test_save_file_name, S_IRUGO|S_IWUSR,
						tsp_test_save_file_name_show, tsp_test_save_file_name_store);
static DEVICE_ATTR(tpd_test_ini_file_path, S_IRUGO|S_IWUSR, tsp_test_ini_file_path_show, tsp_test_ini_file_path_store);
static DEVICE_ATTR(tpd_test_filename, S_IRUGO|S_IWUSR, tsp_test_filename_show, tsp_test_filename_store);
static DEVICE_ATTR(tpd_test_cmd, S_IRUGO|S_IWUSR, tsp_test_cmd_show, tsp_test_cmd_store);
static DEVICE_ATTR(tpd_test_node_data, S_IRUGO|S_IWUSR, tsp_test_node_data_show, tsp_test_node_data_store);

static struct attribute *tsp_dev_attrs[] = {
	&dev_attr_tpinfo.attr,
	/**for tpd test*/
	&dev_attr_tpd_test_filename.attr,
	&dev_attr_tpd_test_node_data.attr,
	&dev_attr_tpd_test_cmd.attr,
	&dev_attr_tpd_test_ini_file_path.attr,
	&dev_attr_tpd_test_save_file_path.attr,
	&dev_attr_tpd_test_save_file_name.attr,
	&dev_attr_tpd_test_channel_setting.attr,
	&dev_attr_tpd_test_result.attr,
	NULL,
};

static const struct attribute_group tsp_dev_attribute_group = {
	.attrs = tsp_dev_attrs,
	/**.bin_attrs = tsp_dev_bin_attributes,*/
};

static const struct attribute_group *tsp_dev_attribute_groups[] = {
	&tsp_dev_attribute_group,
	NULL,
};
static ssize_t tsp_fw_reg_read(struct file *filp, struct kobject *kobj,
				  struct bin_attribute *bin_attr,
				  char *buffer, loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = -1;

	/**pr_info("tpd: %s offset %lld byte, count %ld byte.\n", __func__, offset, count);*/

	if (cdev->read_block) {
		cdev->read_block(cdev, offset, buffer, count);
	}

	retval = count;

	return retval;
}

static ssize_t tsp_fw_reg_write(struct file *filp, struct kobject *kobj,
				   struct bin_attribute *bin_attr,
				   char *buffer, loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = -1;

	/**pr_info("tpd: %s offset %lld byte, count %ld byte.\n", __func__, offset, count);*/

	if (cdev->write_block) {
		cdev->write_block(cdev, offset, buffer, count);
	}

	retval = count;

	return retval;
}

static struct bin_attribute firmware_attr_reg = {
	.attr = { .name = "reg", .mode = 0644 },
	.size = 0,
	.read = tsp_fw_reg_read,
	.write = tsp_fw_reg_write,
};

int tpd_gpio_shutdown_config(void)
{
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (cdev->tpd_gpio_shutdown) {
		cdev->tpd_gpio_shutdown();
	}
	return 0;
}

int suspend_tp_need_awake(void)
{
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (cdev->tpd_suspend_need_awake) {
		return cdev->tpd_suspend_need_awake(cdev);
	}
	return 0;
}

bool tp_esd_check(void)
{
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (cdev->tpd_esd_check) {
		return cdev->tpd_esd_check(cdev);
	}
	return 0;
}

void set_lcd_reset_processing(bool enable)
{
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (enable) {
		cdev->lcd_reset_processing = true;
	} else {
		cdev->lcd_reset_processing = false;
	}
	pr_info("cdev->lcd_reset_processing is %d.\n", cdev->lcd_reset_processing);
}

static int tp_reg_read(
		u8 addr,
		u8 *data,
		int len)
{
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;
	int ret = 0;

	if (cdev->tp_i2c_reg_read) {
		ret = cdev->tp_i2c_reg_read(cdev, addr, data, len);
	} else {
		ret = -EPERM;
	}
	return ret;
}
static int tp_reg_write(
		u8 addr,
		u8 *data,
		int len)
{
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;
	int ret = 0;

	if (cdev->tp_i2c_reg_write) {
		ret = cdev->tp_i2c_reg_write(cdev, addr, data, len);
	} else {
		ret = -EPERM;
	}
	return ret;
}
static int tp_16bor32b_reg_read(
		u32 addr,
		u8 *data,
		int len)
{
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;
	int ret = 0;

	if (cdev->tp_i2c_16bor32b_reg_read) {
		ret = cdev->tp_i2c_16bor32b_reg_read(cdev, addr, data, len);
	} else {
		ret = -EPERM;
	}
	return ret;
}
static int tp_16bor32b_reg_write(
		u32 addr,
		u8 *data,
		int len)
{
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;
	int ret = 0;

	if (cdev->tp_i2c_16bor32b_reg_write) {
		ret = cdev->tp_i2c_16bor32b_reg_write(cdev, addr, data, len);
	} else {
		ret = -EPERM;
	}
	return ret;
}
static int str_to_int(const char *hex_buf, int size)
{
	int i = 0;
	int base = 1;
	int value = 0;
	char single = 0;

	for (i = size - 1; i >= 0; i--) {
		single = hex_buf[i];

		if ((single >= '0') && (single <= '9')) {
			value += (single - '0') * base;
		} else if ((single >= 'a') && (single <= 'z')) {
			value += (single - 'a' + 10) * base;
		} else if ((single >= 'A') && (single <= 'Z')) {
			value += (single - 'A' + 10) * base;
		} else {
			return -EINVAL;
		}

		base *= 16;
	}

	return value;
}
static u8 str_to_u8(const char *hex_buf, int size)
{
	return (u8) str_to_int(hex_buf, size);
}
static int tp_parse_buf(const char *buf, size_t cmd_len)
{
	int length = 0;
	int i = 0;

	tp_reg_rw.reg = str_to_int(buf + 1, tpd_fw_cdev.reg_char_num);
	length = str_to_int(buf + 1 + tpd_fw_cdev.reg_char_num, 2);

	if (buf[0] == '1') {
		tp_reg_rw.len = length;
		tp_reg_rw.type = REG_OP_READ;
		pr_info("read %02X, %d bytes", tp_reg_rw.reg, tp_reg_rw.len);
	} else {
		if (cmd_len < (length * 2 + 3 + tpd_fw_cdev.reg_char_num)) {
			pr_err("data invalided!\n");
			return -EINVAL;
		}
		pr_info("write %02X, %d bytes", tp_reg_rw.reg, length);

		/* first byte is the register addr */
		tp_reg_rw.type = REG_OP_WRITE;
		tp_reg_rw.len = length;
	}

	if (tp_reg_rw.len > 0) {
		tp_reg_rw.opbuf = kzalloc(tp_reg_rw.len, GFP_KERNEL);
		if (tp_reg_rw.opbuf == NULL) {
			pr_err("allocate memory failed!\n");
			return -ENOMEM;
		}

		if (tp_reg_rw.type == REG_OP_WRITE) {
			pr_info("write buffer:\n");
			for (i = 0; i < tp_reg_rw.len; i++) {
				tp_reg_rw.opbuf[i] = str_to_u8(buf + 3 + tpd_fw_cdev.reg_char_num + i * 2, 2);
				pr_info("buf[%d]: %02X\n", i, tp_reg_rw.opbuf[i] & 0xFF);
			}
		}
	}

	return tp_reg_rw.len;
}
static ssize_t tprwreg_show(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	int i = 0;
	int length = 0;
	uint8_t buf[500] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	mutex_lock(&cdev->cmd_mutex);

	if (tp_reg_rw.len < 0) {
		count = snprintf(buf, sizeof(buf), "Invalid cmd line\n");
	} else if (tp_reg_rw.len == 1) {
		if (tp_reg_rw.type == REG_OP_READ) {
			if (tp_reg_rw.res == 0) {
				count = snprintf(buf, sizeof(buf), "Read reg 0x%02X: 0x%02X\n",
					tp_reg_rw.reg, tp_reg_rw.val);
			} else {
				count =
				    snprintf(buf, sizeof(buf), "Read reg 0x%02X failed, ret: %d\n", tp_reg_rw.reg,
					     tp_reg_rw.res);
			}
		} else {
			if (tp_reg_rw.res == 0) {
				count =
				    snprintf(buf, sizeof(buf), "Write reg 0x%02X, 0x%02X success\n", tp_reg_rw.reg,
					     tp_reg_rw.val);
			} else {
				count =
				    snprintf(buf, sizeof(buf), "Write reg 0x%02X failed, ret: %d\n", tp_reg_rw.reg,
					     tp_reg_rw.res);
			}
		}
	} else {
		if (tp_reg_rw.type == REG_OP_READ) {
			length = tp_reg_rw.len - 1;
			length = (length < 0) ? 0 : length;
			count =
			    snprintf(buf, sizeof(buf), "Read Reg: [0x%02X]-[0x%02X]\n", tp_reg_rw.reg,
				     tp_reg_rw.reg + length);
			count += snprintf(buf + count, sizeof(buf) - count, "Result: ");
			if (tp_reg_rw.res) {
				count += snprintf(buf + count, sizeof(buf) - count, "failed, ret: %d\n", tp_reg_rw.res);
			} else {
				if (tp_reg_rw.opbuf) {
					for (i = 0; i < tp_reg_rw.len; i++) {
						count += snprintf(buf + count, sizeof(buf) - count, "0x%02X ",
							tp_reg_rw.opbuf[i]);
					}
					count += snprintf(buf + count, sizeof(buf) - count, "\n");
				}
			}
		} else {
			length = tp_reg_rw.len - 1;
			length = (length < 0) ? 0 : length;
			count =
			    snprintf(buf, sizeof(buf), "Write Reg: [0x%02X]-[0x%02X]\n", tp_reg_rw.reg,
				     tp_reg_rw.reg + length);
			count += snprintf(buf + count, sizeof(buf) - count, "Write Data: ");
			if (tp_reg_rw.opbuf) {
				for (i = 0; i < tp_reg_rw.len; i++) {
					count += snprintf(buf + count, sizeof(buf) - count, "0x%02X ",
						tp_reg_rw.opbuf[i]);
				}
				count += snprintf(buf + count, sizeof(buf) - count, "\n");
			}
			if (tp_reg_rw.res) {
				count += snprintf(buf + count, sizeof(buf) - count, "Result: failed, ret: %d\n",
						tp_reg_rw.res);
			} else {
				count += snprintf(buf + count, sizeof(buf) - count, "Result: success\n");
			}
		}
	}
	if (tp_reg_rw.opbuf != NULL) {
		kfree(tp_reg_rw.opbuf);
		tp_reg_rw.opbuf = NULL;
	}
	mutex_unlock(&cdev->cmd_mutex);

	return simple_read_from_buffer(buffer, count, offset, buf, count);
}
/********************************************************************************************
 * Format buf:
 * [0]: '0' write, '1' read
 * [1-2]: addr, hex
 * [3-4]: length, hex
 * [5-6]...[n-(n+1)]: data, hex
 * read reg 00:echo 00 > rw_reg
 * write reg 00 value 01:echo 0001 > rw_reg
 * read reg [00 -09] :echo 1000A > rw_reg
 * write reg [00 -09] value 00,01,02,03,04,05,06,07,08,09 :echo 0000A00010203040506070809 > rw_reg
 ******************************************************************************************/
static ssize_t tprwreg_store(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	size_t cmd_length = 0;
	int ret = 0;
	char buf[500] = {0};
	u8 reg = 0, val = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	mutex_lock(&cdev->cmd_mutex);
	cmd_length = len - 1;

	if (tp_reg_rw.opbuf != NULL) {
		kfree(tp_reg_rw.opbuf);
		tp_reg_rw.opbuf = NULL;
	}
	if (cmd_length > sizeof(buf))
		goto INVALID_CMD;
	ret = copy_from_user(buf, buffer, cmd_length);

	pr_info("cmd len: %d, buf: %s.\n", (int)cmd_length, buf);
	/* compatible old ops */
	if (cmd_length == tpd_fw_cdev.reg_char_num) {
		tp_reg_rw.type = REG_OP_READ;
		tp_reg_rw.len = 1;
		tp_reg_rw.reg = str_to_int(buf, tpd_fw_cdev.reg_char_num);
	} else if (cmd_length == 2 + tpd_fw_cdev.reg_char_num) {
		tp_reg_rw.type = REG_OP_WRITE;
		tp_reg_rw.len = 1;
		tp_reg_rw.reg = str_to_int(buf, tpd_fw_cdev.reg_char_num);
		tp_reg_rw.val = str_to_int(buf + tpd_fw_cdev.reg_char_num, 2);

	} else if (cmd_length < 3 + tpd_fw_cdev.reg_char_num) {
		pr_err("Invalid cmd buffer.\n");
		mutex_unlock(&cdev->cmd_mutex);
		goto INVALID_CMD;
	} else {
		tp_reg_rw.len = tp_parse_buf(buf, cmd_length);
	}
	if (tp_reg_rw.len < 0) {
		pr_err("cmd buffer error!\n");

	} else {
		if (tp_reg_rw.type == REG_OP_READ) {
			if (tp_reg_rw.len == 1) {
				reg = tp_reg_rw.reg & 0xFF;
				if (tpd_fw_cdev.reg_char_num == REG_CHAR_NUM_2) {
					tp_reg_rw.res = tp_reg_read(reg, &val, 1);
				} else {
					tp_reg_rw.res = tp_16bor32b_reg_read(tp_reg_rw.reg, &val, 1);
				}
				tp_reg_rw.val = val;
			} else {
				reg = tp_reg_rw.reg & 0xFF;
				if (tpd_fw_cdev.reg_char_num == REG_CHAR_NUM_2) {
					tp_reg_rw.res = tp_reg_read(reg, tp_reg_rw.opbuf, tp_reg_rw.len);
				} else {
					tp_reg_rw.res =
						tp_16bor32b_reg_read(tp_reg_rw.reg, tp_reg_rw.opbuf, tp_reg_rw.len);
				}
			}

			if (tp_reg_rw.res < 0) {
				pr_err("Could not read 0x%02x.\n", tp_reg_rw.reg);
			} else {
				pr_info("read 0x%02x, %d bytes successful.\n", tp_reg_rw.reg, tp_reg_rw.len);
				tp_reg_rw.res = 0;
			}

		} else {
			if (tp_reg_rw.len == 1) {
				reg = tp_reg_rw.reg & 0xFF;
				val = tp_reg_rw.val & 0xFF;
				if (tpd_fw_cdev.reg_char_num == REG_CHAR_NUM_2) {
					tp_reg_rw.res = tp_reg_write(reg, &val, 1);
				} else {
					tp_reg_rw.res = tp_16bor32b_reg_write(tp_reg_rw.reg, &val, 1);
				}
			} else {
				reg = tp_reg_rw.reg & 0xFF;
				if (tpd_fw_cdev.reg_char_num == REG_CHAR_NUM_2) {
					tp_reg_rw.res =  tp_reg_write(reg, tp_reg_rw.opbuf, tp_reg_rw.len);
				} else {
					tp_reg_rw.res =
						tp_16bor32b_reg_write(tp_reg_rw.reg, tp_reg_rw.opbuf, tp_reg_rw.len);
				}
			}
			if (tp_reg_rw.res < 0) {
				pr_err("Could not write 0x%02x.\n", tp_reg_rw.reg);

			} else {
				pr_info("Write 0x%02x, %d bytes successful.\n", tp_reg_rw.reg, tp_reg_rw.len);
				tp_reg_rw.res = 0;
			}
		}
	}
	mutex_unlock(&cdev->cmd_mutex);

	return len;
INVALID_CMD:
	return -EINVAL;
}
static ssize_t tp_rawdata_read(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	char *buf = NULL;
	int len = 0;
	int ret = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	mutex_lock(&cdev->cmd_mutex);
	buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("tpd: %s kzalloc failed\n", __func__);
		mutex_unlock(&cdev->cmd_mutex);
		return -ENOMEM;
	}
	if (cdev->tpd_test_get_tp_rawdata) {
		len = cdev->tpd_test_get_tp_rawdata(buf, PAGE_SIZE);
	}
	pr_notice("tpd: tp_rawdata_read:%d\n", len);
	ret = simple_read_from_buffer(buffer, count, offset, buf, len);
	kfree(buf);
	mutex_unlock(&cdev->cmd_mutex);
	return ret;
}

static ssize_t tp_module_info_read(struct file *file,
	char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t buffer_tpd[200];
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	if (cdev->get_tpinfo) {
	cdev->get_tpinfo(cdev);
	}
	tpd_get_tp_module_name(cdev);
	len += snprintf(buffer_tpd + len, sizeof(buffer_tpd) - len, "TP module: %s(0x%x)\n",
			cdev->ic_tpinfo.vendor_name, cdev->ic_tpinfo.module_id);
	len += snprintf(buffer_tpd + len, sizeof(buffer_tpd) - len, "IC type : %s\n",
			cdev->ic_tpinfo.tp_name);
	len += snprintf(buffer_tpd + len, sizeof(buffer_tpd) - len, "I2C address: 0x%x\n",
			cdev->ic_tpinfo.i2c_addr);
	len += snprintf(buffer_tpd + len, sizeof(buffer_tpd) - len, "Firmware version : 0x%x\n",
			cdev->ic_tpinfo.firmware_ver);
	if (cdev->ic_tpinfo.config_ver)
		len += snprintf(buffer_tpd + len, sizeof(buffer_tpd) - len, "Config version:0x%x\n",
			cdev->ic_tpinfo.config_ver);
	if (cdev->ic_tpinfo.display_ver)
		len += snprintf(buffer_tpd + len, sizeof(buffer_tpd) - len, "Display version:0x%x\n",
			cdev->ic_tpinfo.display_ver);
	return simple_read_from_buffer(buffer, count, offset, buffer_tpd, len);
}
static ssize_t tp_wake_gesture_read(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	if (cdev->get_gesture) {
		cdev->get_gesture(cdev);
	}
	pr_notice("tpd: %s val:%d.\n", __func__, cdev->b_gesture_enable);

	len = snprintf(data_buf, sizeof(data_buf), "%u\n", cdev->b_gesture_enable);
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t tp_wake_gesture_write(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	char data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	len = len >= sizeof(data_buf) ? sizeof(data_buf) - 1 : len;
	ret = copy_from_user(data_buf, buffer, len);
	if (ret)
		return -EINVAL;
	ret = kstrtouint(data_buf, 0, &input);
	if (ret)
		return -EINVAL;
	input = input > 0 ? 1 : 0;
	pr_notice("tpd: %s val %d.\n", __func__, input);
	if (cdev->wake_gesture) {
		cdev->wake_gesture(cdev, input);
	}
	return len;
}
static ssize_t tp_smart_cover_read(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	if (cdev->get_smart_cover) {
		cdev->get_smart_cover(cdev);
	}
	pr_notice("tpd: %s val:%d.\n", __func__, cdev->b_smart_cover_enable);
	len = snprintf(data_buf, sizeof(data_buf), "%u\n", cdev->b_smart_cover_enable);
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t tp_smart_cover_write(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	char data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	len = len >= sizeof(data_buf) ? sizeof(data_buf) - 1 : len;
	ret = copy_from_user(data_buf, buffer, len);
	if (ret)
		return -EINVAL;
	ret = kstrtouint(data_buf, 0, &input);
	if (ret)
		return -EINVAL;
	input = input > 0 ? 1 : 0;
	pr_notice("tpd: %s val %d.\n", __func__, input);
	if (cdev->set_smart_cover) {
		cdev->set_smart_cover(cdev, input);
	}
	return len;
}
static ssize_t tpfwupgrade_store(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	char *fwname = NULL;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	fwname = kmalloc(len, GFP_KERNEL);
	if (fwname == NULL) {
		pr_err("tpd: %s kmalloc failed\n", __func__);
		return -ENOMEM;
	}
	memset(fwname, 0, len);
	ret = copy_from_user(fwname, buffer, len);
	if (ret) {
		kfree(fwname);
		return -EINVAL;
	}
	pr_notice("%s, fwname: %s.\n", __func__, fwname);
	if (cdev->tp_fw_upgrade) {
		cdev->tp_fw_upgrade(cdev, fwname, len);
	}
	kfree(fwname);
	return len;
}

static ssize_t lcd_proc_read_val(struct file *file,
	char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	char data[800];
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	len += sprintf(data + len, "IC:%s; Vendor:%s; Resolution:%d*%d.\n", cdev->lcm_chip_info, cdev->lcm_info,
		DISP_GetScreenWidth(), DISP_GetScreenHeight());

	return simple_read_from_buffer(buffer, count, offset, data, len);
}

static ssize_t lcd_proc_write_val(struct file *filp,
					 const char *buff, size_t len,
					 loff_t * off)
{
	return len;
}

static struct file_operations lcd_proc_ops = {
	.read = lcd_proc_read_val,
	.write = lcd_proc_write_val,
};

static void create_lcd_proc_entry(void)
{
	lcd_proc_entry = proc_create(LCD_PROC_FILE, 0644, NULL, &lcd_proc_ops);
	if (lcd_proc_entry) {
		printk(KERN_INFO "create proc file sucess!\n");
	} else
		printk(KERN_INFO "create proc file failed!\n");
}

static ssize_t suspend_show(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[30] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	if (cdev->tp_suspend_show) {
		cdev->tp_suspend_show(cdev);
	}
	pr_notice("tpd: %s val:%d.\n", __func__, cdev->tp_suspend);
	len = snprintf(data_buf, sizeof(data_buf), "tp suspend is: %u\n", cdev->tp_suspend);
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t suspend_store(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	char data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	len = len >= sizeof(data_buf) ? sizeof(data_buf) - 1 : len;
	ret = copy_from_user(data_buf, buffer, len);
	if (ret)
		return -EINVAL;
	ret = kstrtouint(data_buf, 0, &input);
	if (ret)
		return -EINVAL;
	input = input > 0 ? 1 : 0;
	pr_notice("tpd: %s val %d.\n", __func__, input);
	if (cdev->set_tp_suspend) {
		cdev->set_tp_suspend(cdev, input);
	}
	return len;
}

static const struct file_operations proc_ops_tp_module_Info = {
	.owner = THIS_MODULE,
	.read = tp_module_info_read,
};
static const struct file_operations proc_ops_wake_gesture = {
	.owner = THIS_MODULE,
	.read = tp_wake_gesture_read,
	.write = tp_wake_gesture_write,
};
static const struct file_operations proc_ops_smart_cover = {
	.owner = THIS_MODULE,
	.read = tp_smart_cover_read,
	.write = tp_smart_cover_write,
};
static const struct file_operations proc_ops_rwreg = {
	.owner = THIS_MODULE,
	.read = tprwreg_show,
	.write = tprwreg_store,
};
static const struct file_operations proc_ops_tpfwupgrade = {
	.owner = THIS_MODULE,
	.write = tpfwupgrade_store,
};
static const struct file_operations proc_ops_tprawdata_read = {
	.owner = THIS_MODULE,
	.read = tp_rawdata_read,
};

static const struct file_operations proc_ops_suspend = {
	.owner = THIS_MODULE,
	.read = suspend_show,
	.write = suspend_store,
};

static void create_tpd_proc_entry(void)
{
	struct proc_dir_entry *tpd_proc_entry = NULL;

	tpd_proc_dir = proc_mkdir(PROC_TOUCH_DIR, NULL);
	if (tpd_proc_dir == NULL) {
		pr_err("%s: mkdir touchscreen failed!\n",  __func__);
		return;
	}
	tpd_proc_entry = proc_create(PROC_TOUCH_INFO, 0664, tpd_proc_dir, &proc_ops_tp_module_Info);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create ts_information failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_WAKE_GESTURE, 0664,  tpd_proc_dir, &proc_ops_wake_gesture);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create wake_gesture failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_SMART_COVER, 0664, tpd_proc_dir, &proc_ops_smart_cover);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create smart_cover failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_RW_REG, 0664,  tpd_proc_dir, &proc_ops_rwreg);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create rw_reg!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_FW_UPGRADE, 0664,  tpd_proc_dir, &proc_ops_tpfwupgrade);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create FW_upgrade!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_GET_TPRAWDATA, 0664,  tpd_proc_dir, &proc_ops_tprawdata_read);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create tprawdata!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_SUSPEND, 0664,  tpd_proc_dir, &proc_ops_suspend);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create suspend!\n");
}

void tpd_proc_deinit(void)
{
	if (tpd_proc_dir == NULL) {
		pr_err("%s: proc/touchscreen is NULL!\n",  __func__);
		return;
	}
	remove_proc_entry(PROC_TOUCH_INFO, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_WAKE_GESTURE, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_SMART_COVER, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_RW_REG, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_FW_UPGRADE, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_GET_TPRAWDATA, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_SUSPEND, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_DIR, NULL);
}

/**
 * tpd_classdev_register - register a new object of tpd_classdev_t class.
 * @parent: The device to register.
 * @tsp_fw_cdev: the tpd_classdev_t structure for this device.
 */
int tpd_classdev_register(struct device *parent, struct tpd_classdev_t *tsp_fw_cdev)
{
	int error = 0;

	tsp_fw_cdev->dev = device_create(tsp_fw_class, NULL, 0, tsp_fw_cdev,
					  "%s", tsp_fw_cdev->name);
	if (IS_ERR(tsp_fw_cdev->dev))
		return PTR_ERR(tsp_fw_cdev->dev);

	error = device_create_bin_file(tsp_fw_cdev->dev, &firmware_attr_reg);
	if (error) {
		dev_err(tsp_fw_cdev->dev, "%s: sysfs_create_bin_file failed\n", __func__);
	}
	mutex_init(&tsp_fw_cdev->cmd_mutex);

	/**tpd_create_wake_gesture_sysfs();*/
	create_tpd_proc_entry();

	pr_info("tpd: Registered tsp_fw device: %s\n",
			tsp_fw_cdev->name);

	return 0;
}
EXPORT_SYMBOL_GPL(tpd_classdev_register);

/**
 * tpd_classdev_unregister - unregisters a object of tsp_fw_properties class.
 * @tsp_fw_cdev: the tsp_fw device to unregister
 *
 * Unregisters a previously registered via tpd_classdev_register object.
 */
void tpd_classdev_unregister(struct tpd_classdev_t *tsp_fw_cdev)
{
	device_unregister(tsp_fw_cdev->dev);
	tpd_proc_deinit();
}
EXPORT_SYMBOL_GPL(tpd_classdev_unregister);

extern char* mtkfb_find_lcm_driver(void);
#define MAX_LCM_NAME_LEN 256
static void get_lcm_info(struct tpd_classdev_t *cdev)
{
	char * tmp_name = NULL;
	char lcm_info[16],lcm_chip_info[16];

	tmp_name = mtkfb_find_lcm_driver();
	printk("tpd check lcm name = %s", tmp_name);
	if (strnstr(tmp_name, "txd", MAX_LCM_NAME_LEN) != NULL
		|| strnstr(tmp_name, "tongxingda", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_info, "TongXingDa", 15);
	} else if (strnstr(tmp_name, "lead", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_info, "Lead", 15);
	} else if (strnstr(tmp_name, "tianma", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_info, "TianMa", 15);
	} else if (strnstr(tmp_name, "sanxing", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_info, "SanXing", 15);
	} else if (strnstr(tmp_name, "helitai", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_info, "HeLiTai", 15);
	} else if (strnstr(tmp_name, "lcetron", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_info, "Lcetron", 15);
	} else if (strnstr(tmp_name, "skyworth", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_info, "Skyworth", 15);
	} else if (strnstr(tmp_name, "holitech", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_info, "Holitech", 15);
	} else if (strnstr(tmp_name, "dijing", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_info, "DiJing", 15);
	} else if (strnstr(tmp_name, "yassy", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_info, "YaShi", 15);
	} else if (strnstr(tmp_name, "cmi", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_info, "YaShi", 15);
	} else if (strnstr(tmp_name, "easyquick", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_info, "Easyquick", 15);
	} else {
		strlcpy(lcm_info, "unknown", 15);
	}

	if (strnstr(tmp_name, "8394", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "HX8394", 15);
	} else if (strnstr(tmp_name, "9881", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "ILI9881", 15);
	} else if (strnstr(tmp_name, "8399", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "HX8399", 15);
	} else if (strnstr(tmp_name, "35532", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "NT35532", 15);
	} else if (strnstr(tmp_name, "68200", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "RM68200", 15);
	} else if (strnstr(tmp_name, "7703", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "ST7703", 15);
	} else if (strnstr(tmp_name, "hx83112a", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "HX83112A", 15);
	} else if (strnstr(tmp_name, "ft8719", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "FT8719", 15);
	} else if (strnstr(tmp_name, "otm8019a", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "OTM8019A", 15);
	} else if (strnstr(tmp_name, "st7701s", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "ST7701S", 15);
	} else if (strnstr(tmp_name, "ft8006p", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "FT8006P", 15);
	} else if (strnstr(tmp_name, "icnl9911", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "ICNL9911", 15);
	} else if (strnstr(tmp_name, "jd9161z", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "JD9161Z", 15);
	} else if (strnstr(tmp_name, "9366", MAX_LCM_NAME_LEN) != NULL) {
		strlcpy(lcm_chip_info, "JD9366", 15);
	} else {
		strlcpy(lcm_chip_info, "lcd chip", 15);
	}

	strlcpy(cdev->lcm_info, lcm_info, 64);
	strlcpy(cdev->lcm_chip_info, lcm_chip_info, 64);

}

static int __init tpd_class_init(void)
{
	tsp_fw_class = class_create(THIS_MODULE, "tsp_fw");
	if (IS_ERR(tsp_fw_class))
		return PTR_ERR(tsp_fw_class);
	tsp_fw_class->dev_groups = tsp_dev_attribute_groups;

	/* for tp probe after se init.*/
	tpd_fw_cdev.name = "touchscreen";
	tpd_fw_cdev.private = NULL;
	tpd_fw_cdev.read_block = NULL;
	tpd_fw_cdev.write_block = NULL;
	tpd_fw_cdev.get_tpinfo = NULL;
	tpd_fw_cdev.get_gesture = NULL;
	tpd_fw_cdev.wake_gesture = NULL;
	tpd_fw_cdev.get_smart_cover = NULL;
	tpd_fw_cdev.set_smart_cover = NULL;
	tpd_fw_cdev.tp_i2c_reg_read = NULL;
	tpd_fw_cdev.tp_i2c_reg_write = NULL;
	tpd_fw_cdev.tp_i2c_16bor32b_reg_read = NULL;
	tpd_fw_cdev.tp_i2c_16bor32b_reg_write = NULL;
	tpd_fw_cdev.tp_fw_upgrade = NULL;
	tpd_fw_cdev.tp_suspend_show = NULL;
	tpd_fw_cdev.set_tp_suspend = NULL;
	tpd_fw_cdev.tpd_esd_check = NULL;
	tpd_fw_cdev.b_gesture_enable = 0;
	tpd_fw_cdev.b_smart_cover_enable = 0;
	tpd_fw_cdev.TP_have_registered = false;
	tpd_fw_cdev.tp_suspend = false;
	tpd_fw_cdev.lcd_reset_processing = false;
	tpd_fw_cdev.tp_suspend_write_gesture = false;
	tpd_fw_cdev.reg_char_num = REG_CHAR_NUM_2;
	/**for tpd test*/
	tpd_fw_cdev.tpd_test_set_save_filepath = NULL;
	tpd_fw_cdev.tpd_test_get_save_filepath = NULL;
	tpd_fw_cdev.tpd_test_set_save_filename = NULL;
	tpd_fw_cdev.tpd_test_get_save_filename = NULL;
	tpd_fw_cdev.tpd_test_set_ini_filepath = NULL;
	tpd_fw_cdev.tpd_test_get_ini_filepath = NULL;
	tpd_fw_cdev.tpd_test_set_filename = NULL;
	tpd_fw_cdev.tpd_test_get_filename = NULL;
	tpd_fw_cdev.tpd_test_set_cmd = NULL;
	tpd_fw_cdev.tpd_test_get_cmd = NULL;
	tpd_fw_cdev.tpd_test_set_node_data_type = NULL;
	tpd_fw_cdev.tpd_test_get_node_data = NULL;
	tpd_fw_cdev.tpd_test_get_channel_info = NULL;
	tpd_fw_cdev.tpd_test_get_result = NULL;
	tpd_fw_cdev.tpd_test_get_tp_rawdata = NULL;
	tpd_fw_cdev.tpd_gpio_shutdown = NULL;
	tpd_fw_cdev.tpd_suspend_need_awake = NULL;

	tpd_classdev_register(NULL, &tpd_fw_cdev);

	/* lcd information */
	get_lcm_info(&tpd_fw_cdev);
	create_lcd_proc_entry();

	return 0;
}

static void __exit tpd_class_exit(void)
{
	class_destroy(tsp_fw_class);
}

subsys_initcall(tpd_class_init);
module_exit(tpd_class_exit);

MODULE_AUTHOR("John Lenz, Richard Purdie");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TSP FW Class Interface");


