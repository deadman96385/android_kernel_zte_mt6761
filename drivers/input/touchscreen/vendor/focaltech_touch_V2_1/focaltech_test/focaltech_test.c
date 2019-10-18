/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2018, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/************************************************************************
*
* File Name: focaltech_test.c
*
*Author : Focaltech Driver Team
*
* Created: 2016-08-01
*
* Modify:
*
* Abstract: create char device and proc node for  the comm between APK and TP
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "focaltech_test.h"
#include "tpd_sys.h"
#include <linux/time.h>
#include <linux/rtc.h>

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_test_data test_data;
static unsigned char g_str_save_file_path[256];
static unsigned char g_str_ini_file_path[256];
static unsigned char g_str_ini_filename[128];
static int fts_test_get_save_filename(char *filename, char *save_filename, int len);
int g_int_tptest_result = 0;

struct test_funcs *test_funcs_list[] = {
	#if (FTS_CHIP_TYPE == _FT3C47U)
	&test_func_ft3c47,
	#elif (FTS_CHIP_TYPE == _FT3D47)
	&test_func_ft3d47,
	#elif (FTS_CHIP_TEST_TYPE == FT5822_TEST)
	&test_func_ft5822,
	#elif (FTS_CHIP_TEST_TYPE == FT5X46_TEST)
	&test_func_ft5x46,
	#elif (FTS_CHIP_TEST_TYPE == FT5452_TEST)
	&test_func_ft5452,
	#elif (FTS_CHIP_TEST_TYPE == FT6X36_TEST)
	&test_func_ft6x36,
	#elif (FTS_CHIP_TYPE == _FT8006M)
	&test_func_ft8006m,
	#elif (FTS_CHIP_TYPE == _FT8006U)
	&test_func_ft8006u,
	#elif (FTS_CHIP_TYPE == _FT8201)
	&test_func_ft8201,
	#elif (FTS_CHIP_TYPE == _FT8606)
	&test_func_ft8606,
	#elif (FTS_CHIP_TYPE == _FT8607)
	&test_func_ft8607,
	#elif (FTS_CHIP_TYPE == _FT8716)
	&test_func_ft8716,
	#elif (FTS_CHIP_TYPE == _FT8736)
	&test_func_ft8736,
	#elif (FTS_CHIP_TYPE == _FTE716)
	&test_func_fte716,
	#elif (FTS_CHIP_TYPE == _FT8719)
	&test_func_ft8719,
	#elif (FTS_CHIP_TYPE == _FT8739)
	&test_func_ft8739,
	#endif
};

struct test_ic_type ic_types[] = {
	{"FT5X22", 6, IC_FT5X46},
	{"FT5X46", 6, IC_FT5X46},
	{"FT5X46i", 7, IC_FT5X46I},
	{"FT5526", 6, IC_FT5526},
	{"FT3X17", 6, IC_FT3X17},
	{"FT5436", 6, IC_FT5436},
	{"FT3X27", 6, IC_FT3X27},
	{"FT5526i", 7, IC_FT5526I},
	{"FT5416", 6, IC_FT5416},
	{"FT5426", 6, IC_FT5426},
	{"FT5435", 6, IC_FT5435},
	{"FT7681", 6, IC_FT7681},
	{"FT7661", 6, IC_FT7661},
	{"FT7511", 6, IC_FT7511},
	{"FT7421", 6, IC_FT7421},
	{"FT7311", 6, IC_FT7311},
	{"FT3327DQQ-001", 13, IC_FT3327DQQ_001},
	{"FT5446DQS-W01", 13, IC_FT5446DQS_W01},
	{"FT5446DQS-W02", 13, IC_FT5446DQS_W02},
	{"FT5446DQS-002", 13, IC_FT5446DQS_002},
	{"FT5446DQS-Q02", 13, IC_FT5446DQS_Q02},

	{"FT6X36", 6, IC_FT6X36},
	{"FT3X07", 6, IC_FT3X07},
	{"FT6416", 6, IC_FT6416},
	{"FT6336G/U", 9, IC_FT6426},
	{"FT6236U", 7, IC_FT6236U},
	{"FT6436U", 7, IC_FT6436U},
	{"FT3267", 6, IC_FT3267},
	{"FT3367", 6, IC_FT3367},
	{"FT7401", 6, IC_FT7401},
	{"FT3407U", 7, IC_FT3407U},

	{"FT5822", 6, IC_FT5822},
	{"FT5626", 6, IC_FT5626},
	{"FT5726", 6, IC_FT5726},
	{"FT5826B", 7, IC_FT5826B},
	{"FT3617", 6, IC_FT3617},
	{"FT3717", 6, IC_FT3717},
	{"FT7811", 6, IC_FT7811},
	{"FT5826S", 7, IC_FT5826S},
	{"FT3517U", 7, IC_FT3517U},

	{"FT3518", 6, IC_FT3518},
	{"FT3558", 6, IC_FT3558},

	{"FT8606", 6, IC_FT8606},

	{"FT8716U", 7, IC_FT8716U},
	{"FT8716F", 7, IC_FT8716F},
	{"FT8716", 6, IC_FT8716},
	{"FT8613", 6, IC_FT8613},

	{"FT3C47U", 7, IC_FT3C47U},

	{"FT8607U", 7, IC_FT8607U},
	{"FT8607", 6, IC_FT8607},

	{"FT8736", 6, IC_FT8736},

	{"FT3D47", 6, IC_FT3D47},

	{"FTE716", 6, IC_FTE716},

	{"FT8201", 6, IC_FT8201},

	{"FT8006M", 7, IC_FT8006M},
	{"FT7250", 6, IC_FT7250},

	{"FT8006U", 7, IC_FT8006U},

	{"FT8719", 6, IC_FT8719},
	{"FT8615", 6, IC_FT8615},

	{"FT8739", 6, IC_FT8739},
};

/*****************************************************************************
* Static function prototypes
*****************************************************************************/

/*****************************************************************************
* functions body
*****************************************************************************/
void sys_delay(int ms)
{
	msleep(ms);
}

int focal_abs(int value)
{
	if (value < 0)
		value = 0 - value;

	return value;
}

void *fts_malloc(size_t size)
{
	return vmalloc(size);
}

void fts_free_proc(void *p)
{
	return vfree(p);
}

/********************************************************************
 * test i2c read/write interface
 *******************************************************************/
int fts_test_i2c_read(u8 *writebuf, int writelen, u8 *readbuf, int readlen)
{
	int ret = 0;

	if (fts_data == NULL) {
		FTS_TEST_ERROR("fts_data is null, no test");
		return -EINVAL;
	}
	ret = fts_i2c_read(fts_data->client, writebuf, writelen, readbuf, readlen);

	if (ret < 0)
		return ret;
	else
		return 0;
}

int fts_test_i2c_write(u8 *writebuf, int writelen)
{
	int ret = 0;

	if (fts_data == NULL) {
		FTS_TEST_ERROR("fts_data is null, no test");
		return -EINVAL;
	}
	ret = fts_i2c_write(fts_data->client, writebuf, writelen);

	if (ret < 0)
		return ret;
	else
		return 0;
}

int read_reg(u8 addr, u8 *val)
{
	return fts_test_i2c_read(&addr, 1, val, 1);
}

int write_reg(u8 addr, u8 val)
{
	int ret;
	u8 cmd[2] = { 0 };

	cmd[0] = addr;
	cmd[1] = val;
	ret = fts_test_i2c_write(cmd, 2);

	return ret;
}

/********************************************************************
 * test global function enter work/factory mode
 *******************************************************************/
int enter_work_mode(void)
{
	int ret = 0;
	u8 mode = 0;
	int i = 0;
	int j = 0;

	FTS_TEST_FUNC_ENTER();

	ret = read_reg(DEVIDE_MODE_ADDR, &mode);
	if ((ret == 0) && (((mode >> 4) & 0x07) == 0x00))
		return 0;

	for (i = 0; i < ENTER_WORK_FACTORY_RETRIES; i++) {
		ret = write_reg(DEVIDE_MODE_ADDR, 0x00);
		if (ret == 0) {
			for (j = 0; j < 20; j++) {
				ret = read_reg(DEVIDE_MODE_ADDR, &mode);
				if ((ret == 0) && (((mode >> 4) & 0x07) == 0x00))
					goto OUT;
				else
					sys_delay(FACTORY_TEST_DELAY);
			}
		}

		sys_delay(50);
	}

	if (i >= ENTER_WORK_FACTORY_RETRIES) {
		FTS_TEST_ERROR("Enter work mode fail");
		return -EIO;
	}
OUT:
	FTS_TEST_FUNC_EXIT();
	return 0;
}

int enter_factory_mode(void)
{
	int ret = 0;
	u8 mode = 0;
	int i = 0;
	int j = 0;

	ret = read_reg(DEVIDE_MODE_ADDR, &mode);
	if ((ret == 0) && (((mode >> 4) & 0x07) == 0x04))
		return 0;

	for (i = 0; i < ENTER_WORK_FACTORY_RETRIES; i++) {
		ret = write_reg(DEVIDE_MODE_ADDR, 0x40);
		if (ret == 0) {
			for (j = 0; j < 20; j++) {
				ret = read_reg(DEVIDE_MODE_ADDR, &mode);
				if ((ret == 0) && (((mode >> 4) & 0x07) == 0x04)) {
					sys_delay(200);
					goto ENTER_FACTORY_MODE;
				} else
					sys_delay(FACTORY_TEST_DELAY);
			}
		}

		sys_delay(50);
	}
	if (i >= ENTER_WORK_FACTORY_RETRIES) {
		FTS_TEST_ERROR("Enter factory mode fail");
		return -EIO;
	}
ENTER_FACTORY_MODE:
	return 0;
}

/************************************************************************
* Name: fts_i2c_read_write
* Brief:  Write/Read Data by IIC
* Input: writebuf, writelen, readlen
* Output: readbuf
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char fts_i2c_read_write(unsigned char *writebuf, int writelen, unsigned char *readbuf, int readlen)
{
	int ret;

	if (readlen > 0) {
		ret = fts_test_i2c_read(writebuf, writelen, readbuf, readlen);
	} else {
		ret = fts_test_i2c_write(writebuf, writelen);
	}

	if (ret >= 0)
		goto CODE_OK;
	else
		goto CODE_COMM_ERROR;
CODE_OK:
	return ERROR_CODE_OK;
CODE_COMM_ERROR:
	return ERROR_CODE_COMM_ERROR;
}

/*
 * read_mass_data - read rawdata/short test data
 * addr - register addr which read data from
 * byte_num - read data length, unit:byte
 * buf - save data
 *
 * return 0 if read data succuss, otherwise return error code
 */
int read_mass_data(u8 addr, int byte_num, int *buf)
{
	int ret = 0;
	int i = 0;
	u8 reg_addr = 0;
	u8 *data = NULL;
	int read_num = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;

	data = fts_malloc(byte_num * sizeof(u8));
	if (data == NULL) {
		FTS_TEST_SAVE_ERR("rawdata buffer malloc fail\n");
		return -ENOMEM;
	}

	/* read rawdata buffer */
	FTS_TEST_INFO("mass data len:%d", byte_num);
	packet_num = byte_num / BYTES_PER_TIME;
	packet_remainder = byte_num % BYTES_PER_TIME;
	if (packet_remainder)
		packet_num++;

	if (byte_num < BYTES_PER_TIME) {
		read_num = byte_num;
	} else {
		read_num = BYTES_PER_TIME;
	}
	FTS_TEST_INFO("packet num:%d, remainder:%d", packet_num, packet_remainder);

	reg_addr = addr;
	ret = fts_test_i2c_read(&reg_addr, 1, data, read_num);
	if (ret) {
		FTS_TEST_SAVE_ERR("read rawdata fail\n");
		goto READ_MASSDATA_ERR;
	}

	for (i = 1; i < packet_num; i++) {
		offset = read_num * i;
		if ((i == (packet_num - 1)) && packet_remainder) {
			read_num = packet_remainder;
		}

		ret = fts_test_i2c_read(NULL, 0, data + offset, read_num);
		if (ret) {
			FTS_TEST_SAVE_ERR("read much rawdata fail\n");
			goto READ_MASSDATA_ERR;
		}
	}

	for (i = 0; i < byte_num; i = i + 2) {
		buf[i >> 1] = (int)(((int)(data[i]) << 8) + data[i + 1]);
	}

READ_MASSDATA_ERR:
	if (data) {
		fts_free(data);
	}

	return ret;
}

/*
 * chip_clb_incell - auto clb
 */
int chip_clb_incell(void)
{
	int ret = 0;
	u8 val = 0;
	int times = 0;

	/* start clb */
	ret = write_reg(FACTORY_REG_CLB, 0x04);
	if (ret) {
		FTS_TEST_SAVE_ERR("write start clb fail\n");
		return ret;
	}

	while (times++ < FACTORY_TEST_RETRY) {
		sys_delay(FACTORY_TEST_RETRY_DELAY);
		ret = read_reg(FACTORY_REG_CLB, &val);
		if ((ret == 0) && (val == 0x02)) {
			/* clb ok */
			goto CHIP_CLB_OK;
		} else
			FTS_TEST_DBG("reg%x=%x,retry:%d", FACTORY_REG_CLB, val, times);
	}
CHIP_CLB_OK:
	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("chip clb timeout\n");
		return -EIO;
	}

	return 0;
}

/*
 * start_scan_incell - start to scan a frame for incell ic
 */
int start_scan_incell(void)
{
	int ret = 0;
	u8 val = 0x00;
	int times = 0;

	ret = read_reg(DEVIDE_MODE_ADDR, &val);
	if (ret) {
		FTS_TEST_SAVE_ERR("read device mode fail\n");
		return ret;
	}

	/* Top bit position 1, start scan */
	val |= 0x80;
	ret = write_reg(DEVIDE_MODE_ADDR, val);
	if (ret) {
		FTS_TEST_SAVE_ERR("write device mode fail\n");
		return ret;
	}

	while (times++ < FACTORY_TEST_RETRY) {
		/* Wait for the scan to complete */
		sys_delay(FACTORY_TEST_DELAY);

		ret = read_reg(DEVIDE_MODE_ADDR, &val);
		if ((ret == 0) && ((val >> 7) == 0)) {
			goto MODE_ADDR_READ_OK;
		} else
			FTS_TEST_DBG("reg%x=%x,retry:%d", DEVIDE_MODE_ADDR, val, times);
	}
MODE_ADDR_READ_OK:
	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("start scan timeout\n");
		return -EIO;
	}

	return 0;
}

/*
 * wait_state_update - wait fw status update
 */
int wait_state_update(void)
{
	int ret = 0;
	int times = 0;
	u8 state = 0xFF;

	while (times++ < FACTORY_TEST_RETRY) {
		sys_delay(FACTORY_TEST_RETRY_DELAY);
		/* Wait register status update */
		state = 0xFF;
		ret = read_reg(FACTORY_REG_PARAM_UPDATE_STATE, &state);
		if ((ret == 0) && (state == 0x00))
			goto UPDATE_STATE_OK;
		else
			FTS_TEST_DBG("reg%x=%x,retry:%d", FACTORY_REG_PARAM_UPDATE_STATE, state, times);
	}
UPDATE_STATE_OK:
	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("Wait State Update fail\n");
		return -EIO;
	}

	return 0;
}

/*
 * get_rawdata_incell - read rawdata/diff for incell ic
 */
int get_rawdata_incell(int *data)
{
	int ret = 0;
	u8 tx_num = 0;
	u8 rx_num = 0;
	u8 key_num = 0;
	int va_num = 0;

	FTS_TEST_FUNC_ENTER();
	/* enter into factory mode */
	ret = enter_factory_mode();
	if (ret) {
		FTS_TEST_SAVE_ERR("Failed to Enter Factory Mode\n");
		return ret;
	}

	/* get tx/rx num */
	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	va_num = tx_num * rx_num;
	key_num = test_data.screen_param.key_num_total;

	/* start to scan a frame */
	ret = start_scan_incell();
	if (ret) {
		FTS_TEST_SAVE_ERR("Failed to Scan ...\n");
		return ret;
	}

	/* Read RawData for va Area */
	ret = write_reg(FACTORY_REG_LINE_ADDR, 0xAD);
	if (ret) {
		FTS_TEST_SAVE_ERR("write AD to reg01 fail\n");
		return ret;
	}
	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR, va_num * 2, data);
	if (ret) {
		FTS_TEST_SAVE_ERR("Failed to Get RawData of channel.\n");
		return ret;
	}

	/* Read RawData for key Area */
	ret = write_reg(FACTORY_REG_LINE_ADDR, 0xAE);
	if (ret) {
		FTS_TEST_SAVE_ERR("write AE to reg01 fail\n");
		return ret;
	}
	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR, key_num * 2, data + va_num);
	if (ret) {
		FTS_TEST_SAVE_ERR("Failed to Get RawData of keys.\n");
		return ret;
	}

	FTS_TEST_FUNC_EXIT();
	return 0;
}

/*
 * get_cb_incell - get cb data for incell IC
 */
int get_cb_incell(u16 saddr, int byte_num, int *cb_buf)
{
	int ret = 0;
	int i = 0;
	u8 cb_addr = 0;
	u8 addr_h = 0;
	u8 addr_l = 0;
	int read_num = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;
	int addr = 0;
	u8 *data = NULL;

	data = fts_malloc(byte_num * sizeof(u8));
	if (data == NULL) {
		FTS_TEST_SAVE_ERR("cb buffer malloc fail\n");
		return -ENOMEM;
	}

	packet_num = byte_num / BYTES_PER_TIME;
	packet_remainder = byte_num % BYTES_PER_TIME;
	if (packet_remainder)
		packet_num++;
	read_num = BYTES_PER_TIME;

	FTS_TEST_INFO("cb packet:%d,remainder:%d", packet_num, packet_remainder);
	cb_addr = FACTORY_REG_CB_ADDR;
	for (i = 0; i < packet_num; i++) {
		offset = read_num * i;
		addr = saddr + offset;
		addr_h = (addr >> 8) & 0xFF;
		addr_l = addr & 0xFF;
		if ((i == (packet_num - 1)) && packet_remainder) {
			read_num = packet_remainder;
		}

		ret = write_reg(FACTORY_REG_CB_ADDR_H, addr_h);
		if (ret) {
			FTS_TEST_SAVE_ERR("write cb addr high fail\n");
			goto TEST_CB_ERR;
		}
		ret = write_reg(FACTORY_REG_CB_ADDR_L, addr_l);
		if (ret) {
			FTS_TEST_SAVE_ERR("write cb addr low fail\n");
			goto TEST_CB_ERR;
		}

		ret = fts_test_i2c_read(&cb_addr, 1, data + offset, read_num);
		if (ret) {
			FTS_TEST_SAVE_ERR("read cb fail\n");
			goto TEST_CB_ERR;
		}
	}

	for (i = 0; i < byte_num; i++) {
		cb_buf[i] = data[i];
	}

TEST_CB_ERR:
	fts_free(data);
	return ret;
}

/*
 * weakshort_get_adc_data_incell - get short(adc) data for incell IC
 */
int weakshort_get_adc_data_incell(u8 retval, u8 ch_num, int byte_num, int *adc_buf)
{
	int ret = 0;
	int times = 0;
	u8 short_state = 0;

	FTS_TEST_FUNC_ENTER();

	/* Start ADC sample */
	ret = write_reg(FACTORY_REG_SHORT_TEST_EN, 0x01);
	if (ret) {
		FTS_TEST_SAVE_ERR("start short test fail\n");
		goto ADC_ERROR;
	}
	sys_delay(ch_num * FACTORY_TEST_DELAY);

	for (times = 0; times < FACTORY_TEST_RETRY; times++) {
		ret = read_reg(FACTORY_REG_SHORT_TEST_STATE, &short_state);
		if ((ret == 0) && (retval == short_state))
			goto GET_SHORT_TEST_STATE_OK;
		else
			FTS_TEST_DBG("reg%x=%x,retry:%d", FACTORY_REG_SHORT_TEST_STATE, short_state, times);

		sys_delay(FACTORY_TEST_RETRY_DELAY);
	}
GET_SHORT_TEST_STATE_OK:
	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("short test timeout, ADC data not OK\n");
		ret = -EIO;
		goto ADC_ERROR;
	}

	ret = read_mass_data(FACTORY_REG_SHORT_ADDR, byte_num, adc_buf);
	if (ret) {
		FTS_TEST_SAVE_ERR("get short(adc) data fail\n");
	}

ADC_ERROR:
	FTS_TEST_FUNC_EXIT();
	return ret;
}

/*
 * show_data_incell - show and save test data to testresult.txt
 */
void show_data_incell(int *data, bool include_key)
{
	int row = 0;
	int col = 0;
	int tx_num = test_data.screen_param.tx_num;
	int rx_num = test_data.screen_param.rx_num;
	int key_num = test_data.screen_param.key_num_total;

	FTS_TEST_SAVE_INFO("\nVA Channels: ");
	for (row = 0; row < tx_num; row++) {
		FTS_TEST_SAVE_INFO("\nCh_%02d:  ", row + 1);
		for (col = 0; col < rx_num; col++) {
			FTS_TEST_SAVE_INFO("%5d, ", data[row * rx_num + col]);
		}
	}

	if (include_key) {
		FTS_TEST_SAVE_INFO("\nKeys:   ");
		for (col = 0; col < key_num; col++) {
			FTS_TEST_SAVE_INFO("%5d, ", data[rx_num * tx_num + col]);
		}
	}
	FTS_TEST_SAVE_INFO("\n");
}

/*
 * compare_data_incell - check data in range or not
 *
 * return true if check pass, or return false
 */
bool compare_data_incell(int *data, int min, int max, int vk_min, int vk_max, bool include_key)
{
	int row = 0;
	int col = 0;
	int value = 0;
	bool tmp_result = true;
	int tx_num = test_data.screen_param.tx_num;
	int rx_num = test_data.screen_param.rx_num;
	int key_num = test_data.screen_param.key_num_total;

	/* VA area */
	for (row = 0; row < tx_num; row++) {
		for (col = 0; col < rx_num; col++) {
			if (test_data.incell_detail_thr.invalid_node[row * rx_num + col] == 0)
				continue;	/* Invalid Node */
			value = data[row * rx_num + col];
			if (value < min || value > max) {
				tmp_result = false;
				fts_save_failed_node(tx_num, rx_num, row, col);
				FTS_TEST_SAVE_INFO("test failure. Node=(%d, %d), Get_value=%d, Set_Range=(%d, %d)\n",
						   row + 1, col + 1, value, min, max);
			}
		}
	}
	/* Key area */
	if (include_key) {
		if (test_data.screen_param.key_flag) {
			key_num = test_data.screen_param.key_num;
		}
		row = test_data.screen_param.tx_num;
		for (col = 0; col < key_num; col++) {
			if (test_data.incell_detail_thr.invalid_node[tx_num * rx_num + col] == 0)
				continue;	/* Invalid Node */
			value = data[rx_num * tx_num + col];
			if (value < vk_min || value > vk_max) {
				tmp_result = false;
				fts_save_failed_node(tx_num, rx_num, row, col);
				FTS_TEST_SAVE_INFO("test failure. Node=(%d, %d), Get_value=%d,  Set_Range=(%d, %d)\n",
						   row + 1, col + 1, value, vk_min, vk_max);
			}
		}
	}

	return tmp_result;
}

/************************************************************************
* Name: compare_detailthreshold_data_incell
* Brief:  compare_detailthreshold_data_incell
* Input: none
* Output: none
* Return: none.
***********************************************************************/
bool compare_detailthreshold_data_incell(int *data, int *data_min, int *data_max, bool include_key)
{
	int row, col;
	int value;
	bool tmp_result = true;
	int tmp_min, tmp_max;
	int rx_num = test_data.screen_param.rx_num;
	int tx_num = test_data.screen_param.tx_num;
	int key_num = test_data.screen_param.key_num_total;
	/* VA */
	for (row = 0; row < tx_num; row++) {
		for (col = 0; col < rx_num; col++) {
			if (test_data.incell_detail_thr.invalid_node[row * rx_num + col] == 0)
				continue;	/* Invalid Node */
			tmp_min = data_min[row * rx_num + col];
			tmp_max = data_max[row * rx_num + col];
			value = data[row * rx_num + col];
			if (value < tmp_min || value > tmp_max) {
				tmp_result = false;
				fts_save_failed_node(tx_num, rx_num, row, col);
				FTS_TEST_SAVE_INFO
				("\n test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d)",
				 row + 1, col + 1, value, tmp_min, tmp_max);
			}
		}
	}
	/* Key */
	if (include_key) {
		if (test_data.screen_param.key_flag) {
			key_num = test_data.screen_param.key_num;
		}
		row = test_data.screen_param.tx_num;
		for (col = 0; col < key_num; col++) {
			if (test_data.incell_detail_thr.invalid_node[rx_num * tx_num + col] == 0)
				continue;	/* Invalid Node */
			tmp_min = data_min[rx_num * tx_num + col];
			tmp_max = data_max[rx_num * tx_num + col];
			value = data[rx_num * tx_num + col];
			if (value < tmp_min || value > tmp_max) {
				tmp_result = false;
				fts_save_failed_node(tx_num, rx_num, row, col);
				FTS_TEST_SAVE_INFO
				("\n test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d)",
				 row + 1, col + 1, value, tmp_min, tmp_max);
			}
		}
	}

	return tmp_result;
}

/*
 * save_testdata_incell - save data to testdata.csv
 */
void save_testdata_incell(int *data, char *test_num, int index, u8 row, u8 col, u8 item_count)
{
	int len = 0;
	int i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	/* Save Msg */
	len = snprintf(test_data.tmp_buffer, BUFF_LEN_TMP_BUFFER,
		       "%s, %d, %d, %d, %d, %d, ",
		       test_num, test_data.test_item_code, row, col, test_data.start_line, item_count);

	memcpy(test_data.msg_area_line2 + test_data.len_msg_area_line2, test_data.tmp_buffer, len);
	test_data.len_msg_area_line2 += len;

	test_data.start_line += row;
	test_data.test_data_count++;

	/* Save Data */
	for (i = 0 + index; (i < row + index) && (i < TX_NUM_MAX); i++) {
		for (j = 0; (j < col) && (j < RX_NUM_MAX); j++) {
			if (j == (col - 1)) {
				/* The Last Data of the row, add "\n" */
				len = snprintf(test_data.tmp_buffer, BUFF_LEN_TMP_BUFFER,
					       "%d,\n", data[col * (i + index) + j]);
			} else {
				len = snprintf(test_data.tmp_buffer, BUFF_LEN_TMP_BUFFER,
					       "%d, ", data[col * (i + index) + j]);
			}

			memcpy(test_data.store_data_area + test_data.len_store_data_area,
			       test_data.tmp_buffer, len);
			test_data.len_store_data_area += len;
		}
	}

	FTS_TEST_FUNC_EXIT();
}

/*
 * fts_ic_table_get_ic_code_from_ic_name - get ic code from ic name
 */
u32 fts_ic_table_get_ic_code_from_ic_name(char *ic_name)
{
	int i = 0;
	int type_size = 0;
	int ic_types_len = 0;

	ic_types_len = sizeof(ic_types[0]);
	type_size = sizeof(ic_types) / ic_types_len;
	for (i = 0; i < type_size; i++) {
		if (strncmp(ic_name, ic_types[i].ic_name, ic_types[i].len) == 0)
			return ic_types[i].ic_type;
	}

	FTS_TEST_ERROR("no IC type match");
	return 0;
}

/*
 * init_test_funcs - get test function based on ic_type
 */
int init_test_funcs(u32 ic_type)
{
	int i = 0;
	struct test_funcs *funcs = test_funcs_list[0];
	int funcs_len = 0;
	int test_funcs_list_len = 0;
	u32 ic_series = 0;

	test_funcs_list_len = sizeof(test_funcs_list[0]);
	funcs_len = sizeof(test_funcs_list) / test_funcs_list_len;
	ic_series = TEST_ICSERIES(ic_type);
	FTS_TEST_INFO("ic_type:%x, test functions len:%x", ic_type, funcs_len);
	for (i = 0; i < funcs_len; i++) {
		funcs = test_funcs_list[i];
		if (ic_series == funcs->ic_series) {
			test_data.func = funcs;
			break;
		}
	}

	if (i >= funcs_len) {
		FTS_TEST_ERROR("no ic serial function match");
		return -ENODATA;
	}

	return 0;
}

/*
 * fts_test_save_test_data - Save test data to SD card etc.
 */
static int fts_test_save_test_data(char *file_name, char *data_buf, int len)
{
	struct file *pfile = NULL;
	char filepath[128];
	loff_t pos;
	mm_segment_t old_fs;

	FTS_TEST_FUNC_ENTER();
	memset(filepath, 0, sizeof(filepath));
	snprintf(filepath, 128, "%s%s", g_str_save_file_path, file_name);
	if (pfile == NULL) {
		pfile = filp_open(filepath, O_TRUNC | O_CREAT | O_RDWR, 0);
	}
	if (IS_ERR(pfile)) {
		FTS_TEST_ERROR("error occurred while opening file %s.", filepath);
		return -EIO;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(pfile, data_buf, len, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	FTS_TEST_FUNC_EXIT();
	return 0;
}

/************************************************************************
* Name: fts_set_testitem
* Brief:  set test item code and name
* Input: null
* Output: null
* Return:
**********************************************************************/
void fts_set_testitem(u8 itemcode)
{
	test_data.test_item[test_data.test_num].itemcode = itemcode;
	test_data.test_item[test_data.test_num].testnum = test_data.test_num;
	test_data.test_item[test_data.test_num].testresult = RESULT_NULL;
	test_data.test_num++;
}

/************************************************************************
* Name: merge_all_testdata
* Brief:  Merge All Data of test result
* Input: none
* Output: none
* Return: none
***********************************************************************/
void merge_all_testdata(void)
{
	int len = 0;

	/* Msg Area, Add Line1 */
	test_data.len_store_msg_area +=
		snprintf(test_data.store_msg_area, PAGE_SIZE, "ECC, 85, 170, IC Name, %s, IC Code, %x\n",
			 test_data.ini_ic_name, test_data.screen_param.selected_ic);

	/* Add the head part of Line2 */
	len = snprintf(test_data.tmp_buffer, PAGE_SIZE, "TestItem Num, %d, ", test_data.test_data_count);
	memcpy(test_data.store_msg_area + test_data.len_store_msg_area, test_data.tmp_buffer, len);
	test_data.len_store_msg_area += len;

	/* Add other part of Line2, except for "\n" */
	memcpy(test_data.store_msg_area + test_data.len_store_msg_area, test_data.msg_area_line2,
	       test_data.len_msg_area_line2);
	test_data.len_store_msg_area += test_data.len_msg_area_line2;

	/* Add Line3 ~ Line10 */
	len = snprintf(test_data.tmp_buffer, PAGE_SIZE, "\n\n\n\n\n\n\n\n\n");
	memcpy(test_data.store_msg_area + test_data.len_store_msg_area, test_data.tmp_buffer, len);
	test_data.len_store_msg_area += len;

	/* 1.Add Msg Area */
	memcpy(test_data.store_all_data, test_data.store_msg_area, test_data.len_store_msg_area);

	/* 2.Add Data Area */
	if (test_data.len_store_data_area != 0) {
		memcpy(test_data.store_all_data + test_data.len_store_msg_area, test_data.store_data_area,
		       test_data.len_store_data_area);
	}

	FTS_TEST_DBG("lenStoreMsgArea=%d,  lenStoreDataArea = %d", test_data.len_store_msg_area,
		     test_data.len_store_data_area);
}

void init_storeparam_testdata(void)
{
	test_data.testresult_len = 0;

	test_data.len_store_msg_area = 0;
	test_data.len_msg_area_line2 = 0;
	test_data.len_store_data_area = 0;
	/* The Start Line of Data Area is 11 */
	test_data.start_line = 11;
	test_data.test_data_count = 0;
}

int allocate_init_testdata_memory(void)
{
	test_data.testresult = fts_malloc(BUFF_LEN_TESTRESULT_BUFFER);
	if (test_data.testresult == NULL)
		goto ERR;

	test_data.store_all_data = fts_malloc(FTS_TEST_STORE_DATA_SIZE);
	if (test_data.store_all_data == NULL)
		goto ERR;

	test_data.store_msg_area = fts_malloc(BUFF_LEN_STORE_MSG_AREA);
	if (test_data.store_msg_area == NULL)
		goto ERR;

	test_data.msg_area_line2 = fts_malloc(BUFF_LEN_MSG_AREA_LINE2);
	if (test_data.msg_area_line2 == NULL)
		goto ERR;

	test_data.store_data_area = fts_malloc(BUFF_LEN_STORE_DATA_AREA);
	if (test_data.store_data_area == NULL)
		goto ERR;

	test_data.tmp_buffer = fts_malloc(BUFF_LEN_TMP_BUFFER);
	if (test_data.tmp_buffer == NULL)
		goto ERR;

	init_storeparam_testdata();
	return 0;
ERR:
	FTS_TEST_ERROR("fts_malloc memory failed in function.");
	return -ENOMEM;
}

/************************************************************************
* Name: free_testdata_memory
* Brief:  Release pointer memory
* Input: none
* Output: none
* Return: none
***********************************************************************/
void free_testdata_memory(void)
{
	/* free buff */
	if (test_data.testresult  != NULL)
		fts_free(test_data.testresult);

	if (test_data.store_all_data != NULL)
		fts_free(test_data.store_all_data);

	if (test_data.store_msg_area != NULL)
		fts_free(test_data.store_msg_area);

	if (test_data.msg_area_line2 != NULL)
		fts_free(test_data.msg_area_line2);

	if (test_data.store_data_area != NULL)
		fts_free(test_data.store_data_area);

	if (test_data.tmp_buffer != NULL)
		fts_free(test_data.tmp_buffer);

}

int get_tx_rx_num(u8 tx_rx_reg, u8 *ch_num, u8 ch_num_max)
{
	int ret = 0;
	int i = 0;

	for (i = 0; i < 3; i++) {
		ret = read_reg(tx_rx_reg, ch_num);
		if ((ret < 0) || (*ch_num > ch_num_max)) {
			sys_delay(50);
		} else
			break;
	}

	if (i >= 3) {
		FTS_TEST_ERROR("get channel num fail");
		return -EIO;
	}

	return 0;
}

int fts_get_channel_num(void)
{
	int ret = 0;
	u8 tx_num = 0;
	u8 rx_num = 0;

	FTS_TEST_FUNC_ENTER();

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_ERROR("enter factory mode fail, can't get tx/rx num");
		return ret;
	}

	test_data.screen_param.used_max_tx_num = TX_NUM_MAX;
	test_data.screen_param.used_max_rx_num = RX_NUM_MAX;
	test_data.screen_param.key_num_total = KEY_NUM_MAX;
	ret = get_tx_rx_num(FACTORY_REG_CHX_NUM, &tx_num, TX_NUM_MAX);
	if (ret < 0) {
		FTS_TEST_ERROR("get tx_num fail");
		return ret;
	}
#if ((IC_SERIALS == 0x03) || (IC_SERIALS == 0x04))
	ret = get_tx_rx_num(0xA, &rx_num, RX_NUM_MAX);
#else
	ret = get_tx_rx_num(FACTORY_REG_CHY_NUM, &rx_num, RX_NUM_MAX);
#endif
	if (ret < 0) {
		FTS_TEST_ERROR("get rx_num fail");
		return ret;
	}
	test_data.screen_param.tx_num = (int)tx_num;
	test_data.screen_param.rx_num = (int)rx_num;

	FTS_TEST_INFO("TxNum=%d, RxNum=%d, MaxTxNum=%d, MaxRxNum=%d",
		      test_data.screen_param.tx_num,
		      test_data.screen_param.rx_num,
		      test_data.screen_param.used_max_tx_num, test_data.screen_param.used_max_rx_num);

	FTS_TEST_FUNC_EXIT();
	return 0;
}

static int fts_test_init_basicinfo(void)
{
	int ret = 0;
	u8 val = 0;

	ret = read_reg(REG_FW_VERSION, &val);
	FTS_TEST_SAVE_INFO("FW version:0x%02x\n", val);

	ret = read_reg(REG_VA_TOUCH_THR, &val);
	test_data.va_touch_thr = val;
	ret = read_reg(REG_VKEY_TOUCH_THR, &val);
	test_data.key_touch_thr = val;

	/* enter into factory mode and read tx/rx num */
	ret = fts_get_channel_num();
	FTS_TEST_SAVE_INFO("tx_num:%d, rx_num:%d\n",
			   test_data.screen_param.tx_num, test_data.screen_param.rx_num);

	return ret;
}

static int fts_test_main_init(void)
{
	int ret = 0;
	int len = 0;

	FTS_TEST_FUNC_ENTER();
	/* allocate memory for test data:csv&txt */
	ret = allocate_init_testdata_memory();
	if (ret < 0) {
		FTS_TEST_ERROR("allocate memory for test data fail");
		return ret;
	}

	/* get basic information: tx/rx num */
	ret = fts_test_init_basicinfo();
	if (ret < 0) {
		FTS_TEST_ERROR("test init basicinfo fail");
		return ret;
	}

	/* Allocate memory for detail threshold structure */
	ret = malloc_struct_DetailThreshold();
	if (ret < 0) {
		FTS_TEST_ERROR("Failed to malloc memory for detaithreshold");
		return ret;
	}

	/*Allocate test data buffer */
	len = (test_data.screen_param.tx_num + 1) * test_data.screen_param.rx_num;
	test_data.buffer = (int *)fts_malloc(len * sizeof(int));
	if (test_data.buffer == NULL) {
		FTS_TEST_ERROR("test_data.buffer malloc fail");
		return -ENOMEM;
	}
	memset(test_data.buffer, 0, len * sizeof(int));

	FTS_TEST_FUNC_EXIT();
	return ret;
}

/*
 * fts_test_get_testparams - get test parameter from ini
 */
static int fts_test_get_testparams(char *config_name)
{
	int ret = 0;

	ret = fts_test_get_testparam_from_ini(config_name);

	return ret;
}

static bool fts_test_start(void)
{
	bool testresult = false;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_DBG("IC_%s Test", test_data.ini_ic_name);

	if (test_data.func && test_data.func->start_test) {
		testresult = test_data.func->start_test();
	} else {
		FTS_TEST_ERROR("test func/start_test func is null");
		testresult = false;
	}

	FTS_TEST_FUNC_EXIT();
	return testresult;
}

static int fts_test_main_exit(void)
{
	int data_len = 0;
	char testdata_filename[64];
	char testresult_filename[64];

	FTS_TEST_FUNC_ENTER();

	/* Merge test data */
	merge_all_testdata();
	/*Gets the number of tests in the test library and saves it */
	data_len = test_data.len_store_msg_area + test_data.len_store_data_area;
	fts_test_get_save_filename(FTS_TESTDATA_FILE_NAME, testdata_filename, 64);
	fts_test_get_save_filename(FTS_TESTRESULT_FILE_NAME, testresult_filename, 64);
	fts_test_save_test_data(testdata_filename, test_data.store_all_data, data_len);
	fts_test_save_test_data(testresult_filename, test_data.testresult, test_data.testresult_len);

	/* free memory */
	free_struct_DetailThreshold();
	free_testdata_memory();

	/*free test data buffer */
	if (test_data.buffer)
		fts_free(test_data.buffer);

	FTS_TEST_FUNC_EXIT();
	return 0;
}

/*
 * fts_test_entry - test main entry
 *
 * warning - need disable irq & esdcheck before call this function
 *
 */
static int fts_test_entry(char *ini_file_name)
{
	int ret = 0;

	FTS_TEST_FUNC_ENTER();

	/* test initialize */
	ret = fts_test_main_init();
	if (ret < 0) {
		FTS_TEST_ERROR("fts_test_main_init() error.");
		goto TEST_ERR;
	}

	/*Read parse configuration file */
	FTS_TEST_SAVE_INFO("ini_file_name = %s\n", ini_file_name);
	ret = fts_test_get_testparams(ini_file_name);
	if (ret < 0) {
		FTS_TEST_ERROR("get testparam failed");
		goto TEST_ERR;
	}

	/* Start testing according to the test configuration */
	if (fts_test_start() == true) {
		FTS_TEST_SAVE_INFO("\n=======Tp test pass.\n\n");
	} else {
		FTS_TEST_SAVE_INFO("\n=======Tp test failure.\n\n");
	}

	ret = 0;
TEST_ERR:
	enter_work_mode();
	fts_test_main_exit();

	FTS_TEST_FUNC_EXIT();
	return ret;
}

/************************************************************************
* Name: fts_test_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return -EPERM;
}

/************************************************************************
* Name: fts_test_store
* Brief:  upgrade from app.bin
* Input: device, device attribute, char buf, char count
* Output: no
* Return: char count
***********************************************************************/
static ssize_t fts_test_store(struct device *dev, struct device_attribute *attr, const char *buf,
			      size_t count)
{
	char fwname[128] = { 0 };
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev;

	FTS_TEST_FUNC_ENTER();

	if (ts_data->suspended) {
		FTS_INFO("In suspend, no test, return now");
		return -EINVAL;
	}

	input_dev = ts_data->input_dev;
	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, 128, "%s", buf);
	fwname[count - 1] = '\0';
	FTS_TEST_DBG("fwname:%s.", fwname);

	mutex_lock(&input_dev->mutex);
	disable_irq(ts_data->irq);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch(DISABLE);
#endif

	fts_test_entry(fwname);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch(ENABLE);
#endif

	enable_irq(ts_data->irq);
	mutex_unlock(&input_dev->mutex);

	FTS_TEST_FUNC_EXIT();
	return count;
}

/*  test from test.ini
*    example:echo "***.ini" > fts_test
*/
/****************************************************************************
*      zhangjian add for tp self test
****************************************************************************/
u8 rawdata_failed_node_save[MAX_NODE_NUM] = { 0 };

static int fts_test_get_save_filename(char *filename, char *save_filename, int len)
{
	char *board_idFile = { "/persist/factoryinfo/board_id" };
	struct file *pfile = NULL;
	struct inode *inode = NULL;
	char *pValid = NULL;
	/*unsigned long magic; */
	off_t fsize = 0;
	char filepath[128];
	loff_t pos = 0;
	mm_segment_t old_fs;
	char board_id[20];
	struct timespec ts;
	struct rtc_time tm;

	/*get rtc time */
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	memset(filepath, 0, sizeof(filepath));
	snprintf(filepath, sizeof(filepath), "%s", board_idFile);
	if (pfile == NULL) {
		pfile = filp_open(filepath, O_RDONLY, 0);
	}
	if (IS_ERR(pfile)) {
		FTS_TEST_DBG("error occurred while opening file %s.", filepath);
		pValid = strnstr(filename, ".csv", 50);
		if (pValid) {
			snprintf(save_filename, len, "TP_test_data%04d%02d%02d.csv", tm.tm_year + 1900,
				 tm.tm_mon + 1, tm.tm_mday);
		} else {
			pValid = strnstr(filename, ".txt", 50);
			snprintf(save_filename, len, "testresult%04d%02d%02d.txt", tm.tm_year + 1900,
				 tm.tm_mon + 1, tm.tm_mday);
		}
		return -EIO;
	}

	inode = pfile->f_path.dentry->d_inode;
	/*magic = inode->i_sb->s_magic; */
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, board_id, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);
	pValid = strnstr(filename, ".csv", 50);
	if (pValid) {
		snprintf(save_filename, len, "TP_test_data%s.csv", board_id);
	} else {
		pValid = strnstr(filename, ".txt", 50);
		snprintf(save_filename, len, "testresult%s.txt", board_id);
	}
	return 0;

}

static void fts_rawdata_failed_node_init(void)
{
	int i = 0;

	for (i = 0; i < MAX_NODE_NUM; i++)
		rawdata_failed_node_save[i] = 0;
}

static int fts_save_failed_node_to_buffer(char *tmp_buffer, int length)
{

	if (test_data.special_buffer == NULL
	    || (test_data.special_buffer_length + length) > TEST_RESULT_LENGTH) {
		FTS_TEST_INFO("warning:buffer is null or buffer overflow, return");
		return -EPERM;
	}

	memcpy(test_data.special_buffer + test_data.special_buffer_length, tmp_buffer, length);
	test_data.special_buffer_length += length;
	test_data.rawdata_failed_count++;

	return 0;
}

int fts_save_failed_node(int tx_num, int rx_num, int iRow, int iCol)
{
	int i_len = 0;

	if (rawdata_failed_node_save[tx_num * iRow + iCol] == 0) {
		if (test_data.temp_buffer != NULL) {
			i_len = snprintf(test_data.temp_buffer, TEST_TEMP_LENGTH, ",%d,%d", iRow, iCol);
			fts_save_failed_node_to_buffer(test_data.temp_buffer, i_len);
			rawdata_failed_node_save[tx_num * iRow + iCol] = 1;
			return 0;
		} else
			return -EPERM;
	} else
		return 0;
}
void get_ini_file_path(char *ini_file_path)
{
	FTS_TEST_FUNC_ENTER();
	snprintf(ini_file_path, INI_FILE_PATH_MAX_SZIE, "%s", g_str_ini_file_path);
}
static int tpd_test_save_file_path_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev;

	FTS_TEST_FUNC_ENTER();
	input_dev = ts_data->input_dev;
	mutex_lock(&input_dev->mutex);

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", g_str_save_file_path);

	mutex_unlock(&input_dev->mutex);

	return num_read_chars;
}

static int tpd_test_save_file_path_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(g_str_save_file_path, 0, sizeof(g_str_save_file_path));
	snprintf(g_str_save_file_path, 256, "%s", buf);
	/*g_str_save_file_path[count] = '\0'; */

	FTS_TEST_DBG("save file path:%s.", g_str_save_file_path);

	return 0;
}

static int tpd_test_ini_file_path_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	FTS_TEST_FUNC_ENTER();
	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", g_str_ini_file_path);

	return num_read_chars;
}

static int tpd_test_ini_file_path_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(g_str_ini_file_path, 0, sizeof(g_str_ini_file_path));
	snprintf(g_str_ini_file_path, 256, "%s", buf);
	/*g_str_ini_file_path[count] = '\0'; */

	FTS_TEST_DBG("ini file path:%s.", g_str_ini_file_path);

	return 0;
}

static int tpd_test_filename_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	FTS_TEST_FUNC_ENTER();
	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", g_str_ini_filename);

	return num_read_chars;
}

static int tpd_test_filename_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(g_str_ini_filename, 0, sizeof(g_str_ini_filename));
	snprintf(g_str_ini_filename, 128, "%s", buf);
	/*g_str_ini_filename[count] = '\0'; */

	FTS_TEST_DBG("fwname:%s.", g_str_ini_filename);

	return 0;
}

static int tpd_test_cmd_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev;
	int buffer_length = 0;
	int i_len = 0;
	int ret = 0;

	FTS_TEST_FUNC_ENTER();
	input_dev = ts_data->input_dev;
	mutex_lock(&input_dev->mutex);
	disable_irq(ts_data->irq);
	ret = fts_get_channel_num();
	enter_work_mode();
	i_len = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d", g_int_tptest_result,
			 test_data.screen_param.tx_num, test_data.screen_param.rx_num,
			 test_data.rawdata_failed_count);
	FTS_TEST_INFO("tpd test resutl:0x%x && rawdata node failed count:0x%x.\n", g_int_tptest_result,
		      test_data.rawdata_failed_count);
	FTS_TEST_INFO("tpd resutl && failed cout string length:0x%x.\n", i_len);

	buffer_length = (test_data.special_buffer_length + 1) >
			(PAGE_SIZE - i_len) ? (PAGE_SIZE - i_len - 1) : test_data.special_buffer_length;
	FTS_TEST_INFO("tpd failed node string length:0x%x, buffer_length:0x%x.\n",
		      test_data.special_buffer_length, buffer_length);

	if (test_data.special_buffer != NULL && buffer_length > 0) {
		memcpy(buf + i_len, test_data.special_buffer, buffer_length);
		buf[buffer_length + i_len] = '\0';
	}

	FTS_TEST_INFO("tpd  test:%s.\n", buf);

	num_read_chars = buffer_length + i_len;

	enable_irq(ts_data->irq);
	mutex_unlock(&input_dev->mutex);

	return num_read_chars;
}

static int tpd_test_cmd_store(struct tpd_classdev_t *cdev, const char *buf)
{
	unsigned long command = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev;
	int retval = -1;

	FTS_TEST_FUNC_ENTER();
	input_dev = ts_data->input_dev;
	FTS_TEST_DBG("%s fwname:%s.", __func__, g_str_ini_filename);
	retval = kstrtoul(buf, 10, &command);
	if (retval) {
		FTS_TEST_DBG("invalid param:%s", buf);
		return 0;
	}
	mutex_lock(&input_dev->mutex);
	if (command == 1) {
		test_data.special_buffer = NULL;

		if (test_data.special_buffer == NULL) {
			test_data.special_buffer = (char *)fts_malloc(TEST_RESULT_LENGTH);
			test_data.special_buffer_length = 0;
			test_data.rawdata_failed_count = 0;
		}
		test_data.temp_buffer = NULL;
		if (test_data.temp_buffer == NULL) {
			test_data.temp_buffer = (char *)fts_malloc(TEST_TEMP_LENGTH);
		}
		fts_rawdata_failed_node_init();
		test_data.node_opened = 1;
	} else if (command == 2) {
		if (test_data.node_opened == 1) {
			disable_irq(ts_data->irq);
			g_int_tptest_result = 0;
			fts_test_entry(g_str_ini_filename);
			enable_irq(ts_data->irq);
		} else {
			FTS_TEST_DBG("command:%ld, but node not opened.", command);
		}
	} else if (command == 3) {
		test_data.node_opened = 0;
		if (test_data.special_buffer != NULL) {
			fts_free(test_data.special_buffer);
			test_data.special_buffer = NULL;
			test_data.special_buffer_length = 0;
			test_data.rawdata_failed_count = 0;
		}
		if (test_data.temp_buffer != NULL) {
			fts_free(test_data.temp_buffer);
			test_data.temp_buffer = NULL;
		}
	} else {
		FTS_TEST_DBG("invalid command %ld", command);
	}

	mutex_unlock(&input_dev->mutex);
	return 0;
}

static int tpd_test_channel_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	int ret = 0;

	ret = fts_get_channel_num();
	enter_work_mode();
	num_read_chars =
		snprintf(buf, PAGE_SIZE, "%d %d", test_data.screen_param.tx_num, test_data.screen_param.rx_num);

	return num_read_chars;
}

static int tpd_test_result_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "0x%x", g_int_tptest_result);

	return num_read_chars;
}
static int fts_get_tp_rawdata(char *buffer, int length)
{
	int retval = 0;

	/* Get test functin */
	if (FTS_CHIP_TYPE == _FT3C47U)
		test_data.func = &test_func_ft3c47;
	else if (FTS_CHIP_TYPE == _FT3D47)
		test_data.func = &test_func_ft3d47;
	else if (FTS_CHIP_TEST_TYPE == FT5822_TEST)
		test_data.func = &test_func_ft5822;
	else if (FTS_CHIP_TEST_TYPE == FT5X46_TEST)
		test_data.func = &test_func_ft5x46;
	else if (FTS_CHIP_TEST_TYPE == FT5452_TEST)
		test_data.func = &test_func_ft5452;
	else if (FTS_CHIP_TEST_TYPE == FT6X36_TEST)
		test_data.func = &test_func_ft6x36;
	else if (FTS_CHIP_TYPE == _FT8006M)
		test_data.func = &test_func_ft8006m;
	else if (FTS_CHIP_TYPE == _FT8006U)
		test_data.func = &test_func_ft8006u;
	else if (FTS_CHIP_TYPE == _FT8201)
		test_data.func = &test_func_ft8201;
	else if (FTS_CHIP_TYPE == _FT8606)
		test_data.func = &test_func_ft8606;
	else if (FTS_CHIP_TYPE == _FT8607)
		test_data.func = &test_func_ft8607;
	else if (FTS_CHIP_TYPE == _FT8716)
		test_data.func = &test_func_ft8716;
	else if (FTS_CHIP_TYPE == _FT8736)
		test_data.func = &test_func_ft8736;
	else if (FTS_CHIP_TYPE == _FTE716)
		test_data.func = &test_func_fte716;
	else if (FTS_CHIP_TYPE == _FT8719)
		test_data.func = &test_func_ft8719;
	else if (FTS_CHIP_TYPE == _FT8739)
		test_data.func = &test_func_ft8739;
	else {
		FTS_TEST_ERROR("no test_data.func match");
		return retval;
	}
	if (test_data.func && test_data.func->get_tp_rawdata) {
		retval  = test_data.func->get_tp_rawdata(buffer, length);
		enter_work_mode();
	} else {
		FTS_ERROR("test func/get_tp_rawdata  func is null");
	}
	return retval;
}

static int fts_test_init_fw_class(void)
{
	FTS_TEST_FUNC_ENTER();
	tpd_fw_cdev.tpd_test_set_save_filepath = tpd_test_save_file_path_store;
	tpd_fw_cdev.tpd_test_get_save_filepath = tpd_test_save_file_path_show;
	tpd_fw_cdev.tpd_test_set_ini_filepath = tpd_test_ini_file_path_store;
	tpd_fw_cdev.tpd_test_get_ini_filepath = tpd_test_ini_file_path_show;
	tpd_fw_cdev.tpd_test_set_filename = tpd_test_filename_store;
	tpd_fw_cdev.tpd_test_get_filename = tpd_test_filename_show;
	tpd_fw_cdev.tpd_test_set_cmd = tpd_test_cmd_store;
	tpd_fw_cdev.tpd_test_get_cmd = tpd_test_cmd_show;
	tpd_fw_cdev.tpd_test_get_channel_info = tpd_test_channel_show;
	tpd_fw_cdev.tpd_test_get_result = tpd_test_result_show;
	tpd_fw_cdev.tpd_test_get_tp_rawdata = fts_get_tp_rawdata;
	strlcpy(g_str_save_file_path, FTS_INI_FILE_PATH, 256);
	strlcpy(g_str_ini_file_path, FTS_INI_FILE_PATH, 256);
	strlcpy(g_str_ini_filename, "test.ini", 128);
	FTS_TEST_FUNC_EXIT();
	return 0;
}

/******************add for tp test end**************************************/
static DEVICE_ATTR(fts_test, 0644, fts_test_show, fts_test_store);

/* add your attr in here*/
static struct attribute *fts_test_attributes[] = {
	&dev_attr_fts_test.attr,
	NULL
};

static struct attribute_group fts_test_attribute_group = {
	.attrs = fts_test_attributes
};

int fts_test_init(struct i2c_client *client)
{
	int ret = 0;

	FTS_TEST_FUNC_ENTER();
	fts_test_init_fw_class();
	ret = sysfs_create_group(&client->dev.kobj, &fts_test_attribute_group);
	if (ret != 0) {
		FTS_TEST_ERROR("[focal] %s() - ERROR: sysfs_create_group() failed.", __func__);
		sysfs_remove_group(&client->dev.kobj, &fts_test_attribute_group);
	} else {
		FTS_TEST_DBG("[focal] %s() - sysfs_create_group() succeeded.", __func__);
	}

	FTS_TEST_FUNC_EXIT();

	return ret;
}

int fts_test_exit(struct i2c_client *client)
{
	FTS_TEST_FUNC_ENTER();

	sysfs_remove_group(&client->dev.kobj, &fts_test_attribute_group);

	FTS_TEST_FUNC_EXIT();
	return 0;
}
