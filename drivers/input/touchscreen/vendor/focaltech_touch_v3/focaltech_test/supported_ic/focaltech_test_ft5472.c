/************************************************************************
* Copyright (C) 2012-2019, Focaltech Systems (R), All Rights Reserved.
*
* File Name: Focaltech_test_ft5452.c
*
* Author: Focaltech Driver Team
*
* Created: 2018-03-08
*
* Abstract:
*
************************************************************************/

/*****************************************************************************
* included header files
*****************************************************************************/
#include "../focaltech_test.h"
#if (FTS_CHIP_TEST_TYPE == FT5472_TEST)
/*****************************************************************************
* private constant and macro definitions using #define
*****************************************************************************/
#define SHORT_RESISTOR_ACCURACY             300

/*****************************************************************************
* private enumerations, structures and unions using typedef
*****************************************************************************/

/*****************************************************************************
* static variables
*****************************************************************************/

/*****************************************************************************
* global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* static function prototypes
*****************************************************************************/
static int short_get_resistor(int byte_num, int *r_buf, u8 mode, bool is_300)
{
	int ret = 0;
	int i = 0;
	int c1 = 0;
	int tmp1 = 0;
	int tmp2 = 0;
	int ch_num = byte_num / 2;
	struct fts_test *tdata = fts_ftest;

	/* get adc */
	ret = short_get_adc_data_mc(TEST_RETVAL_AA, byte_num, r_buf, mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get weak short(%d) data fail,ret:%d\n", mode, ret);
		return ret;
	}

	if ((mode == FACTROY_REG_SHORT_CA) && (false == is_300)) {
		show_data_mc_sc(r_buf);
		FTS_TEST_SAVE_INFO("offset:%d,code1:%d\n", tdata->offset, tdata->code1);
	}

	/* calculate resistance */
	if (is_300) {
		/* resistor < 300 */
		c1 = 41;
	} else {
		c1 = 1517;
	}

	if (ch_num > tdata->sc_node.channel_num) {
		ch_num = tdata->sc_node.channel_num;
	}
	for (i = 0; i < ch_num; i++) {
		tmp1 = (r_buf[i] - tdata->offset + 600) * c1;
		tmp2 = (tdata->code1 - r_buf[i]) * 10;
		r_buf[i] = tmp1 / tmp2 - 3;
	}

	FTS_TEST_DBG("resistor data:\n");
	print_buffer(r_buf, ch_num, 0);
	return 0;
}

/* get short resistor of abnormal channel */
static int short_get_ab_ch(u8 *ab_ch, u8 delay, u8 mode, bool is_300, int *r_buf)
{
	int ret = 0;
	int ab_num = 0;
	int byte_num = 0;

	/* write abnormal channel */
	ab_num = ab_ch[0];
	ret = fts_test_write(FACTROY_REG_SHORT_AB_CH, ab_ch, ab_num + 1);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write abnormal channel fail\n");
		return ret;
	}

	/* set delay to 0 */
	ret = fts_test_write_reg(FACTROY_REG_SHORT_DELAY, delay);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write shot delay fail\n");
		return ret;
	}

	/* get adc data */
	byte_num = ab_num * 2;
	ret = short_get_resistor(byte_num, r_buf, mode, is_300);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get short resistor(%d) fail\n", mode);
		return ret;
	}

	return 0;
}

/* compare, get resistor again, and save */
static int short_compare_get_save(struct fts_test *tdata, int *r_buf, u8 mode, u8 *ab_ch, bool *result)
{
	int ret = 0;
	int i = 0;
	bool tmp_result = true;
	int ab_pos = 0;
	int ab_300_pos = 0;
	int r_300[TX_NUM_MAX + RX_NUM_MAX] = { 0 };
	u8 ab_ch_300[TX_NUM_MAX + RX_NUM_MAX] = { 0 };
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;
	int short_res_min = 0;
	int tx = tdata->sc_node.tx_num;

	if (!tdata || !r_buf || !ab_ch || !result) {
		FTS_TEST_SAVE_ERR("tdata/r_buf/ab_ch/result is null\n");
		return -EINVAL;
	}

	if (mode == FACTROY_REG_SHORT_CA) {
		short_res_min = thr->basic.short_cc;
	} else {
		short_res_min = thr->basic.short_cg;
	}
	ab_pos = 1;
	ab_300_pos = 1;
	memset(ab_ch, 0, tdata->sc_node.channel_num + 1);
	for (i = 0; i < tdata->sc_node.channel_num; i++) {
		if (r_buf[i] <= short_res_min) {
			tmp_result = false;
			ab_ch[ab_pos++] = i + 1;
			ab_ch[0]++;

			if (r_buf[i] < SHORT_RESISTOR_ACCURACY) {
				ab_ch_300[ab_300_pos++] = i + 1;
				ab_ch_300[0]++;
			}
		}
	}

	*result = tmp_result;
	if (tmp_result) {
		FTS_TEST_DBG("short mode(%d) test pass\n", mode);
		return 0;
	}

	/* check again for resistor < 300 */
	if (ab_ch_300[0] == 0) {
		FTS_TEST_DBG("no channel resistor < 300");
		return 0;
	}
	FTS_TEST_DBG("get resisgor of channel < 300");
	ret = short_get_ab_ch(ab_ch_300, 0x00, mode, true, r_300);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get resistor(<300) fail\n");
		return ret;
	}

	for (i = 0; i < ab_ch_300[0]; i++) {
		r_buf[ab_ch_300[i + 1] - 1] = r_300[i];
	}

	FTS_TEST_DBG("abnormal channel:\n");
	for (i = 0; i < ab_ch[0] + 1; i++) {
		FTS_TEST_DBG("%2d ", ab_ch[i]);
	}

	FTS_TEST_DBG("all channel resistor:\n");
	print_buffer(r_buf, tdata->sc_node.channel_num, 0);

	if (mode == FACTROY_REG_SHORT_CA) {
		FTS_TEST_SAVE_ERR("CC:\n");
	} else {
		FTS_TEST_SAVE_ERR("CG:\n");
	}
	for (i = 1; i <= ab_ch[0]; i++) {
		if (ab_ch[i] < tdata->sc_node.tx_num) {
			FTS_TEST_SAVE_ERR("TX%d:%d ", ab_ch[i], r_buf[ab_ch[i] - 1]);
		} else {
			FTS_TEST_SAVE_ERR("RX%d:%d ", ab_ch[i] - tx, r_buf[ab_ch[i] - 1]);
		}
	}
	FTS_TEST_SAVE_INFO("\n");

	return 0;
}

static int short_test_ch_to_all(struct fts_test *tdata, u8 *ab_ch, bool *result)
{
	int ret = 0;
	int i = 0;
	int *adc = NULL;

	FTS_TEST_DBG("short test:channel to all other\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	adc = tdata->buffer;

	/* get adc data */
	for (i = 1; i <= tdata->sc_node.channel_num; i++) {
		ab_ch[i] = i;
	}
	ab_ch[0] = tdata->sc_node.channel_num;

	ret = short_get_ab_ch(ab_ch, 0x03, FACTROY_REG_SHORT_CA, false, adc);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get resistor(%d) fail\n", FACTROY_REG_SHORT_CA);
		return ret;
	}

	/* verify */
	ret = short_compare_get_save(tdata, adc, FACTROY_REG_SHORT_CA, ab_ch, result);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("compare cg fail\n");
		return ret;
	}
	return 0;
}

static bool short_test_ch_to_gnd(struct fts_test *tdata, u8 *ab_ch, bool *result)
{
	int ret = 0;
	int i = 0;
	int *adc = NULL;
	int tmp_r[TX_NUM_MAX + RX_NUM_MAX] = { 0 };

	FTS_TEST_DBG("short test:channel to gnd\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	adc = tdata->buffer;

	/* set default value to max(valid) */
	for (i = 0; i < tdata->sc_node.channel_num; i++) {
		adc[i] = TEST_SHORT_RES_MAX;
	}

	ret = short_get_ab_ch(ab_ch, 0x03, FACTROY_REG_SHORT_CG, false, tmp_r);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get resistor(%d) fail\n", FACTROY_REG_SHORT_CA);
		return ret;
	}

	for (i = 0; i < ab_ch[0]; i++) {
		adc[ab_ch[i + 1] - 1] = tmp_r[i];
	}

	/* verify */
	ret = short_compare_get_save(tdata, adc, FACTROY_REG_SHORT_CG, ab_ch, result);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("compare cg fail\n");
		return ret;
	}

	return 0;
}

static int ft5472_rawdata_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int *rawdata = NULL;
	u8 fre = 0;
	u8 fir = 0;
	u8 normalize = 0;
	bool result = false;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: rawdata test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	rawdata = tdata->buffer;

	if (!thr->rawdata_h_min || !thr->rawdata_h_max || !test_result) {
		FTS_TEST_SAVE_ERR("rawdata_h_min/max test_result is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		goto test_err;
	}

	/* rawdata test in mapping mode */
	ret = mapping_switch(MAPPING);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("switch mapping fail,ret=%d\n", ret);
		goto test_err;
	}

	/* save origin value */
	ret = fts_test_read_reg(FACTORY_REG_NORMALIZE, &normalize);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read normalize fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_FRE_LIST, &fre);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x0A fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_FIR, &fir);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0xFB error,ret=%d\n", ret);
		goto test_err;
	}

	/* set to auto normalize */
	if (normalize != 0x01) {
		ret = fts_test_write_reg(FACTORY_REG_NORMALIZE, 0x01);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("write normalize fail,ret=%d\n", ret);
			goto restore_reg;
		}
	}

	ret = get_rawdata_mc(0x81, 0x01, rawdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get rawdata fail,ret=%d\n", ret);
		goto restore_reg;
	}

	/* show test data */
	show_data(rawdata, false);

	/* compare */
	result = compare_array(rawdata, thr->rawdata_h_min, thr->rawdata_h_max, false);

restore_reg:
	/* set the origin value */
	ret = fts_test_write_reg(FACTORY_REG_NORMALIZE, normalize);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore normalize fail,ret=%d\n", ret);
	}

	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore 0x0A fail,ret=%d\n", ret);
	}

	ret = fts_test_write_reg(FACTORY_REG_FIR, fir);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore 0xFB fail,ret=%d\n", ret);
	}

test_err:
	if (result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ Rawdata Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("------ Rawdata Test NG\n");
	}

	/* save test data */
	fts_test_save_data("Rawdata Test", CODE_M_RAWDATA_TEST, rawdata, 0, false, false, *test_result);

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft5472_scap_cb_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	bool tmp2_result = false;
	u8 wc_sel = 0;
	u8 sc_mode = 0;
	int byte_num = 0;
	bool fw_wp_check = false;
	bool tx_check = false;
	bool rx_check = false;
	int *scap_cb = NULL;
	int *scb_tmp = NULL;
	int scb_cnt = 0;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Scap CB Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	scap_cb = tdata->buffer;
	byte_num = tdata->sc_node.node_num * 2;

	if ((tdata->sc_node.node_num * 2) > tdata->buffer_length) {
		FTS_TEST_SAVE_ERR("scap cb num(%d) > buffer length(%d)",
				  tdata->sc_node.node_num * 2, tdata->buffer_length);
		ret = -EINVAL;
		goto test_err;
	}

	if (!thr->scap_cb_on_min || !thr->scap_cb_on_max
	    || !thr->scap_cb_off_min || !thr->scap_cb_off_max || !test_result) {
		FTS_TEST_SAVE_ERR("scap_cb_on/off_min/max test_result is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	/* SCAP CB is in no-mapping mode */
	ret = mapping_switch(NO_MAPPING);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("switch no-mapping fail,ret=%d\n", ret);
		goto test_err;
	}

	/* get waterproof channel select */
	ret = fts_test_read_reg(FACTORY_REG_WC_SEL, &wc_sel);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read water_channel_sel fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_MC_SC_MODE, &sc_mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read sc_mode fail,ret=%d\n", ret);
		goto test_err;
	}

	/* water proof on check */
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_ON);
	if (thr->basic.scap_cb_wp_on_check && fw_wp_check) {
		scb_tmp = scap_cb + scb_cnt;
		/* 1:waterproof 0:non-waterproof */
		ret = get_cb_mc_sc(WATER_PROOF_ON, byte_num, scb_tmp, DATA_TWO_BYTE);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read sc_cb fail,ret=%d\n", ret);
			goto restore_reg;
		}

		/* show Scap CB */
		FTS_TEST_SAVE_INFO("scap_cb in waterproof on mode:\n");
		show_data_mc_sc(scb_tmp);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_RX);
		tmp_result = compare_mc_sc(tx_check, rx_check, scb_tmp, thr->scap_cb_on_min, thr->scap_cb_on_max);

		scb_cnt += tdata->sc_node.node_num;
	} else {
		tmp_result = true;
	}

	/* water proof off check */
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_OFF);
	if (thr->basic.scap_cb_wp_on_check && fw_wp_check) {
		scb_tmp = scap_cb + scb_cnt;
		/* 1:waterproof 0:non-waterproof */
		ret = get_cb_mc_sc(WATER_PROOF_OFF, byte_num, scb_tmp, DATA_TWO_BYTE);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read sc_cb fail,ret=%d\n", ret);
			goto restore_reg;
		}

		/* show Scap CB */
		FTS_TEST_SAVE_INFO("scap_cb in waterproof off mode:\n");
		show_data_mc_sc(scb_tmp);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_RX);
		tmp2_result = compare_mc_sc(tx_check, rx_check, scb_tmp, thr->scap_cb_off_min, thr->scap_cb_off_max);

		scb_cnt += tdata->sc_node.node_num;
	} else {
		tmp2_result = true;
	}

restore_reg:
	ret = fts_test_write_reg(FACTORY_REG_MC_SC_MODE, sc_mode);	/* set the origin value */
	if (ret) {
		FTS_TEST_SAVE_ERR("write sc mode fail,ret=%d\n", ret);
	}
test_err:
	if (tmp_result && tmp2_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ SCAP CB Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("------ SCAP CB Test NG\n");
	}

	/* save Scap CB */
	fts_test_save_data("SCAP CB Test", CODE_M_SCAP_CB_TEST, scap_cb, scb_cnt, true, false, *test_result);

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft5472_scap_rawdata_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	bool tmp2_result = false;
	u8 wc_sel = 0;
	bool fw_wp_check = false;
	bool tx_check = false;
	bool rx_check = false;
	int *scap_rawdata = NULL;
	int *srawdata_tmp = NULL;
	int srawdata_cnt = 0;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Scap Rawdata Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	scap_rawdata = tdata->buffer;

	if ((tdata->sc_node.node_num * 2) > tdata->buffer_length) {
		FTS_TEST_SAVE_ERR("scap rawdata num(%d) > buffer length(%d)",
				  tdata->sc_node.node_num * 2, tdata->buffer_length);
		ret = -EINVAL;
		goto test_err;
	}

	if (!thr->scap_rawdata_on_min || !thr->scap_rawdata_on_max
	    || !thr->scap_rawdata_off_min || !thr->scap_rawdata_off_max || !test_result) {
		FTS_TEST_SAVE_ERR("scap_rawdata_on/off_min/max test_result is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	/* SCAP CB is in no-mapping mode */
	ret = mapping_switch(NO_MAPPING);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("switch no-mapping fail,ret=%d\n", ret);
		goto test_err;
	}

	/* get waterproof channel select */
	ret = fts_test_read_reg(FACTORY_REG_WC_SEL, &wc_sel);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read water_channel_sel fail,ret=%d\n", ret);
		goto test_err;
	}

	/* scan rawdata */
	ret = start_scan();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("scan scap rawdata fail\n");
		goto test_err;
	}

	/* water proof on check */
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_ON);
	if (thr->basic.scap_rawdata_wp_on_check && fw_wp_check) {
		srawdata_tmp = scap_rawdata + srawdata_cnt;
		ret = get_rawdata_mc_sc(WATER_PROOF_ON, srawdata_tmp);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("get scap(WP_ON) rawdata fail\n");
			goto test_err;
		}

		FTS_TEST_SAVE_INFO("scap_rawdata in waterproof on mode:\n");
		show_data_mc_sc(srawdata_tmp);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_RX);
		tmp_result = compare_mc_sc(tx_check, rx_check, srawdata_tmp,
					   thr->scap_rawdata_on_min, thr->scap_rawdata_on_max);

		srawdata_cnt += tdata->sc_node.node_num;
	} else {
		tmp_result = true;
	}

	/* water proof off check */
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_OFF);
	if (thr->basic.scap_rawdata_wp_on_check && fw_wp_check) {
		srawdata_tmp = scap_rawdata + srawdata_cnt;
		ret = get_rawdata_mc_sc(WATER_PROOF_OFF, srawdata_tmp);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("get scap(WP_OFF) rawdata fail\n");
			goto test_err;
		}

		FTS_TEST_SAVE_INFO("scap_rawdata in waterproof off mode:\n");
		show_data_mc_sc(srawdata_tmp);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_RX);
		tmp2_result = compare_mc_sc(tx_check, rx_check, srawdata_tmp,
					    thr->scap_rawdata_off_min, thr->scap_rawdata_off_max);

		srawdata_cnt += tdata->sc_node.node_num;
	} else {
		tmp2_result = true;
	}

test_err:
	if (tmp_result && tmp2_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ SCAP Rawdata Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("------ SCAP Rawdata Test NG\n");
	}

	/* save scap rawdata data */
	fts_test_save_data("SCAP Rawdata Test", CODE_M_SCAP_RAWDATA_TEST,
			   scap_rawdata, srawdata_cnt, true, false, *test_result);

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft5472_short_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	bool tmp2_result = false;
	int byte_num = 0;
	int *adc = NULL;
	u8 *ab_ch = NULL;

	FTS_TEST_SAVE_INFO("\n============ Test Item: Short Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	adc = tdata->buffer;

	FTS_TEST_FUNC_ENTER();
	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	/* SCAP CB is in no-mapping mode */
	ret = mapping_switch(NO_MAPPING);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("switch no-mapping fail,ret=%d\n", ret);
		goto test_err;
	}

	/* get offset = readdata - 1024 */
	ret = short_get_adc_data_mc(TEST_RETVAL_AA, 1 * 2, &tdata->offset, FACTROY_REG_SHORT_OFFSET);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get weak short data fail,ret= %d\n", ret);
		goto test_err;
	}
	tdata->offset -= 1024;
	FTS_TEST_DBG("short offset:%d", tdata->offset);

	/* get code1 */
	ret = fts_test_write_reg(FACTROY_REG_SHORT_DELAY, 0x00);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write shot delay fail,ret:%d", ret);
		return ret;
	}

	byte_num = (tdata->sc_node.channel_num + 1) * 2;
	ret = short_get_adc_data_mc(TEST_RETVAL_AA, byte_num, adc, FACTROY_REG_SHORT_CA);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get short adc(%d) fail\n", FACTROY_REG_SHORT_CA);
		return ret;
	}
	tdata->code1 = adc[tdata->sc_node.channel_num];
	FTS_TEST_DBG("code1:%d", tdata->code1);

	ab_ch = fts_malloc(tdata->sc_node.channel_num + 1);
	if (ab_ch == NULL) {
		FTS_TEST_SAVE_ERR("malloc abnormal ch fail\n");
		ret = -ENOMEM;
		goto test_err;
	}

	/* channel to all others */
	ret = short_test_ch_to_all(tdata, ab_ch, &tmp_result);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("channel to all other channel fail\n");
		goto test_err;
	}

	/* test pass */
	if (tmp_result == true) {
		goto test_pass;
	}

	/* 1st test ng, need test 2nd */
	/* channel to gnd */
	ret = short_test_ch_to_gnd(tdata, ab_ch, &tmp2_result);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("channel to GND fail\n");
		goto test_err;
	}

test_pass:
	ret = 0;
test_err:
	fts_free(ab_ch);

	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ Short Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("------ Short Test NG\n");
	}
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft5472_panel_differ_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int i = 0;
	u8 fre = 0;
	u8 fir = 0;
	u8 normalize = 0;
	int *panel_differ = NULL;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Panel Differ Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	panel_differ = tdata->buffer;

	if (!thr->panel_differ_min || !thr->panel_differ_max || !test_result) {
		FTS_TEST_SAVE_ERR("panel_differ_h_min/max test_result is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		goto test_err;
	}

	/* panel differ test in mapping mode */
	ret = mapping_switch(MAPPING);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("switch mapping fail,ret=%d\n", ret);
		goto test_err;
	}

	/* save origin value */
	ret = fts_test_read_reg(FACTORY_REG_NORMALIZE, &normalize);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read normalize fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_FRE_LIST, &fre);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x0A fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_FIR, &fir);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0xFB fail,ret=%d\n", ret);
		goto test_err;
	}

	/* set to overall normalize */
	if (normalize != NORMALIZE_OVERALL) {
		ret = fts_test_write_reg(FACTORY_REG_NORMALIZE, NORMALIZE_OVERALL);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("write normalize fail,ret=%d\n", ret);
			goto restore_reg;
		}
	}

	ret = get_rawdata_mc(0x81, 0x00, panel_differ);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get rawdata fail,ret=%d\n", ret);
		goto restore_reg;
	}

	for (i = 0; i < tdata->node.node_num; i++) {
		panel_differ[i] = panel_differ[i] / 10;
	}

	/* show test data */
	show_data(panel_differ, false);

	/* compare */
	tmp_result = compare_array(panel_differ, thr->panel_differ_min, thr->panel_differ_max, false);

restore_reg:
	/* set the origin value */
	ret = fts_test_write_reg(FACTORY_REG_NORMALIZE, normalize);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore normalize fail,ret=%d\n", ret);
	}

	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore 0x0A fail,ret=%d\n", ret);
	}

	ret = fts_test_write_reg(FACTORY_REG_FIR, fir);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore 0xFB fail,ret=%d\n", ret);
	}
test_err:
	/* result */
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ Panel Diff Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("------ Panel Diff Test NG\n");
	}

	/* save test data */
	fts_test_save_data("Panel Diff Test", CODE_M_PANELDIFFER_TEST, panel_differ, 0, false, false, *test_result);

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int start_test_ft5472(void)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;
	struct mc_sc_testitem *test_item = &tdata->ic.mc_sc.u.item;
	bool temp_result = false;
	bool test_result = true;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_INFO("test item:0x%x", fts_ftest->ic.mc_sc.u.tmp);

	/* rawdata test */
	if (test_item->rawdata_test  == true) {
		ret = ft5472_rawdata_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		}
	}

	/* scap_cb test */
	if (test_item->scap_cb_test == true) {
		ret = ft5472_scap_cb_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		}
	}

	/* scap_rawdata test */
	if (test_item->scap_rawdata_test == true) {
		ret = ft5472_scap_rawdata_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		}
	}

	/* short test */
	if (test_item->short_test == true) {
		ret = ft5472_short_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_GT_SHORT;
		}
	}

	/* panel differ test */
	if (test_item->panel_differ_test == true) {
		ret = ft5472_panel_differ_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_tptest_result = fts_tptest_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		}
	}

	/* restore mapping state */
	fts_test_write_reg(FACTORY_REG_NOMAPPING, tdata->mapping);
	return test_result;
}

struct test_funcs test_func_ft5472 = {
	.ctype = {0x83},
	.hwtype = IC_HW_MC_SC,
	.key_num_total = 0,
	.start_test = start_test_ft5472,
};
#endif
