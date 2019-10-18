#ifndef _CTS_SELFTEST_H_
#define _CTS_SELFTEST_H_
#include "cts_core.h"

#define TEST_RESULT_INFO_LEN    (2 * 1024)
#define TEST_ALL_INFO_LEN       (10 * 1024)
#define MAX_FILE_PATH_LEN       64
#define MAX_FILE_NAME_LEN       64
#define MAX_FILE_FULL_PATH_LEN  128
#define MAX_NAME_LEN_20  20

#define MAX_TEST_ITEM_NUM       8
enum cts_test_type {
	FIRMWARE_VERSION_TEST_CODE = 0,
	RAWDATA_TEST_CODE,
	OPEN_CIRCUITE_TEST_CODE,
	SHORT_CIRCUITE_TEST_CODE,
};

int cts_start_selftest(struct cts_device *cts_dev);
int cts_init_selftest(struct cts_device *cts_dev);
void cts_deinit_selftest(struct cts_device *cts_dev);

typedef struct TestItem {
	u8 code;
	u8 *data;
} T_TestItem, *PT_TestItem;

#define KEY_FIELD_ITEM       "TestItem"
#define KEY_FIELD_PARMETER   "TestParameter"

#define KEY_ITEM_FIRMWARE_VERSION "FIRMWARE_VERSION_TEST"
#define KEY_ITEM_RAWDATA          "RAWDATA_TEST"
#define KEY_ITEM_OPEN_CIRCUITE    "OPEN_CIRCUITE_TEST"
#define KEY_ITEM_SHORT_CIRCUITE   "SHORT_CIRCUITE_TEST"

#define KEY_PARAMETER_FIRMWARE_VERSION  "FIRMWARE_VERSION"
#define KEY_PARAMETER_RAWDATA_MIN		"RAWDATA_THRESHOLD_MIN"
#define KEY_PARAMETER_RAWDATA_MAX		"RAWDATA_THRESHOLD_MAX"
#define KEY_OPEN_CIRCUITE_THRES		"OPEN_CIRCUITE_THRESHOLD"
#define KEY_SHORT_CIRCUITE_THRES		"SHORT_CIRCUITE_THRESHOLD"

#define DEFAULT_FIRMWARE_VERSION_TEST_VAL 0x0400
#define DEFAULT_RAWDATA_TEST_MIN          500
#define DEFAULT_RAWDATA_TEST_MAX          3000
#define DEFAULT_OPEN_TEST_THRES           500
#define DEFAULT_SHORT_TEST_THRES          200

typedef struct {
	int version;
} T_VersionTestVar, *PT_VersionTestVar;

typedef struct {
	s16 min_thres;
	s16 max_thres;
} T_RawdataTestVar, *PT_RawdataTestVar;

typedef struct {
	u16 thres;
} T_OpenTestVar, *PT_OpenTestVar;

typedef struct {
	u16 thres;
} T_ShortTestVar, *PT_ShortTestVar;

#define CTS_KEYWORD_MAX_NUM  20
#define CTS_KEYWORD_FIELD_MAXLEN 20
#define CTS_KEYWORD_KEY_MAXLEN   30
#define CTS_KEYWORD_VAL_MAXLEN   10
#define CTS_KEYWORD_LINE_MAXLEN  50

typedef struct {
	char field[CTS_KEYWORD_FIELD_MAXLEN];
	char key[CTS_KEYWORD_KEY_MAXLEN];
	char val[CTS_KEYWORD_VAL_MAXLEN];
} T_KeyWord, *PT_KeyWord;

typedef struct {
	struct cts_device *cts_dev;

	char *ini_file_buf;

	u16 test_muster;
	u16 test_result;

	u8 testitem_num;
	T_TestItem testitem[MAX_TEST_ITEM_NUM];

	u8 keyword_num;
	T_KeyWord keyword[CTS_KEYWORD_MAX_NUM];

	long all_test_info_len;
	char *all_test_info;
	long test_result_info_len;
	char *test_result_info;
	u16 *rawdata;

	T_VersionTestVar version_test_var;
	T_RawdataTestVar rawdata_test_var;
	T_OpenTestVar open_test_var;
	T_ShortTestVar short_test_var;
} T_SelftestData, *PT_SelftestData;

#define cts_print_test_info(fmt, args...) do {\
	if (selftestdata->all_test_info != NULL) { \
		selftestdata->all_test_info_len += snprintf(selftestdata->all_test_info + \
		selftestdata->all_test_info_len, TEST_ALL_INFO_LEN \
		- selftestdata->all_test_info_len, fmt, ##args);\
	} \
} while (0)

#define cts_print_result_info(fmt, args...) do {\
	if (selftestdata->all_test_info != NULL) { \
		selftestdata->test_result_info_len += snprintf(selftestdata->test_result_info + \
		selftestdata->test_result_info_len, TEST_RESULT_INFO_LEN \
		- selftestdata->test_result_info_len, fmt, ##args);\
	} \
} while (0)

#endif
