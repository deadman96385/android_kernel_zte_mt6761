#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/ctype.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/rtc.h>


#include "synaptics_dsx_core.h"

#include"synaptics_dsx_test_entry.h"

#ifdef CONFIG_TOUCHSCREEN_SPECIAL_INTERFACE


/* Global variable or extern global variabls/functions */
struct synaptics_test_st gSt_synaptics_test;
char g_syna_save_file_name[50] = {0};

void set_max_channel_num(struct synaptics_test_st *stp_test)
{
	stp_test->screen_cfg.iUsedMaxTxNum = 35;
	stp_test->screen_cfg.iUsedMaxRxNum = 35;
}

int malloc_synaptics_test_struct(struct synaptics_test_st *stp_test)
{
	TPD_TEST_DBG("");
	set_max_channel_num(stp_test);

	if (stp_test->detail_threshold.invalid_node == NULL) {
		stp_test->detail_threshold.invalid_node =
			(unsigned char (*)[RX_NUM_MAX])tpd_malloc(NUM_MAX*sizeof(unsigned char));
		if (stp_test->detail_threshold.invalid_node == NULL)
			goto ERR;
	}
	if (stp_test->detail_threshold.rawdata_test_min == NULL) {
		stp_test->detail_threshold.rawdata_test_min =
			(int (*)[RX_NUM_MAX])tpd_malloc(NUM_MAX*sizeof(int));
		if (stp_test->detail_threshold.rawdata_test_min == NULL)
			goto ERR;
	}
	if (stp_test->detail_threshold.rawdata_test_max == NULL) {
		stp_test->detail_threshold.rawdata_test_max =
			(int (*)[RX_NUM_MAX])tpd_malloc(NUM_MAX*sizeof(int));
		if (stp_test->detail_threshold.rawdata_test_max == NULL)
			goto ERR;
	}

	return 0;

ERR:
	TPD_TEST_DBG("tpd_malloc memory failed in function.");
	return -EINVAL;
}

void free_synaptics_test_struct(struct synaptics_test_st *stp_test)
{
	TPD_TEST_DBG("");

	if (stp_test->detail_threshold.invalid_node != NULL) {
		tpd_free(stp_test->detail_threshold.invalid_node);
		stp_test->detail_threshold.invalid_node = NULL;
	}
	if (stp_test->detail_threshold.rawdata_test_min != NULL) {
		tpd_free(stp_test->detail_threshold.rawdata_test_min);
		stp_test->detail_threshold.rawdata_test_min = NULL;
	}
	if (stp_test->detail_threshold.rawdata_test_max != NULL) {
		tpd_free(stp_test->detail_threshold.rawdata_test_max);
		stp_test->detail_threshold.rawdata_test_max = NULL;
	}
	if (stp_test->ini_string != NULL) {
		tpd_free(stp_test->ini_string);
		stp_test->ini_string = NULL;
	}
	if (stp_test->result_buffer != NULL) {
		tpd_free(stp_test->result_buffer);
		stp_test->result_buffer = NULL;
		stp_test->result_length = 0;
	}
	if (stp_test->special_buffer != NULL) {
		tpd_free(stp_test->special_buffer);
		stp_test->special_buffer = NULL;
		stp_test->special_buffer_length = 0;
		stp_test->rawdata_failed_count = 0;
	}
	if (stp_test->temp_buffer != NULL) {
		tpd_free(stp_test->temp_buffer);
		stp_test->temp_buffer = NULL;
	}

	release_key_data_space();
}

void init_tddi_detail_threshold_rawdata(struct synaptics_test_st *stp_test)
{
	char str[MAX_PATH], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int	dividerPos = 0;
	char str_tmp[MAX_PATH];
	int index = 0;
	int  k = 0, i = 0, j = 0;

	TPD_TEST_DBG("");

	/* RawData Test */
	MaxValue = stp_test->basic_threshold.max_limit_value;

	for (i = 0; i < stp_test->screen_cfg.iUsedMaxTxNum; i++)
		for (j = 0; j < stp_test->screen_cfg.iUsedMaxRxNum; j++)
			stp_test->detail_threshold.rawdata_test_max[i][j] = MaxValue;

	for (i = 0; i < stp_test->screen_cfg.iUsedMaxTxNum; i++) {
		snprintf(str, sizeof(str), "RawData_Max_Tx%d", (i + 1));
		dividerPos = get_key_value_string("SpecialSet", str, "111", strTemp);
		snprintf(strValue, sizeof(strValue), "%s", strTemp);
		if (dividerPos == 0)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (strValue[j] == ',') {
				stp_test->detail_threshold.rawdata_test_max[i][k] = (short)(tpd_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (strValue[j] == ' ')
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}

	}

	MinValue = stp_test->basic_threshold.min_limit_value;

	for (i = 0; i < stp_test->screen_cfg.iUsedMaxTxNum; i++)
		for (j = 0; j < stp_test->screen_cfg.iUsedMaxRxNum; j++)
			stp_test->detail_threshold.rawdata_test_min[i][j] = MinValue;

	for (i = 0; i < stp_test->screen_cfg.iUsedMaxTxNum; i++) {
		snprintf(str, sizeof(str), "RawData_Min_Tx%d", (i + 1));
		dividerPos = get_key_value_string("SpecialSet", str, "NULL", strTemp);
		snprintf(strValue, sizeof(strValue), "%s", strTemp);
		if (dividerPos == 0)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (strValue[j] == ',') {
				stp_test->detail_threshold.rawdata_test_min[i][k] = (short)(tpd_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (strValue[j] == ' ')
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}
}

void init_tddi_invalid_node(struct synaptics_test_st *stp_test)
{

	char str[MAX_PATH] = {0}, strTemp[MAX_PATH] = {0};
	int i = 0, j = 0;

	TPD_TEST_DBG("");

	for (i = 0; i < stp_test->screen_cfg.iUsedMaxTxNum; i++) {
		for (j = 0; j < stp_test->screen_cfg.iUsedMaxRxNum; j++) {
			snprintf(strTemp, sizeof(strTemp), "invalid_node[%d][%d]", (i + 1), (j + 1));

			get_key_value_string("INVALID_NODE", strTemp, "1", str);
			if (tpd_atoi(str) == 0)
				stp_test->detail_threshold.invalid_node[i][j] = 0;
			else if (tpd_atoi(str) == 2)
				stp_test->detail_threshold.invalid_node[i][j] = 2;
			else
				stp_test->detail_threshold.invalid_node[i][j] = 1;
		}
	}
}


void init_tddi_detail_threshold(struct synaptics_test_st *stp_test)
{
	TPD_TEST_DBG("");

	set_max_channel_num(stp_test);/* set used TxRx */

	init_tddi_invalid_node(stp_test);
	init_tddi_detail_threshold_rawdata(stp_test);
}

void init_tddi_test_basic_threshold(struct synaptics_test_st *stp_test)
{
	char str[BUFFER_LENGTH];

	TPD_TEST_DBG("");

	/* FW Version */
	get_key_value_string("Basic_Threshold", "firmware_version", "0", str);
	stp_test->basic_threshold.firmware_version = tpd_atoi(str);

	/* IC Version */
	get_key_value_string("Basic_Threshold", "ic_version", "3", str);
	stp_test->basic_threshold.ic_version = tpd_atoi(str);

	/* raw data  */
	get_key_value_string("Basic_Threshold", "max_limit_value", "3600", str);
	stp_test->basic_threshold.max_limit_value = tpd_atoi(str);
	get_key_value_string("Basic_Threshold", "min_limit_value", "600", str);
	stp_test->basic_threshold.min_limit_value = tpd_atoi(str);
	get_key_value_string("Basic_Threshold", "rawdata_test_num_of_frames", "5", str);
	stp_test->basic_threshold.rawdata_test_num_of_frames = tpd_atoi(str);
	get_key_value_string("Basic_Threshold", "rawdata_test_interval_of_frames", "0", str);
	stp_test->basic_threshold.rawdata_test_interval_of_frames = tpd_atoi(str);

	/* touch key max min */
	get_key_value_string("Basic_Threshold", "max_key_limit_value", "3600", str);
	stp_test->basic_threshold.max_key_limit_value = tpd_atoi(str);
	get_key_value_string("Basic_Threshold", "min_key_limit_value", "600", str);
	stp_test->basic_threshold.min_key_limit_value = tpd_atoi(str);

	/* noise test */
	get_key_value_string("Basic_Threshold", "nosie_test_limit", "40", str);
	stp_test->basic_threshold.nosie_test_limit = tpd_atoi(str);
	get_key_value_string("Basic_Threshold", "nosie_test_num_of_frames", "50", str);
	stp_test->basic_threshold.nosie_test_num_of_frames = tpd_atoi(str);

	/* short test */
	get_key_value_string("Basic_Threshold", "ee_short_test_limit_part1", "547", str);
	stp_test->basic_threshold.ee_short_test_limit_part1 = tpd_atoi(str);
	get_key_value_string("Basic_Threshold", "ee_short_test_limit_part2", "59", str);
	stp_test->basic_threshold.ee_short_test_limit_part2 = tpd_atoi(str);

	/* open test */
	get_key_value_string("Basic_Threshold", "amp_open_int_dur_one", "145", str);
	stp_test->basic_threshold.amp_open_int_dur_one = tpd_atoi(str);
	get_key_value_string("Basic_Threshold", "amp_open_int_dur_two", "10", str);
	stp_test->basic_threshold.amp_open_int_dur_two = tpd_atoi(str);
	get_key_value_string("Basic_Threshold", "amp_open_test_limit_phase1_lower", "2100", str);
	stp_test->basic_threshold.amp_open_test_limit_phase1_lower = tpd_atoi(str);
	get_key_value_string("Basic_Threshold", "amp_open_test_limit_phase1_upper", "10000", str);
	stp_test->basic_threshold.amp_open_test_limit_phase1_upper = tpd_atoi(str);
	get_key_value_string("Basic_Threshold", "amp_open_test_limit_phase2_lower", "70", str);
	stp_test->basic_threshold.amp_open_test_limit_phase2_lower = tpd_atoi(str);
	get_key_value_string("Basic_Threshold", "amp_open_test_limit_phase2_upper", "160", str);
	stp_test->basic_threshold.amp_open_test_limit_phase2_upper = tpd_atoi(str);
}

void init_tddi_test_item(struct synaptics_test_st *stp_test)
{
	char str[BUFFER_LENGTH];

	TPD_TEST_DBG("");

	/* firmware version */
	get_key_value_string("TestItem", "FW_VERSION_TEST", "0", str);
	stp_test->test_item.firmware_test = tpd_atoi(str);

	/* IC Version */
	get_key_value_string("TestItem", "IC_VERSION_TEST", "0", str);
	stp_test->test_item.ic_version_test = tpd_atoi(str);

	/* RawData Test */
	get_key_value_string("TestItem", "RAWDATA_TEST", "0", str);
	stp_test->test_item.rawdata_test = tpd_atoi(str);

	/* ee short test */
	get_key_value_string("TestItem", "EE_SHORT_TEST", "1", str);
	stp_test->test_item.eeshort_test = tpd_atoi(str);

	/* amp open test */
	get_key_value_string("TestItem", "AMP_OPEN_TEST", "1", str);
	stp_test->test_item.amp_open_test = tpd_atoi(str);

	/* noise test */
	get_key_value_string("TestItem", "NOISE_TEST", "0", str);
	stp_test->test_item.noise_test = tpd_atoi(str);

	/* accord test */
	get_key_value_string("TestItem", "ACCORD_TEST", "0", str);
	stp_test->test_item.accort_test = tpd_atoi(str);

	/* touch key test */
	get_key_value_string("TestItem", "TOUCH_KEY_TEST", "0", str);
	stp_test->test_item.touch_key_test = tpd_atoi(str);
}
void init_tddi_interface_config(struct synaptics_test_st *stp_test)
{
	char str[BUFFER_LENGTH] = {0};

	TPD_TEST_DBG("");

	get_key_value_string("Interface", "Normalize_Type", 0, str);
	stp_test->screen_cfg.isNormalize = tpd_atoi(str);
	TPD_TEST_DBG(" Normalize_Type:0x%02x. ", stp_test->screen_cfg.isNormalize);
}

/************************************************************************
* Name: set_param_data
* Brief:  load Config. Set IC series, init test items, init basic threshold,
*         int detailThreshold, and set order of test items
* Input: TestParamData, from ini file.
* Output: none
* Return: 0. No sense, just according to the old format.
***********************************************************************/
int syna_set_param_data(struct synaptics_test_st *stp_test)
{
	int retval = 0;

	TPD_TEST_DBG("Enter  syna_set_param_data.");

	/* malloc ini section/key/value data space && parse them to the structure.*/
	retval = syna_ini_get_key_data(stp_test->ini_string);
	if (retval < 0) {
		TPD_TEST_DBG("%s syna_ini_get_key_data failed return:%d.", __func__, retval);
		return retval;
	}

	retval = malloc_synaptics_test_struct(stp_test);
	if (retval < 0) {
		TPD_TEST_DBG("%s malloc_synaptics_test_struct failed return:%d.", __func__, retval);
		return retval;
	}

	init_tddi_interface_config(stp_test);
	init_tddi_test_item(stp_test);
	init_tddi_test_basic_threshold(stp_test);
	init_tddi_detail_threshold(stp_test);

	TPD_TEST_DBG("end of syna_set_param_data.");
	return 0;
}

static int _tpd_test_get_saved_filename(char *filename, int len)
{
	struct timespec ts;
	struct rtc_time tm;

	if (g_syna_save_file_name[0] != 0) {
		snprintf(filename, len, "%s", g_syna_save_file_name);
	} else {
		getnstimeofday(&ts);
		rtc_time_to_tm(ts.tv_sec, &tm);
		snprintf(filename, len, "test_data%04d%02d%02d_%02d%02d%02d.csv",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
	}
	return 0;
}

/* Save test data to SD card etc. */
static int _tpd_test_save_to_file(struct synaptics_test_st *stp_test)
{
	struct file *pfile = NULL;

	char filepath[BUFFER_LENGTH];
	loff_t pos;
	mm_segment_t old_fs;
	char saved_name[64];

	_tpd_test_get_saved_filename(saved_name, 64);

	memset(filepath, 0, sizeof(filepath));
	if (stp_test->test_result > 0)
		snprintf(filepath, sizeof(filepath), "%stest_failed_%s", stp_test->str_saved_path, saved_name);
	else
		snprintf(filepath, sizeof(filepath), "%s%s", stp_test->str_saved_path, saved_name);

	if (pfile == NULL)
		pfile = filp_open(filepath, O_CREAT | O_RDWR, 0);

	if (IS_ERR(pfile)) {
		TPD_TEST_DBG("error occurred while opening file %s.",  filepath);
		return -EIO;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	if (stp_test->result_buffer != NULL)
		vfs_write(pfile, stp_test->result_buffer, stp_test->result_length, &pos);

	filp_close(pfile, NULL);
	set_fs(old_fs);

	return 0;
}

int _tpd_save_test_to_buffer(struct synaptics_test_st *stp_test, char *str_save)
{

	return 0;
}

/* Read configuration to memory */
static int _tpd_test_read_ini_data(struct synaptics_test_st *stp_test)
{
	struct file *pfile = NULL;
	struct inode *inode = NULL;
	off_t fsize = 0;
	char filepath[BUFFER_LENGTH];
	loff_t pos = 0;
	mm_segment_t old_fs;

	memset(filepath, 0, sizeof(filepath));
	snprintf(filepath, sizeof(filepath), "%s%s", stp_test->str_ini_file_path, stp_test->str_ini_file_name);

	if (pfile == NULL)
		pfile = filp_open(filepath, O_RDONLY, 0);

	if (IS_ERR(pfile)) {
		TPD_TEST_DBG("error occurred while opening file %s.",  filepath);
		return -EIO;
	}

	inode = pfile->f_inode;
	fsize = inode->i_size;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	if (fsize <= 0)
		pr_notice("%s ERROR:Get firmware size failed",  __func__);
	else {
		if (stp_test->ini_string == NULL)
			stp_test->ini_string = tpd_malloc(fsize + 1);

		if (stp_test->ini_string == NULL)
			pr_notice("tpd_malloc failed in function:%s",  __func__);
		else {
			memset(stp_test->ini_string, 0, fsize + 1);
			vfs_read(pfile, stp_test->ini_string, fsize, &pos);
		}
	}
	filp_close(pfile, NULL);
	set_fs(old_fs);

	return fsize;
}

/* Read, parse the configuration file, initialize the test variable */
static int _tpd_test_get_testparam_from_ini(struct synaptics_test_st *stp_test)
{
	int retval = -1;

	retval = _tpd_test_read_ini_data(stp_test);

	if (retval <= 0 || stp_test->ini_string == NULL) {
		pr_notice(" - ERROR: _tpd_test_read_ini_data failed");
		return -EIO;
	}

	pr_notice("_tpd_test_read_ini_data successful");

	retval = syna_set_param_data(stp_test);

	if (retval < 0)
		return retval;

	return 0;
}

static int tpd_test_save_file_path_show(struct tpd_classdev_t *cdev,
			char *buf)
{
	ssize_t num_read_chars = 0;
	struct synaptics_test_st *stp_test = (struct synaptics_test_st *)cdev->test_node;

	mutex_lock(&stp_test->tpd_test_mutex);

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", stp_test->str_saved_path);

	mutex_unlock(&stp_test->tpd_test_mutex);

	return num_read_chars;
}

static int tpd_test_save_file_path_store(struct tpd_classdev_t *cdev,
			const char *buf)
{
	struct synaptics_test_st *stp_test = (struct synaptics_test_st *)cdev->test_node;

	mutex_lock(&stp_test->tpd_test_mutex);

	memset(stp_test->str_saved_path, 0, sizeof(stp_test->str_saved_path));
	snprintf(stp_test->str_saved_path, sizeof(stp_test->str_saved_path), "%s", buf);

	pr_notice("tpd save file path:%s.", stp_test->str_saved_path);

	mutex_unlock(&stp_test->tpd_test_mutex);

	return 0;
}

static int tpd_test_save_file_name_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(g_syna_save_file_name, 0, sizeof(g_syna_save_file_name));
	snprintf(g_syna_save_file_name, sizeof(g_syna_save_file_name), "%s", buf);

	pr_notice("save file path:%s.", g_syna_save_file_name);

	return 0;
}

static int tpd_test_save_file_name_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", g_syna_save_file_name);

	return num_read_chars;
}

static int tpd_test_ini_file_path_show(struct tpd_classdev_t *cdev,
			char *buf)
{
	ssize_t num_read_chars = 0;
	struct synaptics_test_st *stp_test = (struct synaptics_test_st *)cdev->test_node;

	mutex_lock(&stp_test->tpd_test_mutex);

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", stp_test->str_ini_file_path);

	mutex_unlock(&stp_test->tpd_test_mutex);

	return num_read_chars;
}

static int tpd_test_ini_file_path_store(struct tpd_classdev_t *cdev,
			const char *buf)
{
	struct synaptics_test_st *stp_test = (struct synaptics_test_st *)cdev->test_node;

	mutex_lock(&stp_test->tpd_test_mutex);

	memset(stp_test->str_ini_file_path, 0, sizeof(stp_test->str_ini_file_path));
	snprintf(stp_test->str_ini_file_path, sizeof(stp_test->str_ini_file_path), "%s", buf);

	pr_notice("tpd ini file path:%s.", stp_test->str_ini_file_path);

	mutex_unlock(&stp_test->tpd_test_mutex);

	return 0;
}

static int tpd_test_filename_show(struct tpd_classdev_t *cdev,
			char *buf)
{
	ssize_t num_read_chars = 0;
	struct synaptics_test_st *stp_test = (struct synaptics_test_st *)cdev->test_node;

	mutex_lock(&stp_test->tpd_test_mutex);

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", stp_test->str_ini_file_name);

	mutex_unlock(&stp_test->tpd_test_mutex);

	return num_read_chars;
}

static int tpd_test_filename_store(struct tpd_classdev_t *cdev,
			const char *buf)
{
	struct synaptics_test_st *stp_test = (struct synaptics_test_st *)cdev->test_node;

	mutex_lock(&stp_test->tpd_test_mutex);

	memset(stp_test->str_ini_file_name, 0, sizeof(stp_test->str_ini_file_name));
	snprintf(stp_test->str_ini_file_name, sizeof(stp_test->str_ini_file_name), "%s", buf);

	pr_notice("tpd fwname:%s.", stp_test->str_ini_file_name);

	mutex_unlock(&stp_test->tpd_test_mutex);

	return 0;
}

static int tpd_test_cmd_show(struct tpd_classdev_t *cdev,
			char *buf)
{
	ssize_t num_read_chars = 0;
	struct synaptics_test_st *stp_test = (struct synaptics_test_st *)cdev->test_node;
	int buffer_length = 0;
	int i_len = 0;

	mutex_lock(&stp_test->tpd_test_mutex);

	pr_notice("tpd %s [func] in.\n", __func__);


	i_len = snprintf(buf, TEST_TEMP_LENGTH, "%d,%d,%d,%d", stp_test->test_result, stp_test->screen_cfg.i_txNum,
		stp_test->screen_cfg.i_rxNum, stp_test->rawdata_failed_count);
	pr_notice("tpd %s [func] test resutl:0x%x && rawdata node failed count:0x%x.\n",
		__func__, stp_test->test_result, stp_test->rawdata_failed_count);
	pr_notice("tpd %s [func] resutl && failed cout string length:0x%x.\n", __func__, i_len);

	buffer_length = (stp_test->special_buffer_length + 1) > (PAGE_SIZE - i_len) ?
		(PAGE_SIZE - i_len - 1) : stp_test->special_buffer_length;
	pr_notice("tpd %s [func] failed node string length:0x%x, buffer_length:0x%x.\n",
		__func__, stp_test->special_buffer_length, buffer_length);

	if (stp_test->special_buffer != NULL && buffer_length > 0) {
		memcpy(buf + i_len, stp_test->special_buffer, buffer_length);
		buf[buffer_length + i_len] = '\0';
	}

	pr_notice("tpd %s test:%s.", __func__, buf);

	num_read_chars = buffer_length + i_len;

	mutex_unlock(&stp_test->tpd_test_mutex);

	return num_read_chars;
}

static int tpd_test_cmd_store(struct tpd_classdev_t *cdev,
			const char *buf)
{
	struct synaptics_test_st *stp_test = (struct synaptics_test_st *)cdev->test_node;

	unsigned long command = 0;
	int retval = -1;

	retval = sstrtoul(buf, 10, &command);
	if (retval) {
		TPD_TEST_DBG("invalid param:%s", buf);
		return -EINVAL;
	}

	pr_notice("tpd %s [func] command:%ld, ini filename:%s.\n", __func__, command, stp_test->str_ini_file_name);

	mutex_lock(&stp_test->tpd_test_mutex);

	/* command 1:open node; 2:start test; 3:close node. */
	if (command == 1) {    /* close test node */
		if (stp_test->result_buffer == NULL) {
			int buffer[5];

			tpd_synaptics_get_channel_setting(&buffer[0]);
			stp_test->screen_cfg.i_txNum = buffer[0];
			stp_test->screen_cfg.i_rxNum = buffer[1];

			stp_test->result_buffer = (char *)tpd_malloc(TEST_RESULT_LENGTH);
			stp_test->result_length = 0;
		}
		if (stp_test->special_buffer == NULL) {
			stp_test->special_buffer = (char *)tpd_malloc(TEST_RESULT_LENGTH);
			stp_test->special_buffer_length = 0;
			stp_test->rawdata_failed_count = 0;
		}
		if (stp_test->temp_buffer == NULL) {
			stp_test->temp_buffer = (char *)tpd_malloc(TEST_TEMP_LENGTH);
		}
		stp_test->node_opened = 1;
	} else if (command == 2) {
		if (stp_test->node_opened == 1) {
			stp_test->result_length = 0;    /* clean buffer. */
			_tpd_test_get_testparam_from_ini(stp_test);
			/* start test function */
			synaptics_tddi_test_start(stp_test);
			_tpd_test_save_to_file(stp_test);
		} else {
			stp_test->test_result = -1;
			TPD_TEST_DBG("command: %ld, but node not opened.", command);
		}
	} else if (command == 3) {
		stp_test->node_opened = 0;
		free_synaptics_test_struct(stp_test);
	} else
		TPD_TEST_DBG("invalid command %ld", command);

	mutex_unlock(&stp_test->tpd_test_mutex);

	return 0;
}


static int tpd_test_node_data_show(struct tpd_classdev_t *cdev,
			char *buf)
{
	ssize_t num_read_chars = 0;
	int iLen = 0;
	int iRow = 0, iCol = 0;
	int iTxNum = 0, iRxNum = 0;
	struct synaptics_test_st *stp_test = (struct synaptics_test_st *)cdev->test_node;
	int *node_data;
	int full_raw_report_size = 0;
	int retval = -1;
	int buffer[5];

	tpd_synaptics_get_channel_setting(&buffer[0]);
	stp_test->screen_cfg.i_txNum = buffer[0];
	stp_test->screen_cfg.i_rxNum = buffer[1];

	iTxNum = stp_test->screen_cfg.i_txNum;
	iRxNum = stp_test->screen_cfg.i_rxNum;
	full_raw_report_size = iTxNum * iRxNum;

	mutex_lock(&stp_test->tpd_test_mutex);

	stp_test->node_data_return_type = 1;

	node_data = (int *)tpd_malloc(sizeof(int) * full_raw_report_size);
	if (node_data == NULL)
		pr_notice("tpd %s error for malloc rawdata buffer", __func__);
	else {
		if (stp_test->node_data_return_type == 1) {
			memset(node_data, 0, sizeof(int) * full_raw_report_size);
			retval = tpd_synaptics_get_full_rawdata(node_data, full_raw_report_size);
			if (retval == 0) {
				for (iRow = 0; iRow < iTxNum; iRow++) {
					for (iCol = 0; iCol < iRxNum; iCol++) {
						if (iCol == (iRxNum - 1))
							iLen += snprintf(buf + iLen, TEST_TEMP_LENGTH, "%d\n",
								node_data[iRow * iRxNum + iCol]);
						else
							iLen += snprintf(buf + iLen, TEST_TEMP_LENGTH, "%d, ",
								node_data[iRow * iRxNum + iCol]);
					}
				}
			}
			tpd_synaptics_test_reset(stp_test);
		}
		tpd_free(node_data);
	}

	num_read_chars = iLen;
	mutex_unlock(&stp_test->tpd_test_mutex);

	return num_read_chars;
}

static int tpd_test_node_data_store(struct tpd_classdev_t *cdev,
			const char *buf)
{
	int current_DataCmd = 0, current_DataType = 0, current_ReturnDataType = 0;
	struct synaptics_test_st *stp_test = (struct synaptics_test_st *)cdev->test_node;

	if (sscanf(buf, "%d %d %d", &current_DataCmd, &current_DataType, &current_ReturnDataType) != 3)
		return -EINVAL;

	pr_notice("tpd %s before cmd:%d, type:%d , current cmd:%d, type:%d .", __func__,
		stp_test->node_data_command, stp_test->node_data_type, current_DataCmd, current_DataType);

	stp_test->node_data_return_type = 1;

	if ((stp_test->node_data_command == 1 && current_DataCmd == 1) ||
		(stp_test->node_data_command == 2 && current_DataCmd == 2)) {
		pr_notice("tpd %s ERROR operation AS before .", __func__);
		return -EINVAL;
	}
	if (current_DataCmd == 2 && current_DataType != stp_test->node_data_type) {
		pr_notice("tpd %s warning close type not same.", __func__);
		current_DataType = stp_test->node_data_type;
	}
	stp_test->node_data_type = current_DataType;
	stp_test->node_data_command = current_DataCmd;

	pr_notice("tpd %s get node data cmd:%d, type:%d .", __func__,
		stp_test->node_data_command, stp_test->node_data_type);

	mutex_lock(&stp_test->tpd_test_mutex);
	/* cmd 1:start 2:stop */
	stp_test->node_data_return_type = 1;

	mutex_unlock(&stp_test->tpd_test_mutex);

	return 0;
}
static int tpd_test_channel_show(struct tpd_classdev_t *cdev,
			char *buf)
{
	ssize_t num_read_chars = 0;
	struct synaptics_test_st *stp_test = (struct synaptics_test_st *)cdev->test_node;
	int buffer[5];

	tpd_synaptics_get_channel_setting(&buffer[0]);
	stp_test->screen_cfg.i_txNum = buffer[0];
	stp_test->screen_cfg.i_rxNum = buffer[1];

	num_read_chars = snprintf(buf, PAGE_SIZE, "%d %d %d %d %d",
		buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);

	return num_read_chars;
}

/*static DEVICE_ATTR(tpd_test_channel_setting, S_IRUGO | S_IRUSR, tpd_test_channel_show, NULL);
static DEVICE_ATTR(tpd_test_save_file_path, S_IRUGO | S_IWUSR, tpd_test_save_file_path_show,
		tpd_test_save_file_path_store);
static DEVICE_ATTR(tpd_test_ini_file_path, S_IRUGO | S_IWUSR, tpd_test_ini_file_path_show,
		tpd_test_ini_file_path_store);
static DEVICE_ATTR(tpd_test_filename, S_IRUGO | S_IWUSR, tpd_test_filename_show, tpd_test_filename_store);
static DEVICE_ATTR(tpd_test_cmd, S_IRUGO | S_IWUSR, tpd_test_cmd_show, tpd_test_cmd_store);
static DEVICE_ATTR(tpd_test_node_data, S_IRUGO | S_IWUSR, tpd_test_node_data_show, tpd_test_node_data_store);*/

/* add your attr in here*/
/*static struct attribute *tpd_test_attributes[] = {
	&dev_attr_tpd_test_filename.attr,
	&dev_attr_tpd_test_node_data.attr,
	&dev_attr_tpd_test_cmd.attr,
	&dev_attr_tpd_test_ini_file_path.attr,
	&dev_attr_tpd_test_save_file_path.attr,
	&dev_attr_tpd_test_channel_setting.attr,
	NULL
};

static struct attribute_group tpd_test_attribute_group = {
	.attrs = tpd_test_attributes
};*/
static int syna_get_tp_rawdata(char *buf, int length)
{
	int iLen = 0;
	int iRow = 0, iCol = 0;
	int iTxNum = 0, iRxNum = 0;
	struct synaptics_test_st *stp_test = &gSt_synaptics_test;
	int *node_data = NULL;
	int full_raw_report_size = 0;
	int retval = -1;
	int buffer[5] = {0};

	tpd_synaptics_get_channel_setting(&buffer[0]);
	stp_test->screen_cfg.i_txNum = buffer[0];
	stp_test->screen_cfg.i_rxNum = buffer[1];

	iTxNum = stp_test->screen_cfg.i_txNum;
	iRxNum = stp_test->screen_cfg.i_rxNum;
	full_raw_report_size = iTxNum * iRxNum;

	mutex_lock(&stp_test->tpd_test_mutex);

	node_data = (int *)tpd_malloc(sizeof(int) * full_raw_report_size);
	if (node_data == NULL)
		pr_notice("tpd %s error for malloc rawdata buffer", __func__);
	else {
		memset(node_data, 0, sizeof(int) * full_raw_report_size);
		retval = tpd_synaptics_get_full_rawdata(node_data, full_raw_report_size);
		if (retval == 0) {
			for (iRow = 0; iRow < iTxNum; iRow++) {
				for (iCol = 0; iCol < iRxNum; iCol++) {
					if ((length - iLen) <= 5)
						break;
					if (iCol == (iRxNum - 1))
						iLen += snprintf(buf + iLen, length - iLen, "%d\n",
							node_data[iRow * iRxNum + iCol]);
					else
						iLen += snprintf(buf + iLen, length - iLen, "%d, ",
							node_data[iRow * iRxNum + iCol]);
				}
			}
		}
		tpd_synaptics_test_reset(stp_test);
		tpd_free(node_data);
	}
	mutex_unlock(&stp_test->tpd_test_mutex);

	return iLen;
}
int init_tpd_test(struct tpd_classdev_t *cdev)
{
	int err = 0;
	struct synaptics_test_st *stp_test = NULL;

	if (IS_ERR(cdev->dev)) {
		pr_notice("[tpd] %s() - tpd_fw_cdev dev null.", __func__);
		goto out;
	} else {
		cdev->tpd_test_set_save_filepath = tpd_test_save_file_path_store;
		cdev->tpd_test_get_save_filepath = tpd_test_save_file_path_show;
		cdev->tpd_test_set_save_filename = tpd_test_save_file_name_store;
		cdev->tpd_test_get_save_filename = tpd_test_save_file_name_show;
		cdev->tpd_test_set_ini_filepath = tpd_test_ini_file_path_store;
		cdev->tpd_test_get_ini_filepath = tpd_test_ini_file_path_show;
		cdev->tpd_test_set_filename = tpd_test_filename_store;
		cdev->tpd_test_get_filename = tpd_test_filename_show;
		cdev->tpd_test_set_cmd = tpd_test_cmd_store;
		cdev->tpd_test_get_cmd = tpd_test_cmd_show;
		cdev->tpd_test_set_node_data_type = tpd_test_node_data_store;
		cdev->tpd_test_get_node_data = tpd_test_node_data_show;
		cdev->tpd_test_get_channel_info = tpd_test_channel_show;
		cdev->tpd_test_get_tp_rawdata = syna_get_tp_rawdata;
	}

	stp_test = &gSt_synaptics_test;

	stp_test->node_opened = 0;
	stp_test->ini_string = NULL;
	stp_test->result_buffer = NULL;
	stp_test->result_length = 0;
	stp_test->test_result = 0;
	stp_test->node_data_type = 0;

	stp_test->special_buffer_length = 0;

	strlcpy(stp_test->str_saved_path, "/data/data/", BUFFER_LENGTH);
	strlcpy(stp_test->str_ini_file_name, "syna_test_sensor_0.ini", BUFFER_LENGTH);
	strlcpy(stp_test->str_ini_file_path, "/system/etc/", BUFFER_LENGTH);

	mutex_init(&stp_test->tpd_test_mutex);

	cdev->test_node = (void *)stp_test;

out:
	return err;
}

#endif

