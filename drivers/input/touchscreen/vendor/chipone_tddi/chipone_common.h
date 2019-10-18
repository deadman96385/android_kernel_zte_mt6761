#ifndef _CHIPONE_COMMON_H_
#define _CHIPONE_COMMON_H_

#define DEFAULT_INI_FILE_PATH  "/etc/"
#define DEFAULT_INI_FILE_NAME  "chipone_test.cfg"
#define DEFAULT_SAVE_FILE_PATH "/sdcard/"
#define DEFAULT_RESULT_FILE_NAME "testresult.txt"
#define DEFAULT_INFO_FILE_NAME "testinfo.csv"

int cts_register_fw_class(struct cts_device *cts_dev);

#define TP_TEST_INIT	1
#define TP_TEST_START	2
#define TP_TEST_END		3
#define TEST_TEMP_LENGTH 8
#define TEST_BEYOND_MAX_LIMIT		0x0001
#define TEST_BEYOND_MIN_LIMIT		0x0002
#define TEST_VERSION_ERR			0x0100
#define TEST_GT_OPEN				0x0200
#define TEST_GT_SHORT				0x0400
#define TP_NODE_NUMBER  (cts_dev->fwdata.rows * cts_dev->fwdata.cols)
#endif
