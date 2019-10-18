#define LOG_TAG "Com"

#include <linux/kernel.h>
#include "cts_core.h"
#include "cts_config.h"
#include "cts_selftest.h"
#include "chipone_common.h"
#include "cts_platform.h"
#include "cts_firmware.h"

char ini_file_path[MAX_FILE_PATH_LEN] = { 0 };
char ini_file_name[MAX_FILE_NAME_LEN] = { 0 };
char save_file_path[MAX_FILE_PATH_LEN] = { 0 };
char save_test_info_file_name[MAX_FILE_NAME_LEN] = { 0 };
char save_test_result_file_name[MAX_FILE_NAME_LEN] = { 0 };
char cts_vendor_name[MAX_NAME_LEN_20] = { 0 };
char cts_firmware_name[MAX_FILE_NAME_LEN] = { 0 };
char *cts_test_failed_node_buffer = NULL;
char *cts_test_temp_buffer = NULL;
u8 *cts_test_failed_node = NULL;
int cts_test_faied_buffer_length = 0;
int cts_test_failed_count = 0;

int cts_test_result = 0;
extern void set_tp_suspend(struct chipone_ts_data *cts_data, bool enable);
struct tpvendor_t chipone_vendor_l[] = {
	{VENDOR_ID1, CTS_VENDOR1_NAME},
	{VENDOR_ID2, CTS_VENDOR2_NAME},
	{VENDOR_ID3, CTS_VENDOR3_NAME},
	{VENDOR_ID4, CTS_VENDOR4_NAME},
	{VENDOR_END, "Unknown"},
};

static int cts_get_chip_vendor(u8 vendor_id)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(chipone_vendor_l); i++) {
		if (vendor_id == 0) {
			if (strnstr(tpd_fw_cdev.lcm_info, chipone_vendor_l[i].vendor_name, 50)) {
				strlcpy(cts_vendor_name, chipone_vendor_l[i].vendor_name, sizeof(cts_vendor_name));
				snprintf(cts_firmware_name, sizeof(cts_firmware_name),
					 "chipone_firmware_%s.bin", cts_vendor_name);
				break;
			}
		} else {
			if (chipone_vendor_l[i].vendor_id == vendor_id || chipone_vendor_l[i].vendor_id == VENDOR_END) {
				strlcpy(cts_vendor_name, chipone_vendor_l[i].vendor_name, sizeof(cts_vendor_name));
				snprintf(cts_firmware_name, sizeof(cts_firmware_name),
					 "chipone_firmware_%s.bin", cts_vendor_name);
				break;
			}
		}
	}
	return 0;
}

static int tpd_init_tpinfo(struct tpd_classdev_t *cdev)
{
	u8 vendor_id1 = 0, vendor_id2 = 0, vendor_id3 = 0, access_flag = 0;
	struct cts_device *cts_dev;
	int ret = 0;
	u16 device_fw_ver = 0;

	cts_dev = (struct cts_device *)cdev->private;
	if (cts_dev->rtdata.suspended) {
		cts_err("cts tp in suspned");
		return -EIO;
	}
	ret = cts_hw_reg_readb(cts_dev, 0x3002C, &access_flag);
	if (ret) {
		cts_err("Read display access flag failed %d", ret);
		return ret;
	}

	ret = cts_hw_reg_writeb(cts_dev, CTS_CHIPID_CONCTROL_REG, access_flag | 0x01);
	if (ret) {
		cts_err("Write display access flag %02x failed %d", access_flag, ret);
		return ret;
	}
	cts_hw_reg_readb(cts_dev, CTS_VENDORID1, &vendor_id1);
	cts_hw_reg_readb(cts_dev, CTS_VENDORID2, &vendor_id2);
	cts_hw_reg_readb(cts_dev, CTS_VENDORID3, &vendor_id3);

	cts_info("Read driver id: 0x%02x, 0x%02x, 0x%02x", vendor_id1, vendor_id2, vendor_id3);

	ret = cts_hw_reg_writeb(cts_dev, CTS_CHIPID_CONCTROL_REG, access_flag);
	if (ret) {
		cts_err("Write display access flag %02x failed %d", access_flag, ret);
		return ret;
	}
	ret = cts_fw_reg_readw_retry(cts_dev, CTS_DEVICE_FW_REG_VERSION, &device_fw_ver, 5, 0);
	if (ret) {
		cts_err("Read firmware version failed %d", ret);
		device_fw_ver = 0;
	} else {
		device_fw_ver = be16_to_cpu(device_fw_ver);
		cts_info("Device firmware version: %04x", device_fw_ver);
	}
	cts_get_chip_vendor(vendor_id1);
	snprintf(cdev->ic_tpinfo.tp_name, sizeof(cdev->ic_tpinfo.tp_name), "chipone");
	strlcpy(cdev->ic_tpinfo.vendor_name, cts_vendor_name, sizeof(cdev->ic_tpinfo.vendor_name));
	cdev->ic_tpinfo.module_id = vendor_id1;
	cdev->ic_tpinfo.chip_model_id = TS_CHIP_CHIPONE;
	cdev->ic_tpinfo.chip_ver = 0;
	cdev->ic_tpinfo.firmware_ver = device_fw_ver;
	cdev->ic_tpinfo.i2c_type = 0;
	cdev->ic_tpinfo.i2c_addr = cts_dev->rtdata.i2c_addr;
	return 0;
}

#ifdef CONFIG_CTS_GESTURE
static int tpd_get_wakegesture(struct tpd_classdev_t *cdev)
{
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;

	cdev->b_gesture_enable = cts_is_gesture_wakeup_enabled(cts_dev);

	return 0;
}

static int tpd_enable_wakegesture(struct tpd_classdev_t *cdev, int enable)
{
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;
	if (cts_dev->rtdata.suspended) {
		cdev->tp_suspend_write_gesture = true;
	}

	if (enable) {
		cts_enable_gesture_wakeup(cts_dev);
	} else {
		cts_disable_gesture_wakeup(cts_dev);
	}
	return enable;
}
#endif

static bool cts_suspend_need_awake(struct tpd_classdev_t *cdev)
{
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;

#ifdef CONFIG_CTS_GESTURE
	if (!cdev->tp_suspend_write_gesture &&
		(cts_dev->rtdata.updating || cts_dev->rtdata.gesture_wakeup_enabled)) {
		cts_info("tp suspend need awake.\n");
		return true;
	}
#else
	if (cts_dev->rtdata.updating) {
		cts_info("tp suspend need awake.\n");
		return true;
	}
#endif
	else {
		cdev->tp_suspend_write_gesture = false;
		cts_info("tp suspend dont need awake.\n");
		return false;
	}
}


static int cts_i2c_reg_read(struct tpd_classdev_t *cdev, u32 addr, u8 *data, int len)
{
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;
	return cts_fw_reg_readsb(cts_dev, addr, data, len);
}

static int cts_i2c_reg_write(struct tpd_classdev_t *cdev, u32 addr, u8 *data, int len)
{
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;
	return cts_fw_reg_writesb(cts_dev, addr, data, len);
}

static int cts_tp_fw_upgrade(struct tpd_classdev_t *cdev, char *fw_name, int fwname_len)
{
	struct cts_device *cts_dev = (struct cts_device *)cdev->private;
	const struct cts_firmware *firmware;
	char fwname[MAX_FILE_NAME_LEN] = { 0 };
	bool to_flash = true;
	int ret;

	if ((fwname_len <= 1) || (fwname_len >= MAX_FILE_NAME_LEN)) {
		cts_err("fw bin name's length(%d) fail", fwname_len);
		return -EINVAL;
	}
	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, sizeof(fwname), "%s", fw_name);
	fwname[fwname_len - 1] = '\0';
	cts_info("Update firmware from file '%s'", fwname);

	firmware = cts_request_firmware_from_fs(cts_dev, fwname);
	if (firmware) {
		ret = cts_stop_device(cts_dev);
		if (ret) {
			cts_err("Stop device failed %d", ret);
			cts_release_firmware(firmware);
			return ret;
		}

		ret = cts_update_firmware(cts_dev, firmware, to_flash);
		if (ret) {
			cts_err("Update firmware failed %d", ret);
		}

		ret = cts_start_device(cts_dev);
		if (ret) {
			cts_err("Start device failed %d", ret);
			cts_release_firmware(firmware);
			return ret;
		}

		cts_release_firmware(firmware);
	} else {
		cts_err("Request firmware from file '%s' failed", fw_name);
		return -ENOENT;
	}

	return 0;
}

static int cts_tp_suspend_show(struct tpd_classdev_t *cdev)
{
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;
	cdev->tp_suspend = cts_is_device_suspended(cts_dev);
	return cdev->tp_suspend;
}

static int cts_set_tp_suspend(struct tpd_classdev_t *cdev, int enable)
{
	struct cts_device *cts_dev;
	struct chipone_ts_data *cts_data = NULL;

	cts_dev = (struct cts_device *)cdev->private;
	cts_data = container_of(cts_dev, struct chipone_ts_data, cts_dev);
	set_tp_suspend(cts_data, enable);
	return 0;
}

static int cts_test_buffer_init(struct cts_device *cts_dev)
{
	cts_info("%s:enter\n", __func__);
	cts_test_failed_node_buffer = kzalloc((TP_NODE_NUMBER * TEST_TEMP_LENGTH), GFP_KERNEL);
	cts_test_temp_buffer = kzalloc(TEST_TEMP_LENGTH, GFP_KERNEL);
	cts_test_failed_node = kzalloc(TP_NODE_NUMBER, GFP_KERNEL);
	if (cts_test_failed_node_buffer == NULL || cts_test_temp_buffer == NULL ||
		cts_test_failed_node == NULL) {
		kfree(cts_test_failed_node_buffer);
		kfree(cts_test_temp_buffer);
		kfree(cts_test_failed_node);
		cts_err("%s:alloc memory failde!\n", __func__);
		return -ENOMEM;
	}
	cts_test_faied_buffer_length = 0;
	cts_test_failed_count = 0;
	cts_test_result = 0;
	return 0;
}

static void cts_test_buffer_free(void)
{
	cts_info("%s:enter\n", __func__);
	kfree(cts_test_failed_node_buffer);
	kfree(cts_test_temp_buffer);
	kfree(cts_test_failed_node);
}

void cts_clean_test_buffer(struct cts_device *cts_dev)
{
	memset(cts_test_failed_node_buffer, 0, (TP_NODE_NUMBER * TEST_TEMP_LENGTH));
	memset(cts_test_failed_node, 0, TP_NODE_NUMBER);
	cts_test_faied_buffer_length = 0;
	cts_test_failed_count = 0;
}

static int cts_save_failed_node_to_buffer(struct cts_device *cts_dev, char *tmp_buffer, int length)
{

	if (cts_test_failed_node_buffer == NULL) {
		cts_err("warning:cts_test_failed_node_buffer is null.");
		return -EPERM;
	}

	cts_test_faied_buffer_length +=
		snprintf(cts_test_failed_node_buffer + cts_test_faied_buffer_length,
		 ((TP_NODE_NUMBER * TEST_TEMP_LENGTH) - cts_test_faied_buffer_length), tmp_buffer);
	cts_test_failed_count++;

	return 0;
}

int cts_save_failed_node(struct cts_device *cts_dev, int failed_node)
{
	int i_len = 0;
	int tx = 0;
	int rx = 0;

	tx = failed_node / cts_dev->fwdata.rows;
	rx = failed_node % cts_dev->fwdata.rows;
	if (cts_test_failed_node == NULL)
		return -EPERM;
	if (cts_test_failed_node[failed_node] == 0) {
		if (cts_test_temp_buffer != NULL) {
			i_len = snprintf(cts_test_temp_buffer, TEST_TEMP_LENGTH, ",%d,%d", tx, rx);
			cts_save_failed_node_to_buffer(cts_dev, cts_test_temp_buffer, i_len);
			cts_test_failed_node[failed_node] = 1;
			return 0;
		} else {
			return -EPERM;
		}
	} else {
		return 0;
	}
}

static int tpd_test_save_file_path_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(save_file_path, 0, sizeof(save_file_path));
	snprintf(save_file_path, sizeof(save_file_path), "%s", buf);
	cts_info("set save_file_path:%s", save_file_path);
	return 0;
}

static int tpd_test_save_file_path_show(struct tpd_classdev_t *cdev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", save_file_path);
}

static int tpd_test_save_file_name_store(struct tpd_classdev_t *cdev, const char *buf)
{

	memset(save_test_result_file_name, 0, sizeof(save_test_result_file_name));
	snprintf(save_test_result_file_name, sizeof(save_test_result_file_name), "%s.txt", buf);
	memset(save_test_info_file_name, 0, sizeof(save_test_info_file_name));
	snprintf(save_test_info_file_name, sizeof(save_test_info_file_name), "%s.csv", buf);
	cts_info("set save_test_result_file_name: %s", save_test_result_file_name);
	cts_info("set save_test_info_file_name: %s", save_test_info_file_name);
	return 0;
}

static int tpd_test_save_file_name_show(struct tpd_classdev_t *cdev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", save_test_result_file_name);
}

static int tpd_test_cmd_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	int i_len = 0;
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;
	cts_info("%s:enter\n", __func__);
	i_len = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d", cts_test_result, cts_dev->fwdata.cols,
		cts_dev->fwdata.rows, cts_test_failed_count);
	cts_info("tpd test resutl:%d && rawdata node failed count:%d.\n", cts_test_result, cts_test_failed_count);

	if (cts_test_failed_node_buffer != NULL) {
		i_len += snprintf(buf + i_len, PAGE_SIZE - i_len, cts_test_failed_node_buffer);
	}
	cts_info("tpd  test:%s.\n", buf);
	num_read_chars = i_len;
	return num_read_chars;
}

static int tpd_test_cmd_store(struct tpd_classdev_t *cdev, const char *buf)
{
	unsigned long command = 0;
	int ret;
	struct cts_device *cts_dev;

	if (cdev->tp_suspend) {
		cts_info("In suspend, no test, return now");
		return -EINVAL;
	}
	cts_dev = (struct cts_device *)cdev->private;
	ret = kstrtoul(buf, 10, &command);
	if (ret) {
		cts_err("invalid param:%s", buf);
		return -EIO;
	}

	if (command == TP_TEST_INIT) {
		ret = cts_init_selftest(cts_dev);
		if (ret) {
			cts_err("cts init selftest err");
			return ret;
		}
		ret = cts_test_buffer_init(cts_dev);
		if (ret) {
			cts_err("cts test buffer init err");
			cts_deinit_selftest(cts_dev);
			return ret;
		}
	} else if (command == TP_TEST_START) {
		msleep(200);
		ret = cts_start_selftest(cts_dev);
		if (ret & (1 << FIRMWARE_VERSION_TEST_CODE))
			cts_test_result = cts_test_result | TEST_VERSION_ERR;
		if (ret & (1 << RAWDATA_TEST_CODE))
			cts_test_result = cts_test_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		if (ret & (1 << OPEN_CIRCUITE_TEST_CODE))
			cts_test_result = cts_test_result | TEST_GT_OPEN;
		if (ret & (1 << SHORT_CIRCUITE_TEST_CODE))
			cts_test_result = cts_test_result | TEST_GT_SHORT;

	} else if (command == TP_TEST_END) {
		cts_deinit_selftest(cts_dev);
		cts_test_buffer_free();
	}
	return 0;
}

int cts_register_fw_class(struct cts_device *cts_dev)
{
	tpd_fw_cdev.private = (void *)cts_dev;
	tpd_fw_cdev.get_tpinfo = tpd_init_tpinfo;
#ifdef CONFIG_CTS_GESTURE
	tpd_fw_cdev.get_gesture = tpd_get_wakegesture;
	tpd_fw_cdev.wake_gesture = tpd_enable_wakegesture;
#endif
	tpd_fw_cdev.reg_char_num = REG_CHAR_NUM_4;
	tpd_fw_cdev.tp_i2c_16bor32b_reg_read = cts_i2c_reg_read;
	tpd_fw_cdev.tp_i2c_16bor32b_reg_write = cts_i2c_reg_write;
	tpd_fw_cdev.tp_fw_upgrade = cts_tp_fw_upgrade;
	tpd_fw_cdev.tp_suspend_show = cts_tp_suspend_show;
	tpd_fw_cdev.set_tp_suspend = cts_set_tp_suspend;
	tpd_fw_cdev.tpd_suspend_need_awake = cts_suspend_need_awake;

	tpd_fw_cdev.tpd_test_set_save_filepath = tpd_test_save_file_path_store;
	tpd_fw_cdev.tpd_test_get_save_filepath = tpd_test_save_file_path_show;
	tpd_fw_cdev.tpd_test_set_save_filename = tpd_test_save_file_name_store;
	tpd_fw_cdev.tpd_test_get_save_filename = tpd_test_save_file_name_show;
	tpd_fw_cdev.tpd_test_set_cmd = tpd_test_cmd_store;
	tpd_fw_cdev.tpd_test_get_cmd = tpd_test_cmd_show;
	tpd_init_tpinfo(&tpd_fw_cdev);

	strlcpy(ini_file_path, DEFAULT_INI_FILE_PATH, sizeof(ini_file_path));
	strlcpy(save_file_path, DEFAULT_SAVE_FILE_PATH, sizeof(ini_file_path));
	strlcpy(save_test_result_file_name, DEFAULT_RESULT_FILE_NAME, sizeof(save_test_result_file_name));
	strlcpy(save_test_info_file_name, DEFAULT_INFO_FILE_NAME, sizeof(save_test_info_file_name));
	snprintf(ini_file_name, sizeof(ini_file_name), "cts_test_sensor_%s.cfg", cts_vendor_name);

	return 0;
}
