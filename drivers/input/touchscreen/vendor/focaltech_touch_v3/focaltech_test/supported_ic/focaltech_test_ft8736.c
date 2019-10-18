/************************************************************************
* Copyright (C) 2012-2019, Focaltech Systems (R), All Rights Reserved.
*
* File Name: focaltech_test_ft8716.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-01
*
* Abstract:
*
************************************************************************/

/*******************************************************************************
* Included header files
*******************************************************************************/
#include "../focaltech_test.h"
#if (FTS_CHIP_TYPE == _FT8736)
/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define CODE2_SHORT_TEST                14
#define CODE2_OPEN_TEST                 15
#define CODE2_LCD_NOISE_TEST            19

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/

/*******************************************************************************
* Static variables
*******************************************************************************/

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/

/*******************************************************************************
* Static function prototypes
*******************************************************************************/
static int ft8736_short_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int byte_num = 0;
	int ch_num = 0;
	int *adcdata = NULL;
	struct incell_threshold *thr = &tdata->ic.incell.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Short Circuit Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	adcdata = tdata->buffer;

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	byte_num = tdata->node.node_num * 2;
	ch_num = (tdata->node.tx_num + tdata->node.rx_num);
	ret = short_get_adcdata_incell(TEST_RETVAL_00, ch_num, byte_num, adcdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get adc data fail\n");
		goto test_err;
	}

	/* save */
	show_data(adcdata, false);

	/* compare */
	tmp_result = compare_data(adcdata, 0, thr->basic.short_res_min, 0, 0, false);

	ret = 0;
test_err:
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n------ Short Circuit Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("\n------ Short Circuit Test NG\n");
	}

	/*save test data */
	fts_test_save_data("Short Circuit Test", CODE2_SHORT_TEST, adcdata, 0, false, false, *test_result);
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft8736_open_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	u8 reg22_val = 0;
	int min = 0;
	int max = 0;
	int *opendata = NULL;
	int byte_num = 0;
	struct incell_threshold *thr = &tdata->ic.incell.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Open Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	opendata = tdata->buffer;

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_OPEN_REG22, &reg22_val);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read 0x22 fail\n");
		goto test_err;
	}

	/* set open mode */
	ret = fts_test_write_reg(FACTORY_REG_OPEN_REG22, 0x80);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 0x22 fail\n");
		goto restore_reg;
	}

	ret = wait_state_update(TEST_RETVAL_00);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("wait state update fail\n");
		goto restore_reg;
	}

	/* clb and read cb */
	ret = chip_clb();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("auto clb fail\n");
		goto restore_reg;
	}

	byte_num = tdata->node.tx_num * tdata->node.rx_num;
	ret = get_cb_incell(0, byte_num, opendata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get cb fail\n");
		goto restore_reg;
	}

	/* show open data */
	show_data(opendata, false);

	/* compare */
	min = thr->basic.open_cb_min;
	max = 256;
	FTS_TEST_DBG("open %d %d\n", min, opendata[0]);
	tmp_result = compare_data(opendata, min, max, 0, 0, false);

	ret = 0;
restore_reg:
	/* restore */
	ret = fts_test_write_reg(FACTORY_REG_OPEN_REG22, reg22_val);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore reg22 fail\n");
	}

	ret = wait_state_update(TEST_RETVAL_00);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("wait state update fail\n");
	}

	ret = chip_clb();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("auto clb fail\n");
	}

test_err:
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n------ Open Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("\n------ Open Test NG\n");
	}

	/* save test data */
	fts_test_save_data("Open Test", CODE2_OPEN_TEST, opendata, 0, false, false, *test_result);

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft8736_cb_test(struct fts_test *tdata, bool *test_result)
{
	bool tmp_result = false;
	int ret = 0;
	int i = 0;
	bool key_check = false;
	u8 keycb_width = 0;
	int key_pos = 0;
	int byte_num = 0;
	int *cbdata = NULL;
	struct incell_threshold *thr = &tdata->ic.incell.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: CB Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	cbdata = tdata->buffer;
	key_check = thr->basic.cb_vkey_check;

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

	ret = fts_test_read_reg(FACTORY_REG_KEY_CBWIDTH, &keycb_width);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read reg0b fail\n");
		goto test_err;
	}

	/* clb */
	ret = chip_clb();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("clb fail\n");
		goto test_err;
	}

	byte_num = tdata->node.node_num;
	if (keycb_width == 0x01) {
		/* 2bytes of key's cb */
		byte_num += tdata->node.key_num;
	}

	ret = get_cb_incell(0, byte_num, cbdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get cb fail\n");
		goto test_err;
	}

	if (keycb_width != 0) {
		key_pos = tdata->node.tx_num * tdata->node.rx_num;
		for (i = 0; i < tdata->node.key_num; i++) {
			cbdata[key_pos + i] = (cbdata[key_pos + i * 2] << 8)
			    + cbdata[key_pos + i * 2 + 1];
		}
	}

	/* save */
	show_data(cbdata, key_check);

	/* compare */
	tmp_result = compare_array(cbdata, thr->cb_min, thr->cb_max, key_check);

	ret = 0;
test_err:
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n------ CB Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("\n------ CB Test NG\n");
	}

	/* save test data */
	fts_test_save_data("CB Test", CODE_CB_TEST, cbdata, 0, false, key_check, *test_result);

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft8736_rawdata_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int i = 0;
	bool key_check = false;
	int *rawdata = NULL;
	struct incell_threshold *thr = &tdata->ic.incell.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: RawData Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	rawdata = tdata->buffer;
	key_check = thr->basic.rawdata_vkey_check;

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

	/* read rawdata */
	for (i = 0; i < 3; i++) {
		ret = get_rawdata(rawdata);
	}
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get RawData fail,ret=%d\n", ret);
		goto test_err;
	}

	/* save */
	show_data(rawdata, key_check);

	/* compare */
	key_check = thr->basic.rawdata_vkey_check;
	tmp_result = compare_array(rawdata, thr->rawdata_min, thr->rawdata_max, key_check);

test_err:
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n------ RawData Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("\n------ RawData Test NG\n");
	}

	/* save test data */
	fts_test_save_data("RawData Test", CODE_RAWDATA_TEST, rawdata, 0, false, key_check, *test_result);
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft8736_lcdnoise_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int frame_num = 0;
	int i = 0;
	int max = 0;
	int max_vk = 0;
	int byte_num = 0;
	u8 old_mode = 0;
	u8 status = 0;
	int *lcdnoise = NULL;
	struct incell_threshold *thr = &tdata->ic.incell.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: LCD Noise Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	lcdnoise = tdata->buffer;

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_DATA_SELECT, &old_mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read reg06 fail\n");
		goto test_err;
	}

	ret = fts_test_write_reg(FACTORY_REG_DATA_SELECT, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 1 to reg06 fail\n");
		goto restore_reg;
	}

	frame_num = thr->basic.lcdnoise_frame;
	ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_FRAME, frame_num / 4);
	if (ret < 0) {
		FTS_TEST_SAVE_INFO("write frame num fail\n");
		goto restore_reg;
	}

	/* start test */
	ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_START, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_INFO("start lcdnoise test fail\n");
		goto restore_reg;
	}
	sys_delay(frame_num * FACTORY_TEST_DELAY / 2);
	for (i = 0; i < FACTORY_TEST_RETRY; i++) {
		status = 0xFF;
		ret = fts_test_read_reg(DEVIDE_MODE_ADDR, &status);
		if ((ret >= 0) && (status == 0x40)) {
			break;
		}
		FTS_TEST_DBG("reg%x=%x,retry:%d\n", DEVIDE_MODE_ADDR, status, i);
		sys_delay(FACTORY_TEST_RETRY_DELAY);
	}
	if (i >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("lcdnoise test timeout\n");
		ret = -ENODATA;
		goto restore_reg;
	}

	/* read noise data */
	ret = fts_test_write_reg(FACTORY_REG_LINE_ADDR, 0xAD);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 0xAD to reg01 fail\n");
		goto test_err;
	}

	byte_num = tdata->node.node_num * 2;
	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR, byte_num, lcdnoise);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read rawdata fail\n");
		goto restore_reg;
	}

	/* save */
	show_data(lcdnoise, true);

	/* compare */
	max = thr->basic.lcdnoise_coefficient * tdata->va_touch_thr * 32 / 100;
	max_vk = thr->basic.lcdnoise_coefficient_vkey * tdata->vk_touch_thr * 32 / 100;
	tmp_result = compare_data(lcdnoise, 0, max, 0, max_vk, true);

	ret = 0;
restore_reg:
	ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_START, 0x00);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore reg11 fail\n");
	}

	ret = fts_test_write_reg(FACTORY_REG_DATA_SELECT, old_mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore reg06 fail\n");
	}
test_err:
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n------ LCD Noise Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("\n------ LCD Noise Test NG\n");
	}

	/* save test data */
	fts_test_save_data("LCD Noise Test", CODE2_LCD_NOISE_TEST, lcdnoise, 0, false, true, *test_result);

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int start_test_ft8736(void)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;
	struct incell_testitem *test_item = &tdata->ic.incell.u.item;
	bool temp_result = false;
	bool test_result = true;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_INFO("test item:0x%x", fts_ftest->ic.incell.u.tmp);

	if (!tdata || !tdata->buffer) {
		FTS_TEST_ERROR("tdata is null");
		return -EINVAL;
	}

	/* short test */
	if (test_item->short_test == true) {
		ret = ft8736_short_test(tdata, &temp_result);
		if ((ret < 0) || (temp_result == false)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_GT_SHORT;
		}
	}

	/* open test */
	if (test_item->open_test == true) {
		ret = ft8736_open_test(tdata, &temp_result);
		if ((ret < 0) || (temp_result == false)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_GT_OPEN;
		}
	}

	/* cb test */
	if (test_item->cb_test == true) {
		ret = ft8736_cb_test(tdata, &temp_result);
		if ((ret < 0) || (temp_result == false)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		}
	}

	/* rawdata test */
	if (test_item->rawdata_test == true) {
		ret = ft8736_rawdata_test(tdata, &temp_result);
		if ((ret < 0) || (temp_result == false)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		}
	}

	/* lcd noise test */
	if (test_item->lcdnoise_test == true) {
		ret = ft8736_lcdnoise_test(tdata, &temp_result);
		if ((ret < 0) || (temp_result == false)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		}
	}

	return test_result;
}

static int param_init_ft8736(void)
{
	struct incell_threshold *thr = &fts_ftest->ic.incell.thr;

	get_value_basic("LCDNoiseTest_FrameNum", &thr->basic.lcdnoise_frame);
	get_value_basic("LCDNoiseTest_Coefficient", &thr->basic.lcdnoise_coefficient);
	get_value_basic("LCDNoiseTest_Coefficient_Key", &thr->basic.lcdnoise_coefficient_vkey);

	return 0;
}

struct test_funcs test_func_ft8736 = {
	.ctype = {0x06},
	.hwtype = IC_HW_INCELL,
	.key_num_total = 4,
	.param_init = param_init_ft8736,
	.start_test = start_test_ft8736,
};
#endif
