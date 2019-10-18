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


#include"synaptics_dsx_test_entry.h"
#include"synaptics_dsx_test.h"

int save_rawdata_test_result(struct synaptics_test_st *stp_test, char *tmp_buffer, int length)
{
	if (stp_test->special_buffer == NULL || (stp_test->special_buffer_length + length) > TEST_RESULT_LENGTH) {
		TPD_TEST_DBG("warning:buffer is null or buffer overflow, return");
		return -EINVAL;
	}

	memcpy(stp_test->special_buffer + stp_test->special_buffer_length, tmp_buffer, length);
	stp_test->special_buffer_length += length;

	return 0;
}

int save_string_to_buffer(struct synaptics_test_st *stp_test, char *tmp_buffer, int length)
{
	if ((stp_test->result_buffer == NULL) || ((stp_test->result_length + length) > TEST_RESULT_LENGTH)) {
		TPD_TEST_DBG("warning:buffer is null or buffer overflow, return");
		return -EINVAL;
	}

	memcpy(stp_test->result_buffer + stp_test->result_length, stp_test->temp_buffer, length);
	stp_test->result_length += length;

	return 0;
}

int save_test_result(struct synaptics_test_st *stp_test)
{
	int i_len = 0;
	int result = stp_test->test_result;

	TPD_TEST_DBG("result:0x%x", result);
	i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "show test result: 0x%x\n", result);
	save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);

	if (result < 0) {
		i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "test skip!\n");
		save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);
	} else if (result == 0) {
		i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "test pass!\n");
		save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);
	} else {
		i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "test fail!\n");
		save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);
		if ((result & TEST_BEYOND_MAX_LIMIT) != 0) {
			i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "beyond max limit\n");
			save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);
		}
		if ((result & TEST_BEYOND_MIN_LIMIT) != 0) {
			i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "beyond min limit\n");
			save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);
		}
		if ((result & TEST_BEYOND_ACCORD_LIMIT) != 0) {
			i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "beyond accord limit\n");
			save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);
		}
		if ((result & TEST_KEY_BEYOND_MAX_LIMIT) != 0) {
			i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "key beyond max limit\n");
			save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);
		}
		if ((result & TEST_KEY_BEYOND_MIN_LIMIT) != 0) {
			i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "key beyond min limit\n");
			save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);
		}
		if ((result & TEST_MODULE_TYPE_ERR) != 0) {
			i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "module type error\n");
			save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);
		}
		if ((result & TEST_VERSION_ERR) != 0) {
			i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "module version error\n");
			save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);
		}
		if ((result & TEST_GT_OPEN) != 0) {
			i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "ito channel open\n");
			save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);
		}
		if ((result & TEST_GT_SHORT) != 0) {
			i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "ito channel short\n");
			save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);
		}
	}

	return i_len;
}

int save_ito_array_data_to_file(struct synaptics_test_st *stp_test, char *title_info,
	void *data, int length, int size, int i_row, int i_col)
{
	int i_len = 0;
	int i = 0, j = 0;
	int i_value = 0;
	int i_rawdata_min = 65535, i_rawdata_max = 0, i_rawdata_count = 0;
	long i_rawdata_average = 0;
	u8 *p_u8_value = NULL;
	u16 *p_u16_vlaue = NULL;
	u32 *p_u32_vlaue = NULL;
	u64 *p_u64_vlaue = NULL;


	p_u8_value = (u8 *)data;
	p_u16_vlaue = (u16 *)data;
	p_u32_vlaue = (u32 *)data;
	p_u64_vlaue = (u64 *)data;

	if (stp_test->temp_buffer == NULL || stp_test->result_buffer == NULL) {
		TPD_TEST_DBG("warning:buffer is null, return");
		return -EINVAL;
	}

	i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "%s Col:%d Row:%d\n", title_info, i_row, i_col);
	save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);

	i_rawdata_min = 65535;
	i_rawdata_max = 0;
	i_rawdata_average = 0;
	i_rawdata_count = 0;
	/* Save Data */
	for (i = 0; i < i_row; i++) {
		for (j = 0; j < i_col; j++) {
			if (size == 4)
				i_value = p_u32_vlaue[i * i_col + j];
			else if (size == 2)
				i_value = p_u16_vlaue[i * i_col + j];
			else if (size == 1)
				i_value = p_u8_value[i * i_col + j];
			else if (size == 8)
				i_value = p_u64_vlaue[i * i_col + j];
			else
				i_value = p_u8_value[i * i_col + j];

			if (j == (i_col - 1))  /* The Last Data of the Row, add "\n"*/
				i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "%d,\n",  i_value);
			else
				i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "%d, ", i_value);

			save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);
			if (i_rawdata_min > i_value)
				i_rawdata_min = i_value;

			if (i_rawdata_max < i_value)
				i_rawdata_max = i_value;

			i_rawdata_average += i_value;
			i_rawdata_count++;
		}
	}
	if (i_rawdata_count == 0)
		return -EINVAL;
	i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, "Data min:%d max:%d average:%ld\n",
		i_rawdata_min, i_rawdata_max, i_rawdata_average / i_rawdata_count);
	save_string_to_buffer(stp_test, stp_test->temp_buffer, i_len);

	return 0;
}

void show_rawdata(struct synaptics_test_st *stp_test, int *buffer)
{
	int i_row = 0, i_col = 0;

	for (i_row = 0; i_row < stp_test->screen_cfg.i_txNum; i_row++) {
		for (i_col = 0; i_col < stp_test->screen_cfg.i_rxNum; i_col++)
			pr_notice("%-4d ", buffer[i_row * stp_test->screen_cfg.i_rxNum + i_col]);

		pr_notice("\n");
	}
}

int synaptics_full_rawdata_test(struct synaptics_test_st *stp_test, int times)
{
	int i_row = 0, i_col = 0;
	int i_min = 0, i_max = 0, i_value;
	bool b_result = true;
	int *array_rawdata;
	unsigned int full_raw_report_size = stp_test->screen_cfg.i_txNum * stp_test->screen_cfg.i_rxNum;
	int retval = -1;
	int i_len = 0;

	TPD_TEST_DBG("");

	array_rawdata = (int *)tpd_malloc(sizeof(int) * full_raw_report_size);
	if (!array_rawdata) {
		TPD_TEST_DBG("error for malloc rawdata buffer");
		retval = -1;
		goto out;
	}
	tpd_synaptics_get_full_rawdata(array_rawdata, full_raw_report_size);
	show_rawdata(stp_test, array_rawdata);

	/* To Determine RawData if in Range or not */
	for (i_row = 0; i_row < stp_test->screen_cfg.i_txNum; i_row++) {
		for (i_col = 0; i_col < stp_test->screen_cfg.i_rxNum; i_col++) {
			if (stp_test->detail_threshold.invalid_node[i_row][i_col] == 0)
				continue; /* Invalid Node */
			i_min = stp_test->detail_threshold.rawdata_test_min[i_row][i_col];
			i_max = stp_test->detail_threshold.rawdata_test_max[i_row][i_col];
			i_value = array_rawdata[i_row * stp_test->screen_cfg.i_rxNum  + i_col];

			if (i_value < i_min) {
				b_result = false;
				stp_test->test_result |= TEST_BEYOND_MIN_LIMIT;
				i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, ",%d,%d", i_row, i_col);
				save_rawdata_test_result(stp_test, stp_test->temp_buffer, i_len);
				stp_test->rawdata_failed_count++;
				TPD_TEST_DBG("rawdata test failure. Node=(%d,  %d),"
					"Get_value=%d, Set_Range=(%d, %d) ",
					i_row + 1, i_col + 1, i_value, i_min, i_max);
			} else if (i_value > i_max) {
				b_result = false;
				stp_test->test_result |= TEST_BEYOND_MAX_LIMIT;
				i_len = snprintf(stp_test->temp_buffer, TEST_TEMP_LENGTH, ",%d,%d", i_row, i_col);
				save_rawdata_test_result(stp_test, stp_test->temp_buffer, i_len);
				stp_test->rawdata_failed_count++;
				TPD_TEST_DBG("rawdata test failure. Node=(%d,  %d),"
					"Get_value=%d,  Set_Range=(%d, %d) ",
					i_row + 1, i_col + 1, i_value, i_min, i_max);
			}
		}
	}

	/* Save Test Data */
	save_ito_array_data_to_file(stp_test, "rawdata test", array_rawdata, full_raw_report_size,
		sizeof(int), stp_test->screen_cfg.i_txNum, stp_test->screen_cfg.i_rxNum);

	if (array_rawdata != NULL)
		tpd_free(array_rawdata);

	retval = 0;
out:
	return retval;
}

int synaptics_tddi_test_start(struct synaptics_test_st *stp_test)
{
	TPD_TEST_DBG("++++++++++++++++++++++++++++++++");

	stp_test->test_result = 0;

	if (stp_test->test_item.rawdata_test) {
		TPD_TEST_DBG("-------------rawdata_test--------------");
		synaptics_full_rawdata_test(stp_test, 3);
	}
	if (stp_test->test_item.eeshort_test) {
		TPD_TEST_DBG("-------------eeshort_test--------------");
		tpd_synaptics_ee_short_test(stp_test);
	}
	if (stp_test->test_item.accort_test)
		TPD_TEST_DBG("-------------accort_test--------------");

	if (stp_test->test_item.noise_test)
		TPD_TEST_DBG("-------------noise_test--------------");

	if (stp_test->test_item.amp_open_test) {
		TPD_TEST_DBG("-------------amp_open_test--------------");
		tpd_synaptics_amp_open_test(stp_test);
	}

	if (!stp_test->test_item.amp_open_test)
	  tpd_synaptics_test_reset(stp_test);

	save_test_result(stp_test);

	return 0;
}

