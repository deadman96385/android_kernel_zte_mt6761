/************************************************************************
*
* File Name: fts_common_interface.c
*
*  *   Version: v1.0
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/

#include "focaltech_core.h"
#include "focaltech_test.h"
#include <linux/kernel.h>
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define TEST_RESULT_LENGTH (8 * 1200)
#define TEST_TEMP_LENGTH 8
#define TEST_PASS	0
#define TEST_BEYOND_MAX_LIMIT		0x0001
#define TEST_BEYOND_MIN_LIMIT		0x0002
#define TP_TEST_INIT		1
#define TP_TEST_START	2
#define TP_TEST_END		3

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
char g_fts_save_file_path[MAX_SAVE_PATH_NAME_LEN] = {0};
char g_fts_save_file_name[MAX_SAVE_FILE_NAME_LEN] = {0};
char g_fts_ini_filename[MAX_INI_FILE_NAME_LEN] = {0};
char *fts_test_failed_node_buffer = NULL;
char *fts_test_temp_buffer = NULL;
u8 *fts_test_failed_node = NULL;
int fts_test_faied_buffer_length = 0;
int fts_test_failed_count = 0;
int fts_tptest_result = 0;

extern struct fts_ts_data *fts_data;
extern struct fts_test *fts_ftest;
extern int fts_ts_suspend(struct device *dev);
extern int fts_test_init_basicinfo(struct fts_test *tdata);
extern int fts_test_entry(char *ini_file_name);
#if FTS_GESTURE_EN
extern int tpd_get_gesture_mode(void);
extern int tpd_set_gesture_mode(int gesture_mode);
#endif

struct tpvendor_t fts_vendor_info[] = {
	{FTS_MODULE_ID, FTS_MODULE_LCD_NAME },
	{FTS_MODULE2_ID, FTS_MODULE2_LCD_NAME },
	{FTS_MODULE3_ID, FTS_MODULE3_LCD_NAME },
	{VENDOR_END, "Unknown"},
};

int get_fts_module_info_from_lcd(void)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(fts_vendor_info); i++) {
		if (strnstr(tpd_fw_cdev.lcm_info, fts_vendor_info[i].vendor_name,
			strlen(tpd_fw_cdev.lcm_info))) {
			return  fts_vendor_info[i].vendor_id;
		}
	}
	return -EINVAL;
}

static int tpd_init_tpinfo(struct tpd_classdev_t *cdev)
{
	u8 fwver_in_chip = 0;
	u8 vendorid_in_chip = 0;
	u8 chipid_in_chip = 0;
	u8 lcdver_in_chip = 0;
	u8 retry = 0;

	if (fts_data->suspended) {
		FTS_ERROR("fts tp in suspned");
		return -EIO;
	}

	while (retry++ < 5) {
		fts_read_reg(FTS_REG_CHIP_ID, &chipid_in_chip);
		fts_read_reg(FTS_REG_VENDOR_ID, &vendorid_in_chip);
		fts_read_reg(FTS_REG_FW_VER, &fwver_in_chip);
		fts_read_reg(FTS_REG_LIC_VER, &lcdver_in_chip);
		if ((chipid_in_chip != 0) && (vendorid_in_chip != 0) && (fwver_in_chip != 0)
			&& (lcdver_in_chip != 0)) {
			FTS_DEBUG("chip_id = %x,vendor_id =%x,fw_version=%x,lcd_version=%x .\n",
				  chipid_in_chip, vendorid_in_chip, fwver_in_chip, lcdver_in_chip);
			break;
		}
		FTS_DEBUG("chip_id = %x,vendor_id =%x,fw_version=%x, lcd_version=%x .\n",
			  chipid_in_chip, vendorid_in_chip, fwver_in_chip, lcdver_in_chip);
		msleep(20);
	}

	snprintf(cdev->ic_tpinfo.tp_name, sizeof(cdev->ic_tpinfo.tp_name), "Focal");
	cdev->ic_tpinfo.chip_model_id = TS_CHIP_FOCAL;

	cdev->ic_tpinfo.chip_part_id = chipid_in_chip;
	cdev->ic_tpinfo.module_id = vendorid_in_chip;
	cdev->ic_tpinfo.chip_ver = 0;
	cdev->ic_tpinfo.firmware_ver = fwver_in_chip;
	cdev->ic_tpinfo.display_ver = lcdver_in_chip;
	cdev->ic_tpinfo.i2c_type = 0;
	cdev->ic_tpinfo.i2c_addr = 0x38;

	return 0;
}

#if FTS_GESTURE_EN
static int tpd_get_wakegesture(struct tpd_classdev_t *cdev)
{
	cdev->b_gesture_enable = tpd_get_gesture_mode();
	return 0;
}

static int tpd_enable_wakegesture(struct tpd_classdev_t *cdev, int enable)
{
	if (fts_data->suspended) {
		cdev->tp_suspend_write_gesture = true;
	}
	tpd_set_gesture_mode(enable);
	return enable;
}
#endif

static bool fts_suspend_need_awake(struct tpd_classdev_t *cdev)
{
	struct fts_ts_data *ts_data = (struct fts_ts_data *)cdev->private;

#ifdef FTS_ESDCHECK_REGISTER
	if (ts_data->esd_check_error) {
		FTS_INFO("tp esd check error need power off.");
		return false;
	}
#endif
#if FTS_GESTURE_EN
	if (!cdev->tp_suspend_write_gesture &&
		(ts_data->fw_loading || tpd_get_gesture_mode())) {
		FTS_INFO("tp suspend need awake.\n");
		return true;
	}
#else
	if (ts_data->fw_loading) {
		FTS_INFO("tp suspend need awake.\n");
		return true;
	}
#endif
	else {
		cdev->tp_suspend_write_gesture = false;
		FTS_INFO("tp suspend dont need awake.\n");
		return false;
	}
}

#ifdef FTS_ESDCHECK_REGISTER
static bool fts_get_esd_flow_cnt(struct fts_ts_data *ts_data)
{
	int ret = 0;
	u8 reg_value = 0;
	u8 reg_addr = 0;

	reg_addr = FTS_REG_FLOW_WORK_CNT;
	ret = fts_read_reg(reg_addr, &reg_value);
	if (ret < 0) {
		FTS_ERROR("[ESD]: Read Reg 0x91 failed ret = %d!!", ret);
		ts_data->esd_check_noack++;
	} else {
		if (reg_value == ts_data->esd_flow_last_val) {
			ts_data->esd_check_flow_cnt++;
		} else {
			ts_data->esd_check_flow_cnt = 0;
		}
		ts_data->esd_flow_last_val = reg_value;
		ts_data->esd_check_noack = 0;
	}
	if (ts_data->esd_check_flow_cnt >= 5) {
		FTS_INFO("[ESD]: Flow Work Cnt(reg0x91) keep a value for 5 times, need execute TP reset!!");
		return true;
	}
	return false;
}

static bool fts_esd_check_register(struct fts_ts_data *ts_data)
{
	int ret = 0;
	u8 reg_value = 0;
	u8 reg_addr = 0;

	reg_addr = FTS_ESDCHECK_REGISTER;
	ret = fts_read_reg(reg_addr, &reg_value);
	if (ret < 0) {
		FTS_ERROR("[ESD]: Read Reg: %d failed ret = %d!!", FTS_ESDCHECK_REGISTER, ret);
		ts_data->esd_check_noack++;
	} else {
		ts_data->esd_check_noack = 0;
		if  (reg_value == 1) {
			fts_write_reg(FTS_REG_SW_RESET, 0x01);
			FTS_ERROR("tp esd check error");
			return true;
		}
	}
	return false;
}

static bool fts_esd_check(struct tpd_classdev_t *cdev)
{
	struct fts_ts_data *ts_data = (struct fts_ts_data *)cdev->private;

	if (ts_data->esd_register_check_enable == false)
		return 0;
	FTS_INFO("%s", __func__);
	ts_data->esd_check_error = fts_esd_check_register(ts_data) || fts_get_esd_flow_cnt(ts_data);
	if (ts_data->esd_check_error) {
		goto exit;
	} else if (ts_data->esd_check_noack > 5) {
		ts_data->esd_check_noack = 0;
		ts_data->esd_check_error = true;
		goto exit;
	}
	ts_data->esd_check_error = false;
exit:
	return ts_data->esd_check_error;
}
#endif

static int fts_i2c_reg_read(struct tpd_classdev_t *cdev, char addr, u8 *data, int len)
{
	return fts_read(&addr, 1, data, len);
}
static int fts_i2c_reg_write(struct tpd_classdev_t *cdev, char addr, u8 *data, int len)
{
	int i;

	if (len == 1) {
		goto WRITE_ONE_REG;
	} else {
		char *tmpbuf = NULL;

		tmpbuf = kzalloc(len + 1, GFP_KERNEL);
		if (!tmpbuf) {
			pr_info("allocate memory failed!\n");
			return -ENOMEM;
		}
		tmpbuf[0] = addr & 0xFF;
		for (i = 1; i <= len; i++) {
			tmpbuf[i] = data[i - 1];
		}
		fts_write(tmpbuf, len);
		kfree(tmpbuf);
		return 0;
	}
WRITE_ONE_REG:
	return fts_write_reg(addr, *data);
}

static int fts_tp_fw_upgrade(struct tpd_classdev_t *cdev, char *fw_name, int fwname_len)
{
	char fwname[FILE_NAME_LENGTH];
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev = ts_data->input_dev;

	if ((fwname_len <= 1) || (fwname_len >= FILE_NAME_LENGTH)) {
		FTS_ERROR("fw bin name's length(%d) fail", fwname_len);
		return -EINVAL;
	}
	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, sizeof(fwname), "%s", fw_name);
	fwname[fwname_len - 1] = '\0';
	FTS_INFO("fwname is %s", fwname);

	FTS_INFO("upgrade with bin file through sysfs node");
	mutex_lock(&input_dev->mutex);
	fts_upgrade_bin(fwname, 0);
	mutex_unlock(&input_dev->mutex);

	return 0;
}

static int fts_tp_suspend_show(struct tpd_classdev_t *cdev)
{
	cdev->tp_suspend = fts_data->suspended;
	return cdev->tp_suspend;
}

static int fts_set_tp_suspend(struct tpd_classdev_t *cdev, int enable)
{
	int ret = 0;

	if (enable) {
		if (!fts_data->fb_tpd_suspend_flag) {
			FTS_DEBUG("lcd off notifier.\n");
			ret = cancel_work_sync(&fts_data->resume_work);
			if (!ret)
				FTS_DEBUG("cancel fb_tpd_resume_wq err = %d\n", ret);
			fts_ts_suspend(fts_data->dev);
			fts_data->fb_tpd_suspend_flag = 1;
		}
	} else {
		if (fts_data->fb_tpd_suspend_flag) {
			ret = queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
			if (!ret) {
				FTS_ERROR("start tpd_resume_wq failed\n");
				return ret;
			}
		}
	}
	cdev->tp_suspend = fts_data->suspended;
	return cdev->tp_suspend;
}

static int fts_test_buffer_init(void)
{
	struct fts_test *tdata = fts_ftest;

	FTS_INFO("%s:enter\n", __func__);
	fts_test_failed_node_buffer = kzalloc(TEST_RESULT_LENGTH, GFP_KERNEL);
	fts_test_temp_buffer = kzalloc(TEST_TEMP_LENGTH, GFP_KERNEL);
	fts_test_failed_node = kzalloc(((tdata->node.tx_num + 1) * tdata->node.rx_num), GFP_KERNEL);
	if (fts_test_failed_node_buffer == NULL || fts_test_temp_buffer == NULL ||
		fts_test_failed_node == NULL) {
		if (fts_test_failed_node_buffer != NULL)
			kfree(fts_test_failed_node_buffer);
		if (fts_test_temp_buffer != NULL)
			kfree(fts_test_temp_buffer);
		if (fts_test_failed_node != NULL)
			kfree(fts_test_failed_node);
		FTS_ERROR("%s:alloc memory failde!\n", __func__);
		return -ENOMEM;
	}
	fts_test_faied_buffer_length = 0;
	fts_test_failed_count = 0;
	fts_tptest_result = 0;
	return 0;
}

static void fts_test_buffer_free(void)
{
	FTS_INFO("%s:enter\n", __func__);
	if (fts_test_failed_node_buffer != NULL)
		kfree(fts_test_failed_node_buffer);
	if (fts_test_temp_buffer != NULL)
		kfree(fts_test_temp_buffer);
	if (fts_test_failed_node != NULL)
		kfree(fts_test_failed_node);
}

static int fts_save_failed_node_to_buffer(char *tmp_buffer, int length)
{

	if (fts_test_failed_node_buffer == NULL) {
		FTS_ERROR("warning:fts_test_failed_node_buffer is null.");
		return -EPERM;
	}

	snprintf(fts_test_failed_node_buffer + fts_test_faied_buffer_length,
		 (TEST_RESULT_LENGTH - fts_test_faied_buffer_length), tmp_buffer);
	fts_test_faied_buffer_length += length;
	fts_test_failed_count++;

	return 0;
}

int fts_save_failed_node(int failed_node)
{
	int i_len = 0;
	int tx = 0;
	int rx = 0;
	struct fts_test *tdata = fts_ftest;

	tx = failed_node / tdata->node.rx_num;
	rx = failed_node % tdata->node.rx_num;
	if (fts_test_failed_node == NULL)
		return -EPERM;
	if (fts_test_failed_node[failed_node] == 0) {
		if (fts_test_temp_buffer != NULL) {
			i_len = snprintf(fts_test_temp_buffer, TEST_TEMP_LENGTH, ",%d,%d", tx, rx);
			fts_save_failed_node_to_buffer(fts_test_temp_buffer, i_len);
			fts_test_failed_node[failed_node] = 1;
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
	memset(g_fts_save_file_path, 0, sizeof(g_fts_save_file_path));
	snprintf(g_fts_save_file_path, sizeof(g_fts_save_file_path), "%s", buf);

	FTS_TEST_DBG("save file path:%s.", g_fts_save_file_path);

	return 0;
}

static int tpd_test_save_file_path_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", g_fts_save_file_path);

	return num_read_chars;
}

static int tpd_test_save_file_name_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(g_fts_save_file_name, 0, sizeof(g_fts_save_file_name));
	snprintf(g_fts_save_file_name, sizeof(g_fts_save_file_name), "%s", buf);

	FTS_TEST_DBG("save file name:%s.", g_fts_save_file_name);

	return 0;
}

static int tpd_test_save_file_name_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", g_fts_save_file_name);

	return num_read_chars;
}


static int tpd_test_cmd_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	int i_len = 0;
	struct fts_test *tdata = fts_ftest;

	FTS_INFO("%s:enter\n", __func__);
	i_len = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d", fts_tptest_result, tdata->node.tx_num,
			tdata->node.rx_num, fts_test_failed_count);
	FTS_INFO("tpd test resutl:%d && rawdata node failed count:%d.\n", fts_tptest_result, fts_test_failed_count);

	if (fts_test_failed_node_buffer != NULL) {
		i_len += snprintf(buf + i_len, PAGE_SIZE - i_len, fts_test_failed_node_buffer);
	}
	FTS_ERROR("tpd  test:%s.\n", buf);
	num_read_chars = i_len;
	return num_read_chars;
}

static int tpd_test_cmd_store(struct tpd_classdev_t *cdev, const char *buf)
{
	unsigned long command = 0;
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct fts_test *tdata = fts_ftest;
	struct input_dev *input_dev;

	if (ts_data->suspended) {
		FTS_INFO("In suspend, no test, return now");
		return -EINVAL;
	}
	input_dev = ts_data->input_dev;

	FTS_INFO("%s:enter\n", __func__);
	ret = kstrtoul(buf, 10, &command);
	if (ret) {
		FTS_ERROR("invalid param:%s", buf);
		return -EIO;
	}
	if (command == TP_TEST_INIT) {
		/* get basic information: tx/rx num ... */
		ret = fts_test_init_basicinfo(tdata);
		if (ret < 0) {
			FTS_TEST_ERROR("test init basicinfo fail");
			return ret;
		}
		ret = fts_test_buffer_init();
		if (ret < 0) {
			FTS_ERROR("%s:alloc memory failde!\n", __func__);
			return -ENOMEM;
		}

	} else if (command == TP_TEST_START) {
		mutex_lock(&input_dev->mutex);
		disable_irq(ts_data->irq);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
		fts_esdcheck_switch(DISABLE);
#endif
#ifdef FTS_ESDCHECK_REGISTER
		ts_data->esd_register_check_enable = false;
#endif
		ret = fts_enter_test_environment(1);
		if (ret < 0) {
			FTS_ERROR("enter test environment fail");
		} else {
			fts_test_entry(g_fts_ini_filename);
		}
		ret = fts_enter_test_environment(0);
		if (ret < 0) {
			FTS_ERROR("enter normal environment fail");
		}
#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
		fts_esdcheck_switch(ENABLE);
#endif
#ifdef FTS_ESDCHECK_REGISTER
		ts_data->esd_register_check_enable = true;
#endif
		enable_irq(ts_data->irq);
		mutex_unlock(&input_dev->mutex);

	} else if (command == TP_TEST_END) {
		fts_test_buffer_free();
	} else {
		FTS_ERROR("invalid command %ld", command);
	}
	return 0;
}

int tpd_register_fw_class(struct fts_ts_data *data)
{
	tpd_fw_cdev.private = (void *)data;
	tpd_fw_cdev.get_tpinfo = tpd_init_tpinfo;
	#if FTS_GESTURE_EN
	tpd_fw_cdev.get_gesture = tpd_get_wakegesture;
	tpd_fw_cdev.wake_gesture = tpd_enable_wakegesture;
	#endif
	tpd_fw_cdev.tp_i2c_reg_read = fts_i2c_reg_read;
	tpd_fw_cdev.tp_i2c_reg_write = fts_i2c_reg_write;
	tpd_fw_cdev.tp_fw_upgrade = fts_tp_fw_upgrade;
	tpd_fw_cdev.tp_suspend_show = fts_tp_suspend_show;
	tpd_fw_cdev.set_tp_suspend = fts_set_tp_suspend;
	tpd_fw_cdev.tpd_suspend_need_awake = fts_suspend_need_awake;
#ifdef FTS_ESDCHECK_REGISTER
	tpd_fw_cdev.tpd_esd_check = fts_esd_check;
	data->esd_register_check_enable = true;
	data->esd_check_error = false;
	data->esd_check_flow_cnt = 0;
	data->esd_check_noack = 0;
	data->esd_flow_last_val = 0;
#endif
	tpd_fw_cdev.tpd_test_set_save_filepath = tpd_test_save_file_path_store;
	tpd_fw_cdev.tpd_test_get_save_filepath = tpd_test_save_file_path_show;
	tpd_fw_cdev.tpd_test_set_save_filename = tpd_test_save_file_name_store;
	tpd_fw_cdev.tpd_test_get_save_filename = tpd_test_save_file_name_show;
	tpd_fw_cdev.tpd_test_set_cmd = tpd_test_cmd_store;
	tpd_fw_cdev.tpd_test_get_cmd = tpd_test_cmd_show;
	tpd_init_tpinfo(&tpd_fw_cdev);

	snprintf(g_fts_save_file_path, sizeof(g_fts_save_file_path), "%s", "/sdcard/");
	snprintf(g_fts_save_file_name, sizeof(g_fts_save_file_name), "%s", "fts_test_result");
	snprintf(g_fts_ini_filename, sizeof(g_fts_ini_filename), "fts_test_sensor_%02x.ini",
		tpd_fw_cdev.ic_tpinfo.module_id);

	return 0;
}

