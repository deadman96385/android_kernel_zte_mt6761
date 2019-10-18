/************************************************************************
* Copyright (C) 2012-2019, Focaltech Systems (R), All Rights Reserved.
*
* File Name: Focaltech_test_ft6216.c
*
* Author: Focaltech Driver Team
*
* Created: 2015-10-08
*
* Abstract:
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "../focaltech_test.h"
#if (FTS_CHIP_TEST_TYPE == FT6216_TEST)
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/

/*******************************************************************************
* Static variables
*******************************************************************************/
static int *tmpcb = NULL;

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/

/*******************************************************************************
* Static function prototypes
*******************************************************************************/
static int get_cb_ft6216(u8 fmode, int byte_num, int *cb)
{
	int ret = 0;

	ret = fts_test_write_reg(FACTORY_REG_FMODE, fmode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write factory mode reg fail\n");
		return ret;
	}

	ret = start_scan();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("start scan fail\n");
		return ret;
	}

	/* get cb */
	ret = get_cb_sc(byte_num, cb, DATA_TWO_BYTE);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get cb fail\n");
		return ret;
	}

	return 0;
}

static void init_dcb_ft6216(int *delta_cb, struct sc_threshold *thr, struct dcb_sort_d *sort)
{
	int i = 0;

	if (!delta_cb || !thr || !sort) {
		FTS_TEST_SAVE_ERR("delta_cb/thr/sort is null\n");
		return;
	}

	/* init sort */
	for (i = 0; i < DCB_SORT_MAX - 1; i++) {
		sort[i].ch_num = 0;
		sort[i].min = delta_cb[0];
		sort[i].max = delta_cb[0];
	}
	sort[0].deviation = thr->basic.dcb_ds1;
	sort[0].critical = thr->basic.dcb_cs1;
	sort[1].deviation = thr->basic.dcb_ds2;
	sort[1].critical = thr->basic.dcb_cs2;
	sort[2].deviation = thr->basic.dcb_ds3;
	sort[2].critical = thr->basic.dcb_cs3;
	sort[3].deviation = thr->basic.dcb_ds4;
	sort[3].critical = thr->basic.dcb_cs4;
	sort[4].deviation = thr->basic.dcb_ds5;
	sort[4].critical = thr->basic.dcb_cs5;
	sort[5].deviation = thr->basic.dcb_ds6;
	sort[5].critical = thr->basic.dcb_cs6;
}

static int ft6216_cb_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	bool key_check = true;
	u8 f_mode = 0;
	u8 cb_sel = 0;
	int byte_num = 0;
	int *cbdata = NULL;
	struct sc_threshold *thr = &tdata->ic.sc.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: CB Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	cbdata = tdata->buffer;
	byte_num = tdata->node.node_num * 2;
	key_check = (tdata->node.key_num == 0) ? false : true;

	if (!thr->cb_min || !thr->cb_max || !test_result) {
		FTS_TEST_SAVE_ERR("cb_min/max test_result is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_FMODE, &f_mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read factory mode reg fail\n");
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_CB_SEL, &cb_sel);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read cb sel reg fail\n");
		goto test_err;
	}

	ret = fts_test_write_reg(FACTORY_REG_CB_SEL, 0x00);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write cb sel reg fail\n");
		goto restore_reg;
	}

	/* get cb in testmode 2 */
	ret = get_cb_ft6216(FACTORY_NORMAL, byte_num, cbdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get cb fail\n");
		goto restore_reg;
	}

	/* get cb in testmode 0 */
	ret = get_cb_ft6216(FACTORY_TESTMODE_2, byte_num, tmpcb);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get cb fail\n");
		goto restore_reg;
	}

	/* save */
	show_data(cbdata, key_check);

	/* compare */
	tmp_result = compare_array(cbdata, thr->cb_min, thr->cb_max, key_check);
restore_reg:
	ret = fts_test_write_reg(FACTORY_REG_FMODE, f_mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore factory mode reg fail\n");
	}

	ret = fts_test_write_reg(FACTORY_REG_CB_SEL, cb_sel);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore cb sel reg fail\n");
	}
test_err:
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ CB Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("------ CB Test NG\n");
	}

	/* save test data */
	fts_test_save_data("CB Test", CODE_S_CB_TEST, cbdata, 0, false, key_check, *test_result);

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft6216_delta_cb_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int i = 0;
	bool tmp_result = false;
	bool key_check = true;
	int diff_max = 0;
	int key_diff_max = 0;
	int sort_num = 0;
	int tmp_value = 0;
	int *cb_0 = NULL;
	int *cb_2 = NULL;
	int *delta_cb = NULL;
	struct sc_threshold *thr = &tdata->ic.sc.thr;
	struct dcb_sort_d sort[DCB_SORT_MAX];

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Delta CB Test\n");
	cb_2 = tmpcb;
	key_check = thr->basic.dcb_key_check && tdata->node.key_num;

	if (!thr->dcb_base || !thr->dcb_sort || !test_result) {
		FTS_TEST_SAVE_ERR("dcb_base/dcb_sort/test_result is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	/* get differ */
	cb_0 = fts_malloc(tdata->node.node_num * sizeof(int));
	if (cb_0 == NULL) {
		FTS_TEST_SAVE_ERR("malloc cb_0 memory fail\n");
		ret = -ENOMEM;
		goto test_err;
	}
	memcpy(cb_0, tdata->buffer, tdata->node.node_num * sizeof(int));
	memset(tdata->buffer, 0, tdata->buffer_length);
	delta_cb = tdata->buffer;

	for (i = 0; i < tdata->node.node_num; i++) {
		delta_cb[i] = focal_abs(cb_2[i] - cb_0[i] - thr->dcb_base[i]);
	}

	FTS_TEST_DBG("cb in mode2:\n");
	print_buffer(cb_2, tdata->node.node_num, tdata->node.rx_num);
	FTS_TEST_DBG("cb in mode0:\n");
	print_buffer(cb_0, tdata->node.node_num, tdata->node.rx_num);
	FTS_TEST_DBG("delta_cb:\n");
	print_buffer(delta_cb, tdata->node.node_num, tdata->node.rx_num);

	init_dcb_ft6216(delta_cb, thr, sort);

	/* get differ max & sort max/min */
	diff_max = delta_cb[0];
	for (i = 0; i < tdata->node.channel_num; i++) {
		if (diff_max < delta_cb[i]) {
			diff_max = delta_cb[i];
		}

		sort_num = thr->dcb_sort[i];
		if ((sort_num >= DCB_SORT_MIN) && (sort_num <= DCB_SORT_MAX)) {
			if (sort[sort_num - 1].min > delta_cb[i]) {
				sort[sort_num - 1].min = delta_cb[i];
			} else if (sort[sort_num - 1].max < delta_cb[i]) {
				sort[sort_num - 1].max = delta_cb[i];
			}
			sort[sort_num - 1].ch_num++;
		}
	}

	/* get key max differ */
	if (key_check) {
		key_diff_max = delta_cb[tdata->node.channel_num];
		for (i = tdata->node.channel_num; i < tdata->node.node_num; i++) {
			if (key_diff_max < delta_cb[i]) {
				key_diff_max = delta_cb[i];
			}

		}
	}

	FTS_TEST_DBG("differ max:%d,key differ max:%d\n", diff_max, key_diff_max);
	for (i = 0; i < DCB_SORT_MAX - 1; i++) {
		FTS_TEST_DBG("sort(%d) data:\n", i + 1);
		FTS_TEST_DBG("ch_num:%d,deviation:%d,critical:%d,min:%d,max:%d\n",
		       sort[i].ch_num, sort[i].deviation, sort[i].critical, sort[i].min, sort[i].max);
	}

	/* save */
	show_data(delta_cb, key_check);

	/* compare */
	tmp_result = true;
	if (diff_max > thr->basic.dcb_differ_max) {
		FTS_TEST_SAVE_ERR("test fail,differ max:%d,range:(0,%d)\n", diff_max, thr->basic.dcb_differ_max);
		tmp_result = false;
	}

	if (key_check && (key_diff_max > thr->basic.dcb_key_differ_max)) {
		FTS_TEST_SAVE_ERR("test fail,key differ max:%d,range:(0,%d)\n",
				  key_diff_max, thr->basic.dcb_key_differ_max);
		tmp_result = false;
	}

	for (i = 0; i < DCB_SORT_MAX - 1; i++) {
		if (sort[i].ch_num > 0) {
			tmp_value = sort[i].max - sort[i].min;
			if (tmp_value > sort[i].deviation) {
				if (thr->basic.dcb_critical_check) {
					/* double check */
					if (tmp_value > sort[i].critical) {
						FTS_TEST_SAVE_ERR("test fail,sort(%d),critical:%d,range:(0,%d)\n",
								  i + 1, tmp_value, sort[i].critical);
						tmp_result = false;
					}
				} else {
					/* no double check */
					FTS_TEST_SAVE_ERR("test fail,sort(%d),deviation:%d,range:(0,%d)\n",
							  i + 1, tmp_value, sort[i].deviation);
					tmp_result = false;
				}
			}
		}
	}

test_err:
	fts_free(cb_0);

	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ Delta CB Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("------ Delta CB Test NG\n");
	}

	/* save test data */
	fts_test_save_data("Delta CB Test", CODE_S_DCB_TEST, delta_cb, 0, false, key_check, *test_result);

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft6216_rawdata_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	bool key_check = true;
	u8 f_mode = 0;
	int *rawdata = NULL;
	struct sc_threshold *thr = &tdata->ic.sc.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: RawData Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	rawdata = tdata->buffer;
	key_check = (tdata->node.key_num == 0) ? false : true;

	if (!thr->rawdata_min || !thr->rawdata_max || !test_result) {
		FTS_TEST_SAVE_ERR("rawdata_min/max test_result is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_FMODE, &f_mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read factory mode reg fail\n");
		goto test_err;
	}

	ret = fts_test_write_reg(FACTORY_REG_FMODE, FACTORY_NORMAL);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write factory mode reg fail\n");
		goto restore_reg;
	}

	ret = get_rawdata(rawdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get RawData fail,ret=%d\n", ret);
		goto restore_reg;
	}

	/* show data */
	show_data(rawdata, key_check);

	/* compare */
	tmp_result = compare_array(rawdata, thr->rawdata_min, thr->rawdata_max, key_check);
restore_reg:
	ret = fts_test_write_reg(FACTORY_REG_FMODE, f_mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore factory mode reg fail\n");
	}
test_err:
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ RawData Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("------ RawData Test NG\n");
	}

	/* save test data */
	fts_test_save_data("RawData Test", CODE_S_RAWDATA_TEST, rawdata, 0, false, key_check, *test_result);

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int start_test_ft6216(void)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;
	struct sc_testitem *test_item = &tdata->ic.sc.u.item;
	bool temp_result = false;
	bool test_result = true;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_INFO("test item:0x%x", fts_ftest->ic.sc.u.tmp);

	if (!tdata || !tdata->buffer) {
		FTS_TEST_ERROR("tdata is null");
		return -EINVAL;
	}

	tmpcb = (int *)fts_malloc(tdata->node.node_num * sizeof(int));
	if (tmpcb == NULL) {
		FTS_TEST_SAVE_ERR("malloc tmpcb memory fail\n");
		return -ENOMEM;
	}
	memset(tmpcb, 0, tdata->node.node_num * sizeof(int));

	/* cb test */
	if (test_item->cb_test == true) {
		ret = ft6216_cb_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		}
	}

	/* cb test */
	if (test_item->delta_cb_test == true) {
		ret = ft6216_delta_cb_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		}
	}

	/* rawdata test */
	if (test_item->rawdata_test == true) {
		ret = ft6216_rawdata_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		}
	}

	fts_free(tmpcb);
	return test_result;
}

struct test_funcs test_func_ft6216 = {
	.ctype = {0x84},
	.startscan_mode = SCAN_SC,
	.hwtype = IC_HW_SC,
	.key_num_total = 4,
	.start_test = start_test_ft6216,
};
#endif
