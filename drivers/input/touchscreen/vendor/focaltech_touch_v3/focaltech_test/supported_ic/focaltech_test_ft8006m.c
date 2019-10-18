/************************************************************************
* Copyright (C) 2012-2019, Focaltech Systems (R), All Rights Reserved.
*
* File Name: focaltech_test_ft8006m.c
*
* Author: Focaltech Driver Team
*
* Created: 2017-08-08
*
* Abstract:
*
************************************************************************/

/*******************************************************************************
* Included header files
*******************************************************************************/
#include "../focaltech_test.h"
#if (FTS_CHIP_TYPE == _FT8006M)
/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define MAX_ADC_VALUE                   4015
#define FACTORY_NOISE_MODE_REG          0x5E
/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/
#define CODE2_SHORT_TEST                14
#define CODE2_LCD_NOISE_TEST            15

/*******************************************************************************
* Static variables
*******************************************************************************/

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/

/*******************************************************************************
* Static function prototypes
*******************************************************************************/
static int ft8006m_short_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = true;
	int byte_num = 0;
	int ch_num = 0;
	int *adcdata = NULL;
	int tmp_adc = 0;
	int i = 0;
	struct incell_threshold *thr = &tdata->ic.incell.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: short test\n");
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

	/* change adc to resistance */
	for (i = 0; i < tdata->node.node_num; ++i) {
		tmp_adc = adcdata[i];
		/* avoid calculating the value of the resistance is too large, limiting the size of the ADC value */
		if (tmp_adc > MAX_ADC_VALUE)
			tmp_adc = MAX_ADC_VALUE;
		adcdata[i] = (tmp_adc * 100) / (4095 - tmp_adc);
	}

	/* save */
	show_data(adcdata, true);

	/* compare */
	tmp_result =
	    compare_data(adcdata, thr->basic.short_res_min, TEST_SHORT_RES_MAX, thr->basic.short_res_vk_min,
			 TEST_SHORT_RES_MAX, true);

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
	fts_test_save_data("Short Circuit Test", CODE2_SHORT_TEST, adcdata, 0, false, true, *test_result);
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft8006m_open_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	u8 reg20_val = 0;
	u8 reg86_val = 0;
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

	ret = fts_test_read_reg(FACTORY_REG_OPEN_REG86, &reg86_val);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read 0x86 fail\n");
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_OPEN_REG20, &reg20_val);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read 0x20 fail\n");
		goto test_err;
	}

	/* set open mode */
	ret = fts_test_write_reg(FACTORY_REG_OPEN_REG86, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 0x86 fail\n");
		goto restore_reg;
	}

	ret = fts_test_write_reg(FACTORY_REG_OPEN_REG20, ((reg20_val & 0xCF) + 0x20));
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 0x86 fail\n");
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
	ret = fts_test_write_reg(FACTORY_REG_OPEN_REG86, reg86_val);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore reg86 fail\n");
	}

	ret = fts_test_write_reg(FACTORY_REG_OPEN_REG20, reg20_val);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore reg20 fail\n");
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

	/* save data */
	fts_test_save_data("Open Test", CODE_OPEN_TEST, opendata, 0, false, false, *test_result);
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft8006m_cb_test(struct fts_test *tdata, bool *test_result)
{
	bool tmp_result = false;
	int ret = 0;
	int i = 0;
	bool key_check = false;
	int byte_num = 0;
	int *cbdata = NULL;
	struct incell_threshold *thr = &tdata->ic.incell.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: CB Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	cbdata = tdata->buffer;

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

	/* clb */
	for (i = 0; i < 10; i++) {
		ret = chip_clb();
		if (ret == 0) {
			FTS_TEST_SAVE_INFO("clb succeed\n");
			break;
		}
	}
	if (i == 10) {
		FTS_TEST_SAVE_ERR("clb failed\n");
		goto test_err;
	}

	byte_num = tdata->node.node_num;
	ret = get_cb_incell(0, byte_num, cbdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get cb fail\n");
		goto test_err;
	}

	key_check = thr->basic.cb_vkey_check;

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

	/*save test data */
	fts_test_save_data("CB Test", CODE_CB_TEST, cbdata, 0, false, key_check, *test_result);
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft8006m_rawdata_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int i = 0;
	bool key_check = true;
	int *rawdata = NULL;
	struct incell_threshold *thr = &tdata->ic.incell.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: RawData Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	rawdata = tdata->buffer;

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
	tmp_result = compare_array(rawdata, thr->rawdata_min, thr->rawdata_max, key_check);

test_err:
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n------ RawData Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("\n------ RawData Test NG\n");
	}

	/*save test data */
	fts_test_save_data("RawData Test", CODE_RAWDATA_TEST, rawdata, 0, false, key_check, *test_result);
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft8006m_lcdnoise_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int frame_num = 0;
	int i = 0;
	int max = 0;
	int max_vk = 0;
	int byte_num = 0;
	u8 old_mode = 0;
	u8 reg_value = 0;
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

	ret = fts_test_read_reg(FACTORY_NOISE_MODE_REG, &reg_value);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read reg5e fail\n");
		goto test_err;
	}

	ret = fts_test_write_reg(FACTORY_REG_DATA_SELECT, 0x64);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 0x64 to reg5e fail\n");
		goto restore_reg;
	}

	ret = fts_test_write_reg(FACTORY_REG_DATA_SELECT, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 1 to reg06 fail\n");
		goto restore_reg;
	}

	frame_num = thr->basic.lcdnoise_frame;
	ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_FRAME, frame_num & 0xff);
	if (ret < 0) {
		FTS_TEST_SAVE_INFO("write frame num fail\n");
		goto restore_reg;
	}
	ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_FRAME + 1, (frame_num >> 8) & 0xff);
	if (ret < 0) {
		FTS_TEST_SAVE_INFO("write frame num fail\n");
		goto restore_reg;
	}

	/* read noise data */
	ret = fts_test_write_reg(FACTORY_REG_LINE_ADDR, 0xAD);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 0xAD to reg01 fail\n");
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
		ret = fts_test_read_reg(FACTORY_REG_LCD_NOISE_START, &status);
		if ((ret >= 0) && (status == 0x00)) {
			break;
		}
		FTS_TEST_DBG("reg%x=%x,retry:%d\n", FACTORY_REG_LCD_NOISE_START, status, i);
		sys_delay(FACTORY_TEST_RETRY_DELAY);
	}
	if (i >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("lcdnoise test timeout\n");
		ret = -ENODATA;
		goto restore_reg;
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
	ret = fts_test_write_reg(FACTORY_REG_DATA_SELECT, old_mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore reg06 fail\n");
	}
	ret = fts_test_write_reg(FACTORY_NOISE_MODE_REG, reg_value);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore reg5e fail\n");
	}
	ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_START, 0x00);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore reg11 fail\n");
	}

test_err:
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n------ LCD Noise Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("\n------ LCD Noise Test NG\n");
	}

	/*save test data */
	fts_test_save_data("LCD Noise Test", CODE2_LCD_NOISE_TEST, lcdnoise, 0, false, true, *test_result);
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int start_test_ft8006m(void)
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
		ret = ft8006m_short_test(tdata, &temp_result);
		if ((ret < 0) || (temp_result == false)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_GT_SHORT;
		}
	}

	/* open test */
	if (test_item->open_test == true) {
		ret = ft8006m_open_test(tdata, &temp_result);
		if ((ret < 0) || (temp_result == false)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_GT_OPEN;
		}
	}

	/* cb test */
	if (test_item->cb_test == true) {
		ret = ft8006m_cb_test(tdata, &temp_result);
		if ((ret < 0) || (temp_result == false)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		}
	}

	/* rawdata test */
	if (test_item->rawdata_test == true) {
		ret = ft8006m_rawdata_test(tdata, &temp_result);
		if ((ret < 0) || (temp_result == false)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		}
	}

	/* lcd noise test */
	if (test_item->lcdnoise_test == true) {
		ret = ft8006m_lcdnoise_test(tdata, &temp_result);
		if ((ret < 0) || (temp_result == false)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		}
	}

	return test_result;
}

struct test_funcs test_func_ft8006m = {
	.ctype = {0x07},
	.hwtype = IC_HW_INCELL,
	.key_num_total = 6,
	.start_test = start_test_ft8006m,
};
#endif
