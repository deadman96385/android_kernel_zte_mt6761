#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include "tlsc6x_main.h"
#include "tpd_sys.h"

#define TEST_TEMP_LENGTH 8
#define MAX_ALLOC_BUFF 256
#define TEST_PASS	0
#define TEST_BEYOND_MAX_LIMIT		0x0001
#define TEST_BEYOND_MIN_LIMIT		0x0002

#define TP_TEST_INIT		1
#define TP_TEST_START	2
#define TP_TEST_END		3
#define MAX_NAME_LEN_50  50
#define MAX_NAME_LEN_20  20

#if defined(TLSC_TP_PROC_SELF_TEST)
extern char *g_tlsc_crtra_file;
extern char *g_sensor_result_file;
extern unsigned short g_allch_num;
#endif
extern unsigned char real_suspend_flag;
extern void tlsc6x_tp_cfg_version(void);
extern int tlsc6x_do_suspend(void);
extern int tlsc6x_do_resume(void);

int tlsc6x_vendor_id = 0;
int tlsc6x_test_failed_count = 0;
int tlsc6x_tptest_result = 0;
char tlsc6x_criteria_csv_name[MAX_NAME_LEN_50] = { 0 };
char g_tlsc6x_save_file_path[MAX_NAME_LEN_50] = { 0 };
char g_tlsc6x_save_file_name[MAX_NAME_LEN_50] = { 0 };
char tlsc6x_vendor_name[MAX_NAME_LEN_50] = { 0 };

u8 *tlsc6x_test_result_node = NULL;
char *g_file_path = NULL;
struct tpvendor_t tlsc6x_vendor_l[] = {
	{0x03, "Yikuailai"},
	{0x1B, "Lead"},
	{0x27, "Jingtai"},
	{0x28, "Lianchuang"},
	{0x29, "Yikuailai_2"},
	{0x00, "Unknown"},
};

static int tlsc6x_get_chip_vendor(int vendor_id)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(tlsc6x_vendor_l); i++) {
		if ((tlsc6x_vendor_l[i].vendor_id == vendor_id) || (tlsc6x_vendor_l[i].vendor_id == 0X00)) {
			tlsc_info("vendor_id is 0x%x.\n", vendor_id);
			strlcpy(tlsc6x_vendor_name, tlsc6x_vendor_l[i].vendor_name, sizeof(tlsc6x_vendor_name));
			tlsc_info("tlsc6x_vendor_name: %s.\n", tlsc6x_vendor_name);
			break;
		}
	}
	return 0;
}

int tlsc6x_get_tp_vendor_info(void)
{
	unsigned int prject_id = 0;
	unsigned int vendor_sensor_id = 0;
	unsigned int full_id = 0;

	prject_id = g_tlsc6x_cfg_ver & 0x1ff;
	vendor_sensor_id = (g_tlsc6x_cfg_ver >> 9) & 0x7f;
	full_id = g_tlsc6x_cfg_ver & 0xffff;
	tlsc_info("vendor_sensor_id =0x%x, prject_id=0x%x\n", vendor_sensor_id, prject_id);

	tlsc6x_get_chip_vendor(vendor_sensor_id);
	snprintf(tlsc6x_criteria_csv_name, sizeof(tlsc6x_criteria_csv_name),
		"chsc_criteria_%02x.bin", full_id);
	g_tlsc_crtra_file = tlsc6x_criteria_csv_name;

	return 0;
}

static int tpd_init_tpinfo(struct tpd_classdev_t *cdev)
{
	tlsc_info("tpd_init_tpinfo\n");
	tlsc6x_tp_cfg_version();
	tlsc6x_set_nor_mode();
	strlcpy(cdev->ic_tpinfo.vendor_name, tlsc6x_vendor_name, sizeof(cdev->ic_tpinfo.vendor_name));
	snprintf(cdev->ic_tpinfo.tp_name, sizeof(cdev->ic_tpinfo.tp_name), "chsc6306");
	cdev->ic_tpinfo.chip_model_id = TS_CHIP_TLSC;
	cdev->ic_tpinfo.firmware_ver = (g_tlsc6x_boot_ver >> 8) & 0xff;
	cdev->ic_tpinfo.config_ver = (g_tlsc6x_cfg_ver >> 26) & 0x3f;
	cdev->ic_tpinfo.module_id = g_tlsc6x_cfg_ver & 0xffff;
	cdev->ic_tpinfo.i2c_addr = 0x2e;
	return 0;
}

static int tlsc6x_i2c_reg_read(struct tpd_classdev_t *cdev, char addr, u8 *data, int len)
{
	if (tlsc6x_i2c_read(g_tp_drvdata->client, &addr, 1, data, len) < 0) {
		tlsc_err("%s: i2c access fail!\n", __func__);
		return -EIO;
	}

	return 0;
}

static int tlsc6x_i2c_reg_write(struct tpd_classdev_t *cdev, char addr, u8 *data, int len)
{
	int i = 0;

	if (len == 1) {
		goto WRITE_ONE_REG;
	} else {
		char *tmpbuf = NULL;

		tmpbuf = kzalloc(len + 1, GFP_KERNEL);
		if (!tmpbuf) {
			tlsc_info("allocate memory failed!\n");
			return -ENOMEM;
		}
		tmpbuf[0] = addr & 0xFF;
		for (i = 1; i <= len; i++) {
			tmpbuf[i] = data[i - 1];
		}
		if (tlsc6x_i2c_write(g_tp_drvdata->client, tmpbuf, len + 1) < 0) {
			tlsc_err("%s: i2c access fail!\n", __func__);
			return -EIO;
		}
		kfree(tmpbuf);
		return 0;
	}
WRITE_ONE_REG:
	if (tlsc6x_write_reg(g_tp_drvdata->client, addr, *data) < 0) {
			tlsc_err("%s: i2c access fail!\n", __func__);
			return -EIO;
	}
	return 0;
}

static int tlsc6x_tp_fw_upgrade(struct tpd_classdev_t *cdev, char *fw_name, int fwname_len)
{
	char fileName[128] = { 0 };

	memset(fileName, 0, sizeof(fileName));
	snprintf(fileName, sizeof(fileName), "%s", fw_name);
	fileName[fwname_len - 1] = '\0';
	tlsc_info("%s: upgrade from file(%s) start!\n", __func__, fileName);
	return tlsc6x_proc_cfg_update(fileName, 1);
}

static int tlsc6x_gpio_shutdown_config(void)
{
	return 0;
}

/* tlsc6x TP slef test*/

static int tlsc6x_test_init(void)
{
	tlsc_info("%s:enter\n", __func__);
	tlsc6x_test_result_node = kzalloc(48, GFP_KERNEL);
	g_file_path = kzalloc(MAX_ALLOC_BUFF, GFP_KERNEL);
	if ((tlsc6x_test_result_node == NULL) || (g_file_path == NULL)) {
		if (tlsc6x_test_result_node != NULL) {
			kfree(tlsc6x_test_result_node);
		}
		if (g_file_path != NULL) {
			kfree(g_file_path);
		}
		tlsc_err("%s:alloc memory failde!\n", __func__);
		return -ENOMEM;
	}
	tlsc6x_tptest_result = 0;
	tlsc6x_test_failed_count = 0;

	return 0;
}

static void tlsc6x_test_buffer_free(void)
{
	tlsc_info("%s:enter\n", __func__);
	if (tlsc6x_test_result_node != NULL) {
		kfree(tlsc6x_test_result_node);
	}
	if (g_file_path != NULL) {
		kfree(g_file_path);
	}
}

int tlsc6x_save_failed_node(int failed_node)
{
	if (tlsc6x_test_result_node == NULL) {
		return -EPERM;
	}
	if (tlsc6x_test_result_node[failed_node] == 0) {
		tlsc6x_test_failed_count++;
		tlsc6x_test_result_node[failed_node] = 1;
	}
	return 0;
}

static int tpd_test_save_file_path_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(g_tlsc6x_save_file_path, 0, sizeof(g_tlsc6x_save_file_path));
	snprintf(g_tlsc6x_save_file_path, sizeof(g_tlsc6x_save_file_path), "%s", buf);

	tlsc_info("save file path:%s.", g_tlsc6x_save_file_path);

	return 0;
}

static int tpd_test_save_file_path_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", g_tlsc6x_save_file_path);

	return num_read_chars;
}

static int tpd_test_save_file_name_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(g_tlsc6x_save_file_name, 0, sizeof(g_tlsc6x_save_file_name));
	snprintf(g_tlsc6x_save_file_name, sizeof(g_tlsc6x_save_file_name), "%s", buf);

	tlsc_info("save file path:%s.", g_tlsc6x_save_file_name);

	return 0;
}

static int tpd_test_save_file_name_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", g_tlsc6x_save_file_name);

	return num_read_chars;
}

static int tpd_test_cmd_show(struct tpd_classdev_t *cdev, char *buf)
{
	int k;
	int i_len;
	char tmpbuf[8];
	ssize_t num_read_chars = 0;

	tlsc_info("%s:enter\n", __func__);
	i_len = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d",
		tlsc6x_tptest_result, 0, (g_allch_num&0xff), tlsc6x_test_failed_count);
	tlsc_info("tpd test resutl:%d && rawdata node failed count:%d.\n", tlsc6x_tptest_result,
		tlsc6x_test_failed_count);

	if (tlsc6x_test_result_node != NULL) {
		for (k = 0; k < 48; k++) {
			if (tlsc6x_test_result_node[k]) {
				snprintf(tmpbuf, 7, ",0,%d", k);
				i_len += snprintf(buf + i_len, PAGE_SIZE - i_len, tmpbuf);
			}
		}
	}
	tlsc_err("tpd  test:%s.\n", buf);
	num_read_chars = i_len;
	return num_read_chars;
}

static int tpd_test_cmd_store(struct tpd_classdev_t *cdev, const char *buf)
{
	int val = 0;
	int retval = 0;
	unsigned long command = 0;

	tlsc_info("%s:enter\n", __func__);
	retval = kstrtoul(buf, 10, &command);
	if (retval) {
		tlsc_err("invalid param:%s", buf);
		return -EIO;
	}
	if (command == TP_TEST_INIT) {
		retval = tlsc6x_test_init();
		if (retval < 0) {
			tlsc_err("%s:alloc memory failde!\n", __func__);
			return -ENOMEM;
		}
	} else if (command == TP_TEST_START) {
		tlsc_info("%s:start TP test.\n", __func__);
		if (g_file_path != NULL) {
			snprintf(g_file_path, MAX_ALLOC_BUFF, "%s%s", g_tlsc6x_save_file_path, g_tlsc6x_save_file_name);
			g_sensor_result_file = g_file_path;
		}
		val = tlsc6x_chip_self_test();
		if (val == 0x00) {
			tlsc6x_tptest_result = TEST_PASS;
			tlsc_info("Self_Test Pass\n");
		} else if (val >= 0x01 && val <= 0x03) {
			tlsc6x_tptest_result = tlsc6x_tptest_result | TEST_BEYOND_MAX_LIMIT | TEST_BEYOND_MIN_LIMIT;
			tlsc_err("Self_Test Fail\n");
		} else {
			tlsc6x_tptest_result = -EIO;
			tlsc_err("self test data init Fail\n");
		}
	} else if (command == TP_TEST_END) {
		tlsc6x_test_buffer_free();
	} else {
		tlsc_err("invalid command %ld", command);
	}
	return 0;
}

static int tpd_test_channel_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%d %d", 4, 8);

	return num_read_chars;
}

static int tlsc6x_tp_suspend_show(struct tpd_classdev_t *cdev)
{
	cdev->tp_suspend = real_suspend_flag;
	return cdev->tp_suspend;
}

static int tlsc6x_set_tp_suspend(struct tpd_classdev_t *cdev, int enable)
{
	if (enable) {
		tlsc6x_do_suspend();
	} else {
		tlsc6x_do_resume();
	}
	cdev->tp_suspend = real_suspend_flag;
	return cdev->tp_suspend;
}

void tlsc6x_tpd_register_fw_class(void)
{
	tlsc_info("tpd_register_fw_class\n");

	tpd_fw_cdev.get_tpinfo = tpd_init_tpinfo;
	tpd_fw_cdev.tp_i2c_reg_read = tlsc6x_i2c_reg_read;
	tpd_fw_cdev.tp_i2c_reg_write = tlsc6x_i2c_reg_write;
	tpd_fw_cdev.reg_char_num = REG_CHAR_NUM_2;
	tpd_fw_cdev.tp_fw_upgrade = tlsc6x_tp_fw_upgrade;
	tpd_fw_cdev.tpd_gpio_shutdown = tlsc6x_gpio_shutdown_config;
	tpd_fw_cdev.tp_suspend_show = tlsc6x_tp_suspend_show;
	tpd_fw_cdev.set_tp_suspend = tlsc6x_set_tp_suspend;

	tpd_fw_cdev.tpd_test_set_save_filepath = tpd_test_save_file_path_store;
	tpd_fw_cdev.tpd_test_get_save_filepath = tpd_test_save_file_path_show;
	tpd_fw_cdev.tpd_test_set_save_filename = tpd_test_save_file_name_store;
	tpd_fw_cdev.tpd_test_get_save_filename = tpd_test_save_file_name_show;
	tpd_fw_cdev.tpd_test_set_cmd = tpd_test_cmd_store;
	tpd_fw_cdev.tpd_test_get_cmd = tpd_test_cmd_show;
	tpd_fw_cdev.tpd_test_get_channel_info = tpd_test_channel_show;
	snprintf(g_tlsc6x_save_file_path, sizeof(g_tlsc6x_save_file_path), "%s", "/sdcard/");
	snprintf(g_tlsc6x_save_file_name, sizeof(g_tlsc6x_save_file_name), "%s", "chsc_test_result");
}
