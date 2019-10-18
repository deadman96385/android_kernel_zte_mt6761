
/********************************* (C) COPYRIGHT 2018 STMicroelectronics ********************************
 *
 * File Name		: n2dm.h
 * Authors		: William ZENG
 * Version		: V1.0.3
 * Date			: 07/24/2018
 * Description	: N2DM driver source file
 *
 *********************************************************************************************************
  * Copyright (c) 2018, STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *********************************************************************************************************
 * REVISON HISTORY
 *
 * VERSION | DATE		| DESCRIPTION
 *
 * 1.0.2   | 03/05/2018	| modified driver to be compatible with Android O
 * 1.0.3   | 07/24/2018	| optimized calibration functions
 *
 ****************************************************************************************************/

#ifndef N2DM_H
#define N2DM_H

#include <linux/ioctl.h>
#include <accel.h>
#include <hwmsensor.h>
#include <cust_acc.h>

#define POWER_NONE_MACRO MT65XX_POWER_NONE

/* N2DM Register Map  (Please refer to N2DM Specifications) */
#define N2DM_REG_WHO_AM_I		0x0F
#define N2DM_REG_CTL_REG1		0x20
#define N2DM_REG_CTL_REG2		0x21
#define N2DM_REG_CTL_REG3		0x22
#define N2DM_REG_CTL_REG4		0x23
#define N2DM_REG_DATAX0			0x28
#define N2DM_REG_OUT_X			0x28
#define N2DM_REG_OUT_Y			0x2A
#define N2DM_REG_OUT_Z			0x2C


#define N2DM_FIXED_DEVID		0x33

#define N2DM_BW_200HZ			0x60
#define N2DM_BW_100HZ			0x50
#define N2DM_BW_50HZ			0x40
#define N2DM_BW_0HZ			0x00

#define	N2DM_FULLRANG_LSB		0XFF

#define N2DM_MEASURE_MODE		0x08
#define N2DM_DATA_READY			0x07

/*#define N2DM_FULL_RES			0x08*/
#define N2DM_RANGE_2G			0x00
#define N2DM_RANGE_4G			0x10
#define N2DM_RANGE_8G			0x20
#define N2DM_RANGE_16G			0x30

#define N2DM_SELF_TEST			0x10

#define N2DM_STREAM_MODE		0x80
#define N2DM_SAMPLES_15			0x0F

#define N2DM_FS_8G_LSB_G		0x20
#define N2DM_FS_4G_LSB_G		0x10
#define N2DM_FS_2G_LSB_G		0x00

#define N2DM_LEFT_JUSTIFY		0x04
#define N2DM_RIGHT_JUSTIFY		0x00

#define N2DM_SUCCESS					0
#define N2DM_ERR_I2C					-1
#define N2DM_ERR_STATUS					-3
#define N2DM_ERR_SETUP_FAILURE			-4
#define N2DM_ERR_GETGSENSORDATA			-5
#define N2DM_ERR_IDENTIFICATION			-6

#define N2DM_BUFSIZE			256

#define N2DM_AXIS_X		0
#define N2DM_AXIS_Y		1
#define N2DM_AXIS_Z		2
#define N2DM_AXES_NUM		3
#define N2DM_DATA_LEN		6
#define N2DM_DEV_NAME		"N2DM"

/* #define CONFIG_N2DM_LOWPASS   //apply low pass filter on output*/

#define CONFIG_N2DM_ACC_DRY

/*----------------------------------------------------------------------------*/
typedef enum {
	ADX_TRC_FILTER  = 0x01,
	ADX_TRC_RAWDATA = 0x02,
	ADX_TRC_IOCTL   = 0x04,
	ADX_TRC_CALI	= 0X08,
	ADX_TRC_INFO	= 0X10,
} ADX_TRC;
/*----------------------------------------------------------------------------*/
struct scale_factor {
	u8  whole;
	u8  fraction;
};
/*----------------------------------------------------------------------------*/
struct data_resolution {
	struct scale_factor scalefactor;
	int				sensitivity;
};
/*----------------------------------------------------------------------------*/
#define C_MAX_FIR_LENGTH (32)
/*----------------------------------------------------------------------------*/
struct data_filter {
	s16 raw[C_MAX_FIR_LENGTH][N2DM_AXES_NUM];
	int sum[N2DM_AXES_NUM];
	int num;
	int idx;
};

/*----------------------------------------------------------------------------*/
struct n2dm_acc {
	/*struct i2c_client *client;*/
	struct acc_hw *n2dm_acc_hw;
	struct hwmsen_convert   cvt;

	/*misc*/
	struct data_resolution *reso;
	atomic_t				trace;
	atomic_t				suspend;
	atomic_t				selftest;
	atomic_t				filter;
	s32					cali_sw[N2DM_AXES_NUM];

	/*data*/
	s32					offset[N2DM_AXES_NUM];
	s16					data[N2DM_AXES_NUM];
	bool					n2dm_acc_power;
	int					odr;
	int					enabled;
#if defined(CONFIG_N2DM_LOWPASS)
	atomic_t				firlen;
	atomic_t				fir_en;
	struct data_filter	fir;
#endif
};

struct n2dm_data {
	struct i2c_client *client;
	struct n2dm_acc n2dm_acc_data;
	u8	reg_addr;
};

#define ST_TAG				"[ST] "
#define ST_ERR(fmt, args...)	pr_err(ST_TAG "%s %d : "fmt, __func__, __LINE__, ##args)
#define ST_LOG(fmt, args...)	pr_info(ST_TAG fmt, ##args)

#if defined(DEBUG)
#define ST_FUN(f)			pr_info(ST_TAG "%s\n", __func__)
#define ST_DBG(fmt, args...)	pr_err(ST_TAG fmt, ##args)
#else
#define ST_FUN(f)
#define ST_DBG(fmt, args...)
#endif

int n2dm_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len);
int n2dm_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len);

void dumpReg(struct n2dm_data *obj);
int n2dm_set_interrupt(struct n2dm_data *obj, u8 intenable);

int n2dm_acc_set_power_mode(struct n2dm_acc *acc_obj, bool enable);
int n2dm_acc_init(struct n2dm_acc *acc_obj, int reset_cali);

/*extern struct i2c_client *n2dm_i2c_client;*/
extern struct n2dm_data *obj_i2c_data;
extern int sensor_suspend;
extern struct acc_init_info n2dm_acc_init_info;
extern int n2dm_acc_init_flag;

#endif
