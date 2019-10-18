#define LOG_TAG         "Selftest"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sysfs.h"
#include "cts_selftest.h"
#include "cts_selftest.h"
#include "cts_selftest_ini.h"
#include "cts_test.h"

PT_SelftestData selftestdata = NULL;

extern char ini_file_path[];
extern char ini_file_name[];
extern char save_file_path[];
extern char save_test_info_file_name[];
extern char save_test_result_file_name[];
extern int cts_save_failed_node(struct cts_device *cts_dev, int failed_node);
extern void cts_clean_test_buffer(struct cts_device *cts_dev);

#define RAWDATA_BUFFER_SIZE(cts_dev) \
		(cts_dev->fwdata.rows * cts_dev->fwdata.cols * 2)

int cts_init_test_item(void);
int cts_show_test_item(void);
u16 max_rawdata = 0x0000;
u16 min_rawdata = 0xffff;

void cts_strupr(char *str)
{
	int i;

	for (i = 0; str[i] != '\0'; i++) {
		if (str[i] >= 'a' && str[i] <= 'z') {
			str[i] -= 0x20;
		}
	}
}

int cts_get_version_test_var(void)
{
	int i = 0;
	int ret = 0;

	i = cts_find_ini_word(selftestdata, KEY_FIELD_PARMETER, KEY_PARAMETER_FIRMWARE_VERSION);
	if (i < 0) {
		cts_err("cannot find version test var, use default");
		selftestdata->version_test_var.version = DEFAULT_FIRMWARE_VERSION_TEST_VAL;
	} else {
		cts_strupr(selftestdata->keyword[i].val);
		ret = sscanf(selftestdata->keyword[i].val, "0X%X", &selftestdata->version_test_var.version);
		if (ret < 0)
			cts_err("%s get version_test_var.version error.\n", __func__);
	}

	cts_info("firmware version test var: 0X%04X", selftestdata->version_test_var.version);

	return 0;
}

int cts_get_rawdata_test_var(void)
{
	int i = 0;
	int ret = 0;

	i = cts_find_ini_word(selftestdata, KEY_FIELD_PARMETER, KEY_PARAMETER_RAWDATA_MIN);
	if (i < 0) {
		cts_err("cannot find rawdata min test var, use default");
		selftestdata->rawdata_test_var.min_thres = DEFAULT_RAWDATA_TEST_MIN;
	} else {
		ret = sscanf(selftestdata->keyword[i].val, "%d", (int *)&selftestdata->rawdata_test_var.min_thres);
		if (ret < 0)
			cts_err("%s get rawdata_test_var.min_thres error.\n", __func__);
	}

	i = cts_find_ini_word(selftestdata, KEY_FIELD_PARMETER, KEY_PARAMETER_RAWDATA_MAX);
	if (i < 0) {
		cts_err("cannot find rawdata max test var, use default");
		selftestdata->rawdata_test_var.max_thres = DEFAULT_RAWDATA_TEST_MAX;
	} else {
		ret = sscanf(selftestdata->keyword[i].val, "%d", (int *)&selftestdata->rawdata_test_var.max_thres);
		if (ret < 0)
			cts_err("%s get rawdata_test_var.max_thres error.\n", __func__);
	}

	cts_info("rawdata test var, min=%d, max=%d", selftestdata->rawdata_test_var.min_thres,
		 selftestdata->rawdata_test_var.max_thres);

	return 0;
}

int cts_get_open_test_var(void)
{
	int i = 0;
	int ret = 0;

	i = cts_find_ini_word(selftestdata, KEY_FIELD_PARMETER, KEY_OPEN_CIRCUITE_THRES);
	if (i < 0) {
		cts_err("cannot find open test var, use default");
		selftestdata->open_test_var.thres = DEFAULT_OPEN_TEST_THRES;
	} else {
		ret = sscanf(selftestdata->keyword[i].val, "%d", (int *)&selftestdata->open_test_var.thres);
		if (ret < 0)
			cts_err("%s get open_test_var.thres error.\n", __func__);
	}

	cts_info("open test var, threshold:%d", selftestdata->open_test_var.thres);

	return 0;
}

int cts_get_short_test_var(void)
{
	int i = 0;
	int ret = 0;

	i = cts_find_ini_word(selftestdata, KEY_FIELD_PARMETER, KEY_SHORT_CIRCUITE_THRES);
	if (i < 0) {
		cts_err("cannot find short test var, use default");
		selftestdata->short_test_var.thres = DEFAULT_SHORT_TEST_THRES;
	} else {
		ret = sscanf(selftestdata->keyword[i].val, "%d", (int *)&selftestdata->short_test_var.thres);
		if (ret < 0)
			cts_err("%s get short_test_var.thres error.\n", __func__);
	}

	cts_info("short test var, threshold:%d", selftestdata->short_test_var.thres);

	return 0;
}

int cts_parse_ini(void)
{
	char inifilepath[MAX_FILE_FULL_PATH_LEN];
	struct file *inifile;
	mm_segment_t old_fs;
	struct inode *inode = NULL;
	off_t fsize = 0;
	loff_t pos;
	int ret = 0;

	snprintf(inifilepath, MAX_FILE_FULL_PATH_LEN - 1, "%s%s", ini_file_path, ini_file_name);

	inifile = filp_open(inifilepath, O_RDONLY, 0);
	if (IS_ERR(inifile)) {
		cts_err("can not open ini file:%s", inifilepath);
		return -EIO;
	}

	inode = inifile->f_path.dentry->d_inode;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	selftestdata->ini_file_buf = kzalloc(fsize + 1, GFP_KERNEL);
	if (selftestdata->ini_file_buf == NULL) {
		cts_err("malloc ini file buf failed");
		ret = -ENOMEM;
		goto cts_parse_ini_exit;
	}
	vfs_read(inifile, selftestdata->ini_file_buf, fsize, &pos);
	cts_init_keyword(selftestdata);

	cts_init_test_item();
	cts_show_test_item();
	cts_get_version_test_var();
	cts_get_rawdata_test_var();
	cts_get_open_test_var();
	cts_get_short_test_var();

	kfree(selftestdata->ini_file_buf);
	selftestdata->ini_file_buf = NULL;
cts_parse_ini_exit:
	filp_close(inifile, NULL);
	set_fs(old_fs);

	return ret;
}

int cts_init_selftest(struct cts_device *cts_dev)
{
	selftestdata = (PT_SelftestData) kzalloc(sizeof(T_SelftestData), GFP_KERNEL);
	if (selftestdata == NULL) {
		cts_err("Malloc selftest failed");
		goto cts_init_exit;
	}

	selftestdata->all_test_info = kzalloc(TEST_ALL_INFO_LEN, GFP_KERNEL);
	if (selftestdata->all_test_info == NULL) {
		cts_err("Malloc all_test_info failed");
		goto cts_init_free_selfdata;
	}
	selftestdata->test_result_info = kzalloc(TEST_RESULT_INFO_LEN, GFP_KERNEL);
	if (selftestdata->test_result_info == NULL) {
		cts_err("Malloc test_result_info failed");
		goto cts_init_free_all_test_info;
	}
	selftestdata->rawdata = kmalloc(RAWDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
	if (selftestdata->rawdata == NULL) {
		cts_err("Malloc rawdata failed");
		goto cts_init_free_test_result_info;
	}

	selftestdata->cts_dev = cts_dev;
	max_rawdata = 0x0000;
	min_rawdata = 0xffff;
	selftestdata->test_result = 0;
	selftestdata->test_muster = 0;
	return 0;
cts_init_free_test_result_info:
	kfree(selftestdata->test_result_info);
	selftestdata->test_result_info = NULL;
cts_init_free_all_test_info:
	kfree(selftestdata->all_test_info);
	selftestdata->all_test_info = NULL;
cts_init_free_selfdata:
	kfree(selftestdata);
	selftestdata = NULL;
cts_init_exit:
	return -ENOMEM;
}

int cts_attach_testitem(u8 type, u8 *test_data)
{
	if (selftestdata->testitem_num >= MAX_TEST_ITEM_NUM) {
		cts_err("can not add test item");
		return -EPERM;
	}
	selftestdata->testitem[selftestdata->testitem_num].code = type;
	selftestdata->testitem[selftestdata->testitem_num].data = test_data;
	selftestdata->testitem_num++;
	return 0;
}

int cts_show_test_item(void)
{
	int i;

	cts_info("cts_show_test_item");
	for (i = 0; i < selftestdata->testitem_num; i++) {
		cts_info("Item %d:%d", i, selftestdata->testitem[i].code);
	}
	return 0;
}

int cts_init_test_item(void)
{
	int i;
	int val;
	int ret = 0;

	val = 0;
	i = cts_find_ini_word(selftestdata, KEY_FIELD_ITEM, KEY_ITEM_FIRMWARE_VERSION);
	if (i >= 0) {
		ret = sscanf(selftestdata->keyword[i].val, "%d", &val);
		if (ret < 0)
			cts_err("%s get val error.\n", __func__);
	}
	if (val == 0) {
		cts_info("no need firmware version test");
	} else {
		cts_info("need firmware version test");
		cts_attach_testitem(FIRMWARE_VERSION_TEST_CODE, NULL);
	}

	val = 0;
	i = cts_find_ini_word(selftestdata, KEY_FIELD_ITEM, KEY_ITEM_RAWDATA);
	if (i >= 0) {
		ret = sscanf(selftestdata->keyword[i].val, "%d", &val);
		if (ret < 0)
			cts_err("%s get val error.\n", __func__);
	}
	if (val == 0) {
		cts_info("no need rawdata test");
	} else {
		cts_info("need rawdata test");
		cts_attach_testitem(RAWDATA_TEST_CODE, NULL);
	}

	val = 0;
	i = cts_find_ini_word(selftestdata, KEY_FIELD_ITEM, KEY_ITEM_OPEN_CIRCUITE);
	if (i >= 0) {
		ret = sscanf(selftestdata->keyword[i].val, "%d", &val);
		if (ret < 0)
			cts_err("%s get val error.\n", __func__);
	}
	if (val == 0) {
		cts_info("no need open circuite test");
	} else {
		cts_info("need open circuite test");
		cts_attach_testitem(OPEN_CIRCUITE_TEST_CODE, NULL);
	}

	val = 0;
	i = cts_find_ini_word(selftestdata, KEY_FIELD_ITEM, KEY_ITEM_SHORT_CIRCUITE);
	if (i >= 0) {
		ret = sscanf(selftestdata->keyword[i].val, "%d", &val);
		if (ret < 0)
			cts_err("%s get val error.\n", __func__);
	}
	if (val == 0) {
		cts_info("no need short circuite test");
	} else {
		cts_info("need short circuite test");
		cts_attach_testitem(SHORT_CIRCUITE_TEST_CODE, NULL);
	}

	return 0;
}

int cts_selftest_version(void)
{
	int ret;

	cts_info("cts_selftest_version");
	ret = cts_fwversion_test(selftestdata->cts_dev, selftestdata->version_test_var.version);

	return ret;
}

int cts_selftest_rawdata(void)
{
	int ret = 0;
	u16 *rawdata;
	int i;
	int j;
	int index;
	u16 val;
	struct cts_device *cts_dev;

	cts_info("cts_selftest_rawdata");
	max_rawdata = 0x0000;
	min_rawdata = 0xffff;
	cts_dev = selftestdata->cts_dev;
	if (selftestdata->rawdata == NULL) {
		cts_err("rawdata test malloc failed");
		return -EPERM;
	}
	rawdata = selftestdata->rawdata;
	ret = cts_test_getrawdata(cts_dev, rawdata);
	if (ret) {
		cts_err("rawdata rest get rawdata failed");
		goto cts_selftest_rawdata_free_rawdata;
	}

	for (i = 0; i < cts_dev->fwdata.rows; i++) {
		for (j = 0; j < cts_dev->fwdata.cols; j++) {
			index = i * cts_dev->fwdata.cols + j;
			val = rawdata[index];
			if (val > max_rawdata) {
				max_rawdata = val;
			}
			if (val < min_rawdata) {
				min_rawdata = val;
			}
			if (val > selftestdata->rawdata_test_var.max_thres ||
				val < selftestdata->rawdata_test_var.min_thres) {
				cts_save_failed_node(cts_dev, index);
				ret = -1;
			}
		}
	}

cts_selftest_rawdata_free_rawdata:
	return ret;
}

int cts_selftest_open(void)
{
	int ret;

	cts_info("cts_selftest_open");

	ret = cts_open_test(selftestdata->cts_dev, selftestdata->open_test_var.thres);

	return ret;
}

int cts_selftest_short(void)
{
	int ret;

	cts_info("cts_selftest_short");
	ret = cts_short_test(selftestdata->cts_dev, selftestdata->short_test_var.thres);

	return ret;
}

int cts_save_test_to_file(void)
{
	char infofilepath[MAX_FILE_FULL_PATH_LEN], resultfilepath[MAX_FILE_FULL_PATH_LEN];
	struct file *infofile, *resultfile;
	mm_segment_t old_fs;
	loff_t pos;
	int ret = 0;

	snprintf(infofilepath, MAX_FILE_FULL_PATH_LEN - 1, "%s%s", save_file_path, save_test_info_file_name);

	infofile = filp_open(infofilepath, O_RDWR | O_CREAT | O_TRUNC, 0);
	if (IS_ERR(infofile)) {
		cts_err("can not open file:%s", infofilepath);
		return -EIO;
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(infofile, selftestdata->all_test_info, selftestdata->all_test_info_len, &pos);
	filp_close(infofile, NULL);
	set_fs(old_fs);

	snprintf(resultfilepath, MAX_FILE_FULL_PATH_LEN - 1, "%s%s", save_file_path, save_test_result_file_name);

	resultfile = filp_open(resultfilepath, O_RDWR | O_CREAT | O_TRUNC, 0);
	if (IS_ERR(resultfile)) {
		cts_err("can not open file:%s", resultfilepath);
		return -EIO;
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(resultfile, selftestdata->test_result_info, selftestdata->test_result_info_len, &pos);
	filp_close(resultfile, NULL);
	set_fs(old_fs);

	return ret;
}

int cts_produce_test_info(void)
{
	int i, j;
	u16 *rawdata;
	int index;
	struct cts_device *cts_dev;

	cts_info("cts_produce_test_info");
	cts_dev = selftestdata->cts_dev;
	if (selftestdata->test_result == 0) {
		cts_print_result_info("[TEST RESULT]\nPASSED\n\n\n\n");
		cts_print_test_info("[TEST RESULT],\nPASSED,\n\n\n\n");
	} else {
		cts_print_result_info("[TEST RESULT]\nFAILED\n\n\n\n");
		cts_print_test_info("[TEST RESULT],\nFAILED,\n\n\n\n");
	}

	cts_print_test_info("[TEST ITEM],\n");
	if (selftestdata->test_muster & (1 << FIRMWARE_VERSION_TEST_CODE)) {

		if (selftestdata->test_result & (1 << FIRMWARE_VERSION_TEST_CODE)) {
			cts_print_result_info("Firmware Test:FAILED\n");
			cts_print_test_info("Firmware Test,FAILED,\n");
		} else {
			cts_print_result_info("Firmware Test:PASSED\n");
			cts_print_test_info("Firmware Test,PASSED,\n");
		}
	}

	if (selftestdata->test_muster & (1 << RAWDATA_TEST_CODE)) {
		if (selftestdata->test_result & (1 << RAWDATA_TEST_CODE)) {
			cts_print_result_info("Rawdata Test:FAILED\n");
			cts_print_test_info("Rawdata Test,FAILED,\n");
		} else {
			cts_print_result_info("Rawdata Test:PASSED\n");
			cts_print_test_info("Rawdata Test,PASSED,\n");
		}
	}

	if (selftestdata->test_muster & (1 << OPEN_CIRCUITE_TEST_CODE)) {
		if (selftestdata->test_result & (1 << OPEN_CIRCUITE_TEST_CODE)) {
			cts_print_result_info("Open circuite Test:FAILED\n");
			cts_print_test_info("Open circuite Test,FAILED,\n");
		} else {
			cts_print_result_info("Open circuite Test:PASSED\n");
			cts_print_test_info("Open circuite Test,PASSED,\n");
		}
	}

	if (selftestdata->test_muster & (1 << SHORT_CIRCUITE_TEST_CODE)) {
		if (selftestdata->test_result & (1 << SHORT_CIRCUITE_TEST_CODE)) {
			cts_print_result_info("Short circuite Test:FAILED\n");
			cts_print_test_info("Short circuite Test,FAILED,\n");
		} else {
			cts_print_result_info("Short circuite Test:PASSED\n");
			cts_print_test_info("Short circuite Test,PASSED,\n");
		}
	}

	cts_print_test_info("\n[TEST PARAMETER],\n");
	cts_print_test_info("INI FILE:,%s%s,\n", ini_file_path, ini_file_name);
	cts_print_test_info("OUTPUT INFO FILE:,%s%s,\n", save_file_path, save_test_info_file_name);
	cts_print_test_info("OUTPUT RESULT FILE:,%s%s,\n", save_file_path, save_test_result_file_name);
	cts_print_test_info("Firmware Test:, 0x%04x,\n", selftestdata->version_test_var.version);
	cts_print_test_info("Rawdata Test:,MIN=%d, MAX=%d,\n", selftestdata->rawdata_test_var.min_thres,
			    selftestdata->rawdata_test_var.max_thres);
	cts_print_test_info("Open circuite Test:,%d,\n", selftestdata->open_test_var.thres);
	cts_print_test_info("Short circuite Test:,%d,\n", selftestdata->short_test_var.thres);

	cts_print_result_info("\n\n\n");
	cts_print_test_info("\n\n\n");
	if (selftestdata->test_muster & (1 << RAWDATA_TEST_CODE)) {
		cts_print_test_info("[RAWDATA],\n");
		cts_print_test_info("MIN=%d,MAX=%d,\n", min_rawdata, max_rawdata);
		rawdata = selftestdata->rawdata;
		cts_print_test_info(",");
		for (i = 0; i < cts_dev->fwdata.cols; i++) {
			cts_print_test_info("%d,", i);
		}
		cts_print_test_info("\n");
		for (i = 0; i < cts_dev->fwdata.rows; i++) {
			cts_print_test_info("%d,", i);
			for (j = 0; j < cts_dev->fwdata.cols; j++) {
				index = i * cts_dev->fwdata.cols + j;
				cts_print_test_info("%d,", rawdata[index]);
			}
			cts_print_test_info("\n");
		}
	}
	return 0;
}

int cts_start_selftest(struct cts_device *cts_dev)
{
	int i;
	T_TestItem testitem;
	int ret;
	int retry = 0;

	if (selftestdata == NULL) {
		cts_err("not init test data");
		return -EPERM;
	}

	ret = cts_parse_ini();
	if (ret) {
		cts_err("parse ini file error");
		return -EPERM;
	}

	if (selftestdata->testitem_num == 0) {
		cts_err("not have any test item");
		return -EPERM;
	}

	for (i = 0; i < selftestdata->testitem_num; i++) {
		testitem = selftestdata->testitem[i];
		ret = 0;
		if (testitem.code == FIRMWARE_VERSION_TEST_CODE) {
			ret = cts_selftest_version();
			if (ret) {
				cts_err("firmware test failed");
			}
		} else if (testitem.code == RAWDATA_TEST_CODE) {
			do {
				cts_clean_test_buffer(cts_dev);
				ret = cts_selftest_rawdata();
				if (ret) {
					retry++;
					cts_err("rawdata test failed, retry:%d", retry);
					msleep(500);
				} else {
					break;
				}
			} while (retry < 3);
		} else if (testitem.code == OPEN_CIRCUITE_TEST_CODE) {
			ret = cts_selftest_open();
			if (ret) {
				cts_err("open circuite test failed");
			}
		} else if (testitem.code == SHORT_CIRCUITE_TEST_CODE) {
			ret = cts_selftest_short();
			if (ret) {
				cts_err("short circuite test failed");
			}
		}
		selftestdata->test_muster |= 1 << testitem.code;
		if (ret) {
			selftestdata->test_result |= 1 << testitem.code;
		}
	}

	cts_produce_test_info();

	cts_save_test_to_file();
	return selftestdata->test_result;
}

void cts_deinit_selftest(struct cts_device *cts_dev)
{
	if (selftestdata->rawdata != NULL) {
		kfree(selftestdata->rawdata);
		selftestdata->rawdata = NULL;
	}
	if (selftestdata->all_test_info != NULL) {
		kfree(selftestdata->all_test_info);
		selftestdata->all_test_info = NULL;
	}
	if (selftestdata->test_result_info != NULL) {
		kfree(selftestdata->test_result_info);
		selftestdata->test_result_info = NULL;
	}
	if (selftestdata != NULL) {
		kfree(selftestdata);
		selftestdata = NULL;
	}
}
