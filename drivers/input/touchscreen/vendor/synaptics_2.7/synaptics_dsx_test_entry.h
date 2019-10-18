#ifndef _SYNAPTICS_DSX_TEST_ENTRY_H_
#define _SYNAPTICS_DSX_TEST_ENTRY_H_

#include "synaptics_dsx_test_parse_ini.h"
#include "../tpd_sys.h"

#define TEST_PASS			0
#define TEST_BEYOND_MAX_LIMIT		0x0001
#define TEST_BEYOND_MIN_LIMIT		0x0002
#define TEST_BEYOND_ACCORD_LIMIT	0x0004
#define TEST_BEYOND_OFFSET_LIMIT	0x0008
#define TEST_BEYOND_JITTER_LIMIT	0x0010
#define TEST_KEY_BEYOND_MAX_LIMIT	0x0020
#define TEST_KEY_BEYOND_MIN_LIMIT	0x0040
#define TEST_MODULE_TYPE_ERR		0x0080
#define TEST_VERSION_ERR			0x0100
#define TEST_GT_OPEN				0x0200
#define TEST_GT_SHORT				0x0400
#define TEST_BEYOND_UNIFORMITY_LIMIT 0x0800
#define TEST_BEYOND_FLASH_LAYER_LIMIT  0x1000
#define TEST_BETWEEN_ACCORD_AND_LINE  0x2000

#define TX_NUM_MAX			60
#define RX_NUM_MAX			60
#define NUM_MAX		((TX_NUM_MAX) * (RX_NUM_MAX))
#define MAX_PATH			256

#define MAX_TEST_ITEM		20
#define MAX_GRAPH_ITEM		 20
#define MAX_CHANNEL_NUM 144

#define TEST_RESULT_LENGTH (256 * 1024)
#define TEST_TEMP_LENGTH (16 * 1024)

struct detail_threshold_st {
	unsigned char (*invalid_node)[RX_NUM_MAX];/* Invalid node, the node is not tested */

	int (*rawdata_test_min)[RX_NUM_MAX];
	int (*rawdata_test_max)[RX_NUM_MAX];
};

struct tddi_test_item_st {
	bool firmware_test;
	bool ic_version_test;
	bool rawdata_test;
	bool eeshort_test;
	bool noise_test;
	bool accort_test;
	bool touch_key_test;
	bool amp_open_test;
};

struct basic_threshold_st {
	int firmware_version;
	int ic_version;

	int max_limit_value;
	int min_limit_value;
	int rawdata_test_num_of_frames;
	int rawdata_test_interval_of_frames;

	int max_key_limit_value;
	int min_key_limit_value;

	int nosie_test_limit;
	int nosie_test_num_of_frames;

	int ee_short_test_limit_part1;
	int ee_short_test_limit_part2;

	int amp_open_int_dur_one;
	int amp_open_int_dur_two;
	int amp_open_test_limit_phase1_lower;
	int amp_open_test_limit_phase1_upper;
	int amp_open_test_limit_phase2_lower;
	int amp_open_test_limit_phase2_upper;
};

struct basic_screen_cfg_st {
	int i_txNum;
	int i_rxNum;
	int isNormalize;
	int iUsedMaxTxNum;
	int iUsedMaxRxNum;

	unsigned char iKeyNum;
};

struct synaptics_test_st {
	struct basic_screen_cfg_st screen_cfg;
	struct tddi_test_item_st test_item;
	struct basic_threshold_st basic_threshold;
	struct detail_threshold_st detail_threshold;
	int node_opened;
	struct mutex tpd_test_mutex;

	int test_result;

	unsigned char str_saved_path[256];
	unsigned char str_ini_file_path[256];
	unsigned char str_ini_file_name[128];

	int node_data_type;
	int node_data_command;
	int node_data_return_type;

	char *ini_string;

	char *result_buffer;
	int result_length;
	char *temp_buffer;

	char *special_buffer;
	int special_buffer_length;
	int rawdata_failed_count;
};

int init_tpd_test(struct tpd_classdev_t *cdev);
int tpd_synaptics_get_channel_setting(int *buffer);
int tpd_synaptics_get_full_rawdata(int *rawdata, int length);
int tpd_synaptics_amp_open_test(struct synaptics_test_st *stp_test);
int tpd_synaptics_ee_short_test(struct synaptics_test_st *stp_test);
int tpd_synaptics_test_reset(struct synaptics_test_st *stp_test);
int synaptics_tddi_test_start(struct synaptics_test_st *stp_test);
int save_ito_array_data_to_file(struct synaptics_test_st *stp_test, char *title_info,
	void *data, int length, int size, int i_row, int i_col);

#endif
