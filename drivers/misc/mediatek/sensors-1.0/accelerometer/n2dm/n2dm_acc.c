/* N2DM driver
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

#include "n2dm.h"

/*----------------------------------------------------------------------------*/
static struct data_resolution n2dm_data_resolution[] = {
	/* combination by {FULL_RES,RANGE}*/
	{{ 1, 0}, 1024},/* dataformat +/-2g in 12-bit resolution;*/
					/*{ 1, 0} = 1.0 = (2*2*1000)/(2^12);  1024 = (2^12)/(2*2)*/
	{{ 1, 9}, 512},	/* dataformat +/-4g  in 12-bit resolution;*/
					/*{ 1, 9} = 1.9 = (2*4*1000)/(2^12);  512 = (2^12)/(2*4)*/
	{{ 3, 9}, 256},	/* dataformat +/-8g  in 12-bit resolution;*/
					/*{ 3, 9} = 3.9 = (2*8*1000)/(2^12);  256 = (2^12)/(2*8)*/
};
/*----------------------------------------------------------------------------*/
static struct data_resolution n2dm_offset_resolution = {{15, 6}, 64};
static struct GSENSOR_VECTOR3D gsensor_gain;
#ifdef MISC_DEVICE_FACTORY
static struct GSENSOR_VECTOR3D gsensor_offset;
#endif
struct acc_hw n2dm_cust_hw;
int n2dm_acc_init_flag = -1; /* 0<==>OK -1 <==> fail
*/
/*----------------------------------------------------------------------------*/


/*For driver get cust info*/
struct acc_hw *n2dm_get_cust_acc_hw(void)
{
	return &n2dm_cust_hw;
}

/*----------------------------------------------------------------------------*/
static int n2dm_acc_set_resolution(struct n2dm_acc *acc_obj)
{
	struct n2dm_data *obj = container_of(acc_obj, struct n2dm_data, n2dm_acc_data);
	struct i2c_client *client = obj->client;

	int err;
	u8  dat, reso;

	err = n2dm_i2c_read_block(client, N2DM_REG_CTL_REG4, &dat, 0x01);

	if (err < 0) {
		ST_ERR("write data format fail!!\n");
		return err;
	}

	/*the data_reso is combined by 3 bits: {FULL_RES, DATA_RANGE}*/
	reso = (dat & 0x30) >> 4;
	if (reso >= 0x3)
		reso = 0x2;


	if (reso < ARRAY_SIZE(n2dm_data_resolution)) {
		acc_obj->reso = &n2dm_data_resolution[reso];
		return 0;
	} else {
		return -EINVAL;
	}
}
/*----------------------------------------------------------------------------*/
static int n2dm_acc_read_rawdata(struct n2dm_acc *acc_obj, s16 data[N2DM_AXES_NUM])
{

	struct n2dm_data *obj = container_of(acc_obj, struct n2dm_data, n2dm_acc_data);
	struct i2c_client *client = obj->client;
	u8 buf[N2DM_DATA_LEN] = {0};
	int err = 0;

	if (client == NULL) {
		err = -EINVAL;
	} else {
		if ((n2dm_i2c_read_block(client, N2DM_REG_OUT_X, buf, 0x01)) < 0) {
			ST_ERR("read  G sensor data register err!\n");
			return -EPERM;
		}

		if ((n2dm_i2c_read_block(client, N2DM_REG_OUT_X + 1, &buf[1], 0x01)) < 0) {
			ST_ERR("read  G sensor data register err!\n");
			return -EPERM;
		}

		if ((n2dm_i2c_read_block(client, N2DM_REG_OUT_Y, &buf[2], 0x01)) < 0) {
			ST_ERR("read  G sensor data register err!\n");
			return -EPERM;
		}
		if ((n2dm_i2c_read_block(client, N2DM_REG_OUT_Y + 1, &buf[3], 0x01)) < 0) {
			ST_ERR("read  G sensor data register err!\n");
			return -EPERM;
		}

		if ((n2dm_i2c_read_block(client, N2DM_REG_OUT_Z, &buf[4], 0x01)) < 0) {
			ST_ERR("read  G sensor data register err!\n");
			return -EPERM;
		}

		if ((n2dm_i2c_read_block(client, N2DM_REG_OUT_Z + 1, &buf[5], 0x01)) < 0) {
			ST_ERR("read  G sensor data register err!\n");
			return -EPERM;
		}

		data[N2DM_AXIS_X] = ((s16)(((buf[1] << 8) | buf[0]))) >> 4;
		data[N2DM_AXIS_Y] = ((s16)(((buf[3] << 8) | buf[2]))) >> 4;
		data[N2DM_AXIS_Z] = ((s16)(((buf[5] << 8) | buf[4]))) >> 4;

		if (atomic_read(&acc_obj->trace) & ADX_TRC_RAWDATA) {
			ST_LOG("[%08X %08X %08X] => [%5d %5d %5d]\n",
					data[N2DM_AXIS_X], data[N2DM_AXIS_Y],
					data[N2DM_AXIS_Z], data[N2DM_AXIS_X],
					data[N2DM_AXIS_Y], data[N2DM_AXIS_Z]);
		}

		if (atomic_read(&acc_obj->trace) & ADX_TRC_RAWDATA) {
			ST_LOG("[%08X %08X %08X] => [%5d %5d %5d] after\n", data[N2DM_AXIS_X],
					data[N2DM_AXIS_Y], data[N2DM_AXIS_Z],
					data[N2DM_AXIS_X], data[N2DM_AXIS_Y], data[N2DM_AXIS_Z]);
		}

#ifdef CONFIG_N2DM_LOWPASS
		if (atomic_read(&acc_obj->filter)) {
			if (atomic_read(&acc_obj->fir_en) && !atomic_read(&acc_obj->suspend)) {
				int idx, firlen = atomic_read(&acc_obj->firlen);

				if (acc_obj->fir.num < firlen) {
					acc_obj->fir.raw[acc_obj->fir.num][N2DM_AXIS_X] = data[N2DM_AXIS_X];
					acc_obj->fir.raw[acc_obj->fir.num][N2DM_AXIS_Y] = data[N2DM_AXIS_Y];
					acc_obj->fir.raw[acc_obj->fir.num][N2DM_AXIS_Z] = data[N2DM_AXIS_Z];
					acc_obj->fir.sum[N2DM_AXIS_X] += data[N2DM_AXIS_X];
					acc_obj->fir.sum[N2DM_AXIS_Y] += data[N2DM_AXIS_Y];
					acc_obj->fir.sum[N2DM_AXIS_Z] += data[N2DM_AXIS_Z];
					if (atomic_read(&acc_obj->trace) & ADX_TRC_FILTER) {
						ST_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n", acc_obj->fir.num,
						acc_obj->fir.raw[acc_obj->fir.num][N2DM_AXIS_X],
						acc_obj->fir.raw[acc_obj->fir.num][N2DM_AXIS_Y],
						acc_obj->fir.raw[acc_obj->fir.num][N2DM_AXIS_Z],
						acc_obj->fir.sum[N2DM_AXIS_X],
						acc_obj->fir.sum[N2DM_AXIS_Y],
						acc_obj->fir.sum[N2DM_AXIS_Z]);
					}
					acc_obj->fir.num++;
					acc_obj->fir.idx++;
				} else {
					idx = acc_obj->fir.idx % firlen;
					acc_obj->fir.sum[N2DM_AXIS_X] -= acc_obj->fir.raw[idx][N2DM_AXIS_X];
					acc_obj->fir.sum[N2DM_AXIS_Y] -= acc_obj->fir.raw[idx][N2DM_AXIS_Y];
					acc_obj->fir.sum[N2DM_AXIS_Z] -= acc_obj->fir.raw[idx][N2DM_AXIS_Z];
					acc_obj->fir.raw[idx][N2DM_AXIS_X] = data[N2DM_AXIS_X];
					acc_obj->fir.raw[idx][N2DM_AXIS_Y] = data[N2DM_AXIS_Y];
					acc_obj->fir.raw[idx][N2DM_AXIS_Z] = data[N2DM_AXIS_Z];
					acc_obj->fir.sum[N2DM_AXIS_X] += data[N2DM_AXIS_X];
					acc_obj->fir.sum[N2DM_AXIS_Y] += data[N2DM_AXIS_Y];
					acc_obj->fir.sum[N2DM_AXIS_Z] += data[N2DM_AXIS_Z];
					acc_obj->fir.idx++;
					data[N2DM_AXIS_X] = acc_obj->fir.sum[N2DM_AXIS_X] / firlen;
					data[N2DM_AXIS_Y] = acc_obj->fir.sum[N2DM_AXIS_Y] / firlen;
					data[N2DM_AXIS_Z] = acc_obj->fir.sum[N2DM_AXIS_Z] / firlen;
					if (atomic_read(&acc_obj->trace) & ADX_TRC_FILTER) {
						ST_LOG("add[%2d][%5d %5d %5d]=>[%5d %5d %5d]:[%5d %5d %5d]\n", idx,
							acc_obj->fir.raw[idx][N2DM_AXIS_X],
							acc_obj->fir.raw[idx][N2DM_AXIS_Y],
							acc_obj->fir.raw[idx][N2DM_AXIS_Z],
							acc_obj->fir.sum[N2DM_AXIS_X],
							acc_obj->fir.sum[N2DM_AXIS_Y],
							acc_obj->fir.sum[N2DM_AXIS_Z],
							data[N2DM_AXIS_X],
							data[N2DM_AXIS_Y],
							data[N2DM_AXIS_Z]);
					}
				}
			}
		}
#endif
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int n2dm_acc_reset_calibration(struct n2dm_acc *acc_obj)
{
	memset(acc_obj->cali_sw, 0x00, sizeof(acc_obj->cali_sw));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int n2dm_acc_read_calibration(struct n2dm_acc *acc_obj, int dat[N2DM_AXES_NUM])
{
	dat[acc_obj->cvt.map[N2DM_AXIS_X]] = acc_obj->cvt.sign[N2DM_AXIS_X] * acc_obj->cali_sw[N2DM_AXIS_X];
	dat[acc_obj->cvt.map[N2DM_AXIS_Y]] = acc_obj->cvt.sign[N2DM_AXIS_Y] * acc_obj->cali_sw[N2DM_AXIS_Y];
	dat[acc_obj->cvt.map[N2DM_AXIS_Z]] = acc_obj->cvt.sign[N2DM_AXIS_Z] * acc_obj->cali_sw[N2DM_AXIS_Z];

	return 0;
}
/*----------------------------------------------------------------------------*/
static int n2dm_acc_write_calibration(struct n2dm_acc *acc_obj, int dat[N2DM_AXES_NUM])
{
	int err = 0;

	ST_FUN();

	if (!acc_obj || !dat) {
		ST_ERR("null ptr!!\n");
		return -EINVAL;
	}
	acc_obj->cali_sw[N2DM_AXIS_X] += acc_obj->cvt.sign[N2DM_AXIS_X] *
							dat[acc_obj->cvt.map[N2DM_AXIS_X]];
	acc_obj->cali_sw[N2DM_AXIS_Y] += acc_obj->cvt.sign[N2DM_AXIS_Y] *
							dat[acc_obj->cvt.map[N2DM_AXIS_Y]];
	acc_obj->cali_sw[N2DM_AXIS_Z] += acc_obj->cvt.sign[N2DM_AXIS_Z] *
							dat[acc_obj->cvt.map[N2DM_AXIS_Z]];

	return err;
}
/*----------------------------------------------------------------------------*/
static int n2dm_acc_set_full_scale(struct n2dm_acc *acc_obj, u8 dataformat)
{
	struct n2dm_data *obj = container_of(acc_obj, struct n2dm_data, n2dm_acc_data);
	struct i2c_client *client = obj->client;
	u8 databuf[10];
	u8 addr = N2DM_REG_CTL_REG4;
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);

	if ((n2dm_i2c_read_block(client, addr, databuf, 0x01)) < 0) {
		ST_ERR("read reg_ctl_reg1 register err!\n");
		return N2DM_ERR_I2C;
	}

	databuf[0] &= ~0x30;
	databuf[0] |= dataformat;

	res = n2dm_i2c_write_block(client, N2DM_REG_CTL_REG4, databuf, 0x1);
	if (res < 0) {
		return N2DM_ERR_I2C;
	}

	return n2dm_acc_set_resolution(acc_obj);
}
/*----------------------------------------------------------------------------*/
static int n2dm_acc_set_odr(struct n2dm_acc *acc_obj, u8 bwrate)
{
	struct n2dm_data *obj = container_of(acc_obj, struct n2dm_data, n2dm_acc_data);
	struct i2c_client *client = obj->client;

	u8 databuf[10];
	u8 addr = N2DM_REG_CTL_REG1;
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);

	if ((n2dm_i2c_read_block(client, addr, databuf, 0x01)) < 0) {
		ST_ERR("read reg_ctl_reg1 register err!\n");
		return N2DM_ERR_I2C;
	}

	databuf[0] &= ~0xF0;
	databuf[0] |= bwrate;


	res = n2dm_i2c_write_block(client, N2DM_REG_CTL_REG1, databuf, 0x1);

	if (res < 0) {
		return N2DM_ERR_I2C;
	}
	acc_obj->odr = bwrate;

	return N2DM_SUCCESS;
}
/*----------------------------------------------------------------------------*/
int n2dm_acc_set_power_mode(struct n2dm_acc *acc_obj, bool state)
{
	u8 databuf[2];
	int res = 0;

	if (state == acc_obj->n2dm_acc_power) {
		ST_LOG("Sensor power status is newest!\n");
		return N2DM_SUCCESS;
	}

	if (state == true) {
		if (acc_obj->odr == 0)
			acc_obj->odr = N2DM_BW_100HZ;

		res = n2dm_acc_set_odr(acc_obj, acc_obj->odr);
	} else {
		res = n2dm_acc_set_odr(acc_obj, N2DM_BW_0HZ);
	}

	if (res < 0) {
		ST_LOG("set power mode failed!\n");
		return N2DM_ERR_I2C;
	} else if (atomic_read(&acc_obj->trace) & ADX_TRC_INFO) {
		ST_LOG("set power mode ok %d!\n", databuf[1]);
	}

	acc_obj->n2dm_acc_power = state;

	return N2DM_SUCCESS;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
int n2dm_acc_init(struct n2dm_acc *acc_obj, int reset_cali)
{
	struct n2dm_data *obj = container_of(acc_obj, struct n2dm_data, n2dm_acc_data);
	struct i2c_client *client = obj->client;

	int res = 0;
	u8 databuf[2] = {0, 0};

	/* first clear reg1
*/
	databuf[0] = 0x0f;
	res = n2dm_i2c_write_block(client, N2DM_REG_CTL_REG1, databuf, 0x01);
	if (res < 0) {
		ST_ERR("n2dm_acc_init step 1!\n");
		return res;
	}

	acc_obj->odr = 0;
	res = n2dm_acc_set_odr(acc_obj, N2DM_BW_0HZ);/*power down
*/
	if (res < 0) {
		ST_ERR("n2dm_acc_init step 2!\n");
		return res;
	}

	res = n2dm_acc_set_full_scale(acc_obj, N2DM_RANGE_2G); /*8g or 2G no oher choise
*/
	if (res < 0) {
		ST_ERR("n2dm_acc_init step 3!\n");
		return res;
	}
	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = acc_obj->reso->sensitivity;

#ifdef CONFIG_N2DM_ACC_DRY
	res = n2dm_set_interrupt(obj, true);
	if (res < 0) {
		ST_ERR("n2dm_acc_init step 4!\n");
		return res;
	}
#endif

	/*res = n2dm_acc_set_power_mode(acc_obj, false);
*/
	/*if(res < 0)
*/
	/*{
*/
	/*	ST_ERR("n2dm_acc_init step 5!\n");
*/
	/*	return res;
*/
	/*}
*/

	if (reset_cali != 0) {
		/*reset calibration only in power on
*/
		res = n2dm_acc_reset_calibration(acc_obj);
		if (res < 0) {
			return res;
		}
	}

#ifdef CONFIG_N2DM_LOWPASS
	memset(&acc_obj->fir, 0x00, sizeof(acc_obj->fir));
#endif

	return N2DM_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int n2dm_read_chip_name(struct n2dm_data *obj, u8 *buf, int bufsize)
{
	struct i2c_client *client = obj->client;

	u8 databuf[10];

	memset(databuf, 0, sizeof(u8) * 10);

	if ((buf == NULL) || (bufsize <= 30)) {
		return -EPERM;
	}

	if (client == NULL) {
		*buf = 0;
		return -ENOENT;
	}

	snprintf(buf, 64, "N2DM Chip");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int n2dm_acc_read_data(struct n2dm_acc *acc_obj, u8 *buf, int bufsize)
{
	struct n2dm_data *obj = container_of(acc_obj, struct n2dm_data, n2dm_acc_data);
	struct i2c_client *client = obj->client;
	u8 databuf[20];
	int acc[N2DM_AXES_NUM] = {0};
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);

	if (buf == NULL) {
		return -EPERM;
	}
	if (client == NULL) {
		*buf = 0;
		return -ENOENT;
	}

	if (atomic_read(&acc_obj->suspend)) {
		ST_LOG("sensor in suspend read not data!\n");
		return 0;
	}
#if 0
	if (acc_obj->n2dm_acc_power == FALSE) {
		res = n2dm_acc_set_power_mode(client, true);
		if (res) {
			ST_ERR("Power on n2dm error %d!\n", res);
		}
		msleep(20);
	}
#endif
	res = n2dm_acc_read_rawdata(acc_obj, acc_obj->data);
	if (res) {
		ST_ERR("I2C error: ret value=%d", res);
		return -ESRCH;
	}
	/*Out put the mg*/
	acc_obj->data[N2DM_AXIS_X] = acc_obj->data[N2DM_AXIS_X] *
					GRAVITY_EARTH_1000 / acc_obj->reso->sensitivity;
	acc_obj->data[N2DM_AXIS_Y] = acc_obj->data[N2DM_AXIS_Y] *
					GRAVITY_EARTH_1000 / acc_obj->reso->sensitivity;
	acc_obj->data[N2DM_AXIS_Z] = acc_obj->data[N2DM_AXIS_Z] *
					GRAVITY_EARTH_1000 / acc_obj->reso->sensitivity;
	acc_obj->data[N2DM_AXIS_X] += acc_obj->cali_sw[N2DM_AXIS_X];
	acc_obj->data[N2DM_AXIS_Y] += acc_obj->cali_sw[N2DM_AXIS_Y];
	acc_obj->data[N2DM_AXIS_Z] += acc_obj->cali_sw[N2DM_AXIS_Z];

	/*remap coordinate*/
	acc[acc_obj->cvt.map[N2DM_AXIS_X]] = acc_obj->cvt.sign[N2DM_AXIS_X] * acc_obj->data[N2DM_AXIS_X];
	acc[acc_obj->cvt.map[N2DM_AXIS_Y]] = acc_obj->cvt.sign[N2DM_AXIS_Y] * acc_obj->data[N2DM_AXIS_Y];
	acc[acc_obj->cvt.map[N2DM_AXIS_Z]] = acc_obj->cvt.sign[N2DM_AXIS_Z] * acc_obj->data[N2DM_AXIS_Z];

	snprintf(buf, 512, "%04x %04x %04x", acc[N2DM_AXIS_X], acc[N2DM_AXIS_Y], acc[N2DM_AXIS_Z]);
	if (atomic_read(&acc_obj->trace) & ADX_TRC_IOCTL) {
		/*atomic_read(&obj->trace) & ADX_TRC_IOCTL*/
		ST_LOG("gsensor data: %s!\n", buf);
		dumpReg(obj);
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static int n2dm_acc_read_rawdata_string(struct n2dm_acc *acc_obj, u8 *buf)
{
	struct n2dm_data *obj = container_of(acc_obj, struct n2dm_data, n2dm_acc_data);
	struct i2c_client *client = obj->client;
	int res = 0;

	if (!buf || !client) {
		return -EINVAL;
	}

	res = n2dm_acc_read_rawdata(acc_obj, acc_obj->data);
	if (res) {
		ST_ERR("I2C error: ret value=%d", res);
		return -EIO;
	}
	snprintf(buf, 512, "%04x %04x %04x", acc_obj->data[N2DM_AXIS_X],
		acc_obj->data[N2DM_AXIS_Y], acc_obj->data[N2DM_AXIS_Z]);

	return 0;
}


/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	/*struct i2c_client *client = n2dm_i2c_client;
*/
	struct n2dm_data *obj = obj_i2c_data;
	u8 strbuf[N2DM_BUFSIZE];

	if (obj->client == NULL) {
		ST_ERR("i2c client is null!!\n");
		return 0;
	}
	n2dm_read_chip_name(obj, strbuf, N2DM_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_show_chipid_value(struct device_driver *ddri, char *buf)
{
	struct n2dm_data *obj = obj_i2c_data;
	u8 strbuf[N2DM_BUFSIZE] = "unknown chipid";

	if (obj->client == NULL) {
		ST_ERR("i2c client is null!!\n");
		return 0;
	}

	/*n2dm_read_chip_name(client, strbuf, N2DM_BUFSIZE);
*/
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;
	u8 strbuf[N2DM_BUFSIZE];

	if (obj->client == NULL) {
		ST_ERR("i2c client is null!!\n");
		return 0;
	}
	n2dm_acc_read_data(acc_obj, strbuf, N2DM_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_show_rawdata_value(struct device_driver *ddri, char *buf)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;
	u8 strbuf[N2DM_BUFSIZE];

	if (obj->client == NULL) {
		ST_ERR("i2c client is null!!\n");
		return 0;
	}

	n2dm_acc_read_rawdata_string(acc_obj, strbuf);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_show_cali_value(struct device_driver *ddri, char *buf)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;
	int err, len, mul;
	int tmp[N2DM_AXES_NUM];

	len = 0;

	if (obj->client == NULL) {
		ST_ERR("i2c client is null!!\n");
		return 0;
	}

	err = n2dm_acc_read_calibration(acc_obj, tmp);
	if (err) {
		return -EINVAL;
	}
	mul = acc_obj->reso->sensitivity / n2dm_offset_resolution.sensitivity;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,
			acc_obj->offset[N2DM_AXIS_X], acc_obj->offset[N2DM_AXIS_Y], acc_obj->offset[N2DM_AXIS_Z],
			acc_obj->offset[N2DM_AXIS_X], acc_obj->offset[N2DM_AXIS_Y], acc_obj->offset[N2DM_AXIS_Z]);
	len += snprintf(buf + len, PAGE_SIZE - len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1,
			acc_obj->cali_sw[N2DM_AXIS_X],
			acc_obj->cali_sw[N2DM_AXIS_Y],
			acc_obj->cali_sw[N2DM_AXIS_Z]);

	len += snprintf(buf + len, PAGE_SIZE - len, "[ALL](%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n",
			acc_obj->offset[N2DM_AXIS_X] * mul + acc_obj->cali_sw[N2DM_AXIS_X],
			acc_obj->offset[N2DM_AXIS_Y] * mul + acc_obj->cali_sw[N2DM_AXIS_Y],
			acc_obj->offset[N2DM_AXIS_Z] * mul + acc_obj->cali_sw[N2DM_AXIS_Z],
			tmp[N2DM_AXIS_X], tmp[N2DM_AXIS_Y], tmp[N2DM_AXIS_Z]);

		return len;

}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;
	int err, x, y, z;
	int dat[N2DM_AXES_NUM];

	if (!strncmp(buf, "rst", 3)) {
		err = n2dm_acc_reset_calibration(acc_obj);
		if (err) {
			ST_ERR("reset offset err = %d\n", err);
		}
	} else if (sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z) == 3) {
		dat[N2DM_AXIS_X] = x;
		dat[N2DM_AXIS_Y] = y;
		dat[N2DM_AXIS_Z] = z;
		err = n2dm_acc_write_calibration(acc_obj, dat);
		if (err) {
			ST_ERR("write calibration err = %d\n", err);
		}
	} else {
		ST_ERR("invalid format\n");
	}

	return count;
}
/*----------------------------------------------------------------------------*/

static ssize_t n2dm_attr_acc_show_power_status(struct device_driver *ddri, char *buf)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct i2c_client *client = obj->client;

	u8 data;

	if (obj->client == NULL) {
		ST_ERR("i2c client is null!!\n");
		return 0;
	}

	n2dm_i2c_read_block(client, N2DM_REG_CTL_REG1, &data, 0x01);
	return snprintf(buf, PAGE_SIZE, "%x\n", data);
}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_N2DM_LOWPASS
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;

	if (atomic_read(&acc_obj->firlen)) {
		int idx, len = atomic_read(&acc_obj->firlen);

		ST_LOG("len = %2d, idx = %2d\n", acc_obj->fir.num, acc_obj->fir.idx);
		for (idx = 0; idx < len; idx++) {
			ST_LOG("[%5d %5d %5d]\n",
					acc_obj->fir.raw[idx][N2DM_AXIS_X],
					acc_obj->fir.raw[idx][N2DM_AXIS_Y],
					acc_obj->fir.raw[idx][N2DM_AXIS_Z]);
		}

		ST_LOG("sum = [%5d %5d %5d]\n", acc_obj->fir.sum[N2DM_AXIS_X], acc_obj->fir.sum[N2DM_AXIS_Y],
		       acc_obj->fir.sum[N2DM_AXIS_Z]);
		ST_LOG("avg = [%5d %5d %5d]\n", acc_obj->fir.sum[N2DM_AXIS_X] /
				len, acc_obj->fir.sum[N2DM_AXIS_Y] / len,
				acc_obj->fir.sum[N2DM_AXIS_Z] / len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&acc_obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_N2DM_LOWPASS
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;
	int firlen;

	if (sscanf(buf, "%d", &firlen) != 1) {
		ST_ERR("invallid format\n");
	} else if (firlen > C_MAX_FIR_LENGTH) {
		ST_ERR("exceeds maximum filter length\n");
	} else {
		atomic_set(&acc_obj->firlen, firlen);
		if (firlen == 0) {
			atomic_set(&acc_obj->fir_en, 0);
		} else {
			memset(&acc_obj->fir, 0x00, sizeof(acc_obj->fir));
			atomic_set(&acc_obj->fir_en, 1);
		}
	}
#endif
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_show_trace_value(struct device_driver *ddri, char *buf)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;

	ssize_t res;

	if (obj == NULL) {
		ST_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&acc_obj->trace));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;
	int trace;

	if (obj == NULL) {
		ST_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "0x%x", &trace) == 1) {
		atomic_set(&acc_obj->trace, trace);
	} else {
		ST_ERR("invalid content: '%s', length = %d\n", buf, (int)count);
	}

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_show_status_value(struct device_driver *ddri, char *buf)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;
	ssize_t len = 0;

	if (obj == NULL) {
		ST_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (acc_obj->n2dm_acc_hw) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"CUST: i2c_num=%d, direction=%d, sensitivity = %d,(power_id=%d, power_vol=%d)\n",
			acc_obj->n2dm_acc_hw->i2c_num, acc_obj->n2dm_acc_hw->direction, acc_obj->reso->sensitivity,
			acc_obj->n2dm_acc_hw->power_id, acc_obj->n2dm_acc_hw->power_vol);
		dumpReg(obj);
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_show_chipinit_value(struct device_driver *ddri, char *buf)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;

	ssize_t res;

	if (obj == NULL) {
		ST_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&acc_obj->trace));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_store_chipinit_value(struct device_driver *ddri,
				const char *buf, size_t count)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;

	if (obj == NULL) {
		ST_ERR("i2c_data obj is null!!\n");
		return count;
	}

	n2dm_acc_init(acc_obj, 0);
	dumpReg(obj);

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_show_layout_value(struct device_driver *ddri, char *buf)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;

	if (obj == NULL) {
		ST_LOG("n2dm_acc is null!!\n");
		return -EPERM;
	}

	return snprintf(buf, 512, "(%d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		       acc_obj->n2dm_acc_hw->direction, acc_obj->cvt.sign[0], acc_obj->cvt.sign[1],
		       acc_obj->cvt.sign[2], acc_obj->cvt.map[0], acc_obj->cvt.map[1], acc_obj->cvt.map[2]);
}
/*----------------------------------------------------------------------------*/
static ssize_t n2dm_attr_acc_store_layout_value(struct device_driver *ddri,
				const char *buf, size_t count)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;

	int layout = 0;

	if (obj == NULL) {
		ST_ERR("n2dm_acc is null!!\n");
		return count;
	}

	if (sscanf(buf, "%d", &layout) == 1) {
		if (!hwmsen_get_convert(layout, &acc_obj->cvt)) {
			ST_ERR("HWMSEN_GET_CONVERT function error!\r\n");
		} else if (!hwmsen_get_convert(acc_obj->n2dm_acc_hw->direction, &acc_obj->cvt)) {
			ST_LOG("invalid layout: %d, restore to %d\n", layout, acc_obj->n2dm_acc_hw->direction);
		} else {
			ST_ERR("invalid layout: (%d, %d)\n", layout, acc_obj->n2dm_acc_hw->direction);
			hwmsen_get_convert(0, &acc_obj->cvt);
		}
	} else {
		ST_LOG("invalid format = '%s'\n", buf);
	}

	return count;
}
/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo,	S_IRUGO, n2dm_attr_acc_show_chipinfo_value,	NULL);
static DRIVER_ATTR(chipid,		S_IRUGO, n2dm_attr_acc_show_chipid_value,		NULL);
static DRIVER_ATTR(rawdata,		S_IRUGO, n2dm_attr_acc_show_rawdata_value,	NULL);
static DRIVER_ATTR(sensordata,	S_IRUGO, n2dm_attr_acc_show_sensordata_value,	NULL);
static DRIVER_ATTR(cali,	S_IWUSR | S_IRUGO, n2dm_attr_acc_show_cali_value,
				n2dm_attr_acc_store_cali_value);
static DRIVER_ATTR(power,		S_IRUGO, n2dm_attr_acc_show_power_status,		NULL);
static DRIVER_ATTR(firlen,	S_IWUSR | S_IRUGO, n2dm_attr_acc_show_firlen_value,
				n2dm_attr_acc_store_firlen_value);
static DRIVER_ATTR(trace,	S_IWUSR | S_IRUGO, n2dm_attr_acc_show_trace_value,
				n2dm_attr_acc_store_trace_value);
static DRIVER_ATTR(status,		S_IRUGO, n2dm_attr_acc_show_status_value,		NULL);
static DRIVER_ATTR(chipinit,	S_IWUSR | S_IRUGO, n2dm_attr_acc_show_chipinit_value,
				n2dm_attr_acc_store_chipinit_value);
static DRIVER_ATTR(layout,	S_IRUGO | S_IWUSR, n2dm_attr_acc_show_layout_value,
				n2dm_attr_acc_store_layout_value);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *n2dm_attr_acc_list[] = {
	&driver_attr_chipinfo,	/*chip information*/
	&driver_attr_chipid,	/*chip id*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_rawdata,	/*dump sensor raw data*/
	&driver_attr_cali,		/*show calibration data*/
	&driver_attr_power,		/*show power reg*/
	&driver_attr_firlen,	/*filter length: 0: disable, others: enable*/
	&driver_attr_trace,		/*trace log*/
	&driver_attr_status,
	&driver_attr_chipinit,
	&driver_attr_layout,
};
/*----------------------------------------------------------------------------*/
int n2dm_acc_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(n2dm_attr_acc_list);

	if (driver == NULL) {
		return -EINVAL;
	}

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, n2dm_attr_acc_list[idx]);
		if (err) {
			ST_ERR("driver_create_file (%s) = %d\n", n2dm_attr_acc_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}
/*----------------------------------------------------------------------------*/
int n2dm_acc_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(n2dm_attr_acc_list);

	if (driver == NULL) {
		return -EINVAL;
	}


	for (idx = 0; idx < num; idx++) {
		driver_remove_file(driver, n2dm_attr_acc_list[idx]);
	}


	return err;
}

/*----------------------------------------------------------------------------*/

/* if use  this typ of enable, Gsensor should report inputEvent(x, y, z,stats, div) to HAL
*/
static int n2dm_acc_open_report_data_intf(int open)
{
	/*should queuq work to report event if  is_report_input_direct=true
*/
	return 0;
}

/* if use  this typ of enable, Gsensor only enabled but not report inputEvent to HAL
*/
static int n2dm_acc_enable_nodata_intf(int en)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;
	int res = 0;
	bool power = false;

	if (en == 1)
		power = true;
	else if (en == 0)
		power = false;

	acc_obj->enabled = en;

	res = n2dm_acc_set_power_mode(acc_obj, power);
	if (res != N2DM_SUCCESS) {
		ST_ERR("n2dm_acc_set_power_mode fail!\n");
		return -EPERM;
	}

	ST_LOG("n2dm_acc_enable_nodata_intf OK!\n");
	return 0;
}

static int n2dm_acc_set_delay_intf(u64 ns)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;

	int value = 0;
	int sample_delay = 0;
	int err;

	value = (int)ns / 1000 / 1000;
	if (value <= 5) {
		sample_delay = N2DM_BW_200HZ;
	} else if (value <= 10) {
		sample_delay = N2DM_BW_100HZ;
	} else {
		sample_delay = N2DM_BW_50HZ;
	}

	/*acc_obj->odr = sample_delay;
*/
	err = n2dm_acc_set_odr(acc_obj, sample_delay);
	if (err != N2DM_SUCCESS) { /*0x2C->BW=100Hz
*/
		ST_ERR("Set delay parameter error!\n");
	}

	if (value >= 50) {
		atomic_set(&acc_obj->filter, 0);
	} else {
#ifdef CONFIG_N2DM_LOWPASS
		acc_obj->fir.num = 0;
		acc_obj->fir.idx = 0;
		acc_obj->fir.sum[N2DM_AXIS_X] = 0;
		acc_obj->fir.sum[N2DM_AXIS_Y] = 0;
		acc_obj->fir.sum[N2DM_AXIS_Z] = 0;
#endif
		atomic_set(&acc_obj->filter, 1);
	}

	ST_LOG("n2dm_acc_set_delay_intf (%d)\n", value);
	return 0;
}

static int n2dm_acc_batch_intf(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return n2dm_acc_set_delay_intf((u64)samplingPeriodNs);
}

static int n2dm_acc_flush_intf(void)
{
	return acc_flush_report();
}

static int n2dm_acc_get_data_intf(int *x, int *y, int *z, int *status)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;
	u8 buff[N2DM_BUFSIZE];

	n2dm_acc_read_data(acc_obj, buff, N2DM_BUFSIZE);

	if (sscanf(buff, "%x %x %x", x, y, z) < 0) {
		pr_err("scanf error");
	}
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}

#ifdef MISC_DEVICE_FACTORY
static int n2dm_acc_open(struct inode *inode, struct file *file)
{
	file->private_data = obj_i2c_data;

	if (file->private_data == NULL) {
		ST_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int n2dm_acc_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
static long n2dm_acc_unlocked_ioctl(struct file *file, unsigned int cmd,
				    unsigned long arg)

{
	struct n2dm_data *obj = (struct n2dm_data *)file->private_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;
	struct SENSOR_DATA sensor_data;

	u8 strbuf[N2DM_BUFSIZE];
	void __user *data;
	long err = 0;
	int cali[3];

	/*ST_FUN(f);
*/
	if (_IOC_DIR(cmd) & _IOC_READ) {
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if (err) {
		ST_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case GSENSOR_IOCTL_INIT:
		n2dm_acc_init(acc_obj, 0);
		break;

	case GSENSOR_IOCTL_READ_CHIPINFO:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		n2dm_read_chip_name(obj, strbuf, N2DM_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_SENSORDATA:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		n2dm_acc_set_power_mode(acc_obj, true);

		n2dm_acc_read_data(acc_obj, strbuf, N2DM_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_GAIN:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (copy_to_user(data, &gsensor_gain, sizeof(struct GSENSOR_VECTOR3D))) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_OFFSET:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (copy_to_user(data, &gsensor_offset, sizeof(struct GSENSOR_VECTOR3D))) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_RAW_DATA:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		n2dm_acc_read_rawdata_string(acc_obj, strbuf);
		if (copy_to_user(data, &strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_SET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (copy_from_user(&sensor_data, data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		if (atomic_read(&acc_obj->suspend)) {
			ST_ERR("Perform calibration in suspend state!!\n");
			err = -EINVAL;
		} else {
			cali[N2DM_AXIS_X] = sensor_data.x * acc_obj->reso->sensitivity / GRAVITY_EARTH_1000;
			cali[N2DM_AXIS_Y] = sensor_data.y * acc_obj->reso->sensitivity / GRAVITY_EARTH_1000;
			cali[N2DM_AXIS_Z] = sensor_data.z * acc_obj->reso->sensitivity / GRAVITY_EARTH_1000;
			err = n2dm_acc_write_calibration(acc_obj, cali);
		}
		break;

	case GSENSOR_IOCTL_CLR_CALI:
		err = n2dm_acc_reset_calibration(acc_obj);
		break;

	case GSENSOR_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = n2dm_acc_read_calibration(acc_obj, cali)
		if (err) {
			break;
		}

		sensor_data.x = cali[N2DM_AXIS_X] * GRAVITY_EARTH_1000 / acc_obj->reso->sensitivity;
		sensor_data.y = cali[N2DM_AXIS_Y] * GRAVITY_EARTH_1000 / acc_obj->reso->sensitivity;
		sensor_data.z = cali[N2DM_AXIS_Z] * GRAVITY_EARTH_1000 / acc_obj->reso->sensitivity;
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		break;

	default:
		ST_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}

#ifdef CONFIG_COMPAT
static long n2dm_acc_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long err = 0;
	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_GSENSOR_IOCTL_READ_SENSORDATA:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_READ_SENSORDATA, (unsigned long)arg32);
		if (err) {
			ST_ERR("GSENSOR_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
			return err;
		}
		break;

	case COMPAT_GSENSOR_IOCTL_SET_CALI:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_SET_CALI, (unsigned long)arg32);
		if (err) {
			ST_ERR("GSENSOR_IOCTL_SET_CALI unlocked_ioctl failed.");
			return err;
		}
		break;

	case COMPAT_GSENSOR_IOCTL_GET_CALI:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_GET_CALI, (unsigned long)arg32);
		if (err) {
			ST_ERR("GSENSOR_IOCTL_GET_CALI unlocked_ioctl failed.");
			return err;
		}
		break;

	case COMPAT_GSENSOR_IOCTL_CLR_CALI:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_CLR_CALI, (unsigned long)arg32);
		if (err) {
			ST_ERR("GSENSOR_IOCTL_CLR_CALI unlocked_ioctl failed.");
			return err;
		}
		break;

	default:
		ST_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}
#endif


static struct file_operations n2dm_acc_fops = {
	.owner = THIS_MODULE,
	.open = n2dm_acc_open,
	.release = n2dm_acc_release,
	.unlocked_ioctl = n2dm_acc_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = n2dm_acc_compat_ioctl,
#endif
};

static struct miscdevice n2dm_acc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &n2dm_acc_fops,
};

#else
static int n2dm_acc_factory_do_self_test(void)
{
	return 0;
}

static int n2dm_acc_factory_get_cali(int32_t data[3])
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;
	int cali[3];
	int err = -1;

	err = n2dm_acc_read_calibration(acc_obj, cali);
	if (err) {
		ST_LOG("n2dm_acc_read_calibration failed\n");
		return -EPERM;
	}

	data[0] = cali[N2DM_AXIS_X];
	data[1] = cali[N2DM_AXIS_Y];
	data[2] = cali[N2DM_AXIS_Z];

	return 0;
}

static int n2dm_acc_factory_set_cali(int32_t data[3])
{
	int err = 0;
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;

	ST_FUN();

	err = n2dm_acc_write_calibration(acc_obj, data);
	if (err) {
		ST_LOG("n2dm_acc_write_calibration failed!\n");
		return -EPERM;
	}

	return 0;
}

static int n2dm_acc_factory_enable_calibration(void)
{
	return 0;
}

static int n2dm_acc_factory_clear_cali(void)
{
	int err = 0;
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;

	err = n2dm_acc_reset_calibration(acc_obj);
	if (err) {
		ST_LOG("n2dm_acc_reset_calibration failed!\n");
		return -EPERM;
	}

	return 0;
}

static int n2dm_acc_factory_get_raw_data(int32_t data[3])
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;
	s16 databuff[3];

	n2dm_acc_read_rawdata(acc_obj, databuff);
	data[0] = (s16)databuff[0];
	data[1] = (s16)databuff[1];
	data[2] = (s16)databuff[2];

	ST_LOG("n2dm_factory_get_raw_data done!\n");

	return 0;
}

static int n2dm_acc_factory_get_data(int32_t data[3], int *status)
{
	return n2dm_acc_get_data_intf(&data[0], &data[1], &data[2], status);
}

static int n2dm_acc_factory_enable_sensor(bool enable, int64_t sample_periods_ms)
{
	int err;

	err = n2dm_acc_enable_nodata_intf(enable == true ? 1 : 0);
	if (err) {
		ST_ERR("n2dm_acc_enable_nodata_intf failed!\n");
		return -EPERM;
	}

	err = n2dm_acc_set_delay_intf(sample_periods_ms * 1000000);
	if (err) {
		ST_ERR("n2dm_acc_set_delay_intf failed!\n");
		return -EPERM;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static struct accel_factory_fops n2dm_acc_factory_fops = {
	.enable_sensor	= n2dm_acc_factory_enable_sensor,
	.get_data		= n2dm_acc_factory_get_data,
	.get_raw_data	= n2dm_acc_factory_get_raw_data,
	.enable_calibration = n2dm_acc_factory_enable_calibration,
	.clear_cali		= n2dm_acc_factory_clear_cali,
	.set_cali		= n2dm_acc_factory_set_cali,
	.get_cali		= n2dm_acc_factory_get_cali,
	.do_self_test	= n2dm_acc_factory_do_self_test,
};

static struct accel_factory_public n2dm_acc_factory_device = {
	.gain		= 1,
	.sensitivity = 1,
	.fops		= &n2dm_acc_factory_fops,
};
#endif

/*----------------------------------------------------------------------------*/
static int n2dm_acc_local_init(void)
{
	struct n2dm_data *obj = obj_i2c_data;
	struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;
	struct i2c_client *client = obj->client;
	int err = 0, retry = 0;
	struct acc_control_path ctl = {0};
	struct acc_data_path data = {0};
	/*const u8 *name = "mediatek,n2dm";
*/
	ST_FUN();

#if 0
	acc_obj->n2dm_acc_hw = get_accel_dts_func(name, &n2dm_cust_hw);
	if (!acc_obj->n2dm_acc_hw) {
		ST_ERR("get n2dm dts info failed\n");
	}
#else
	err = get_accel_dts_func(client->dev.of_node, &n2dm_cust_hw);
	if (err < 0) {
		ST_ERR("get n2dm dts info fail\n");
		return -EFAULT;
	}

	acc_obj->n2dm_acc_hw = &n2dm_cust_hw;
#endif

	err = hwmsen_get_convert(acc_obj->n2dm_acc_hw->direction, &acc_obj->cvt);
	if (err) {
		ST_ERR("invalid direction: %d\n", acc_obj->n2dm_acc_hw->direction);
		goto exit;
	}

	atomic_set(&acc_obj->trace, 0);
	atomic_set(&acc_obj->suspend, 0);

#ifdef CONFIG_N2DM_LOWPASS
	if (acc_obj->n2dm_acc_hw->firlen > C_MAX_FIR_LENGTH) {
		atomic_set(&acc_obj->firlen, C_MAX_FIR_LENGTH);
	} else {
		atomic_set(&acc_obj->firlen, acc_obj->n2dm_acc_hw->firlen);
	}

	if (atomic_read(&acc_obj->firlen) > 0) {
		atomic_set(&acc_obj->fir_en, 1);
	}
#endif

	for (retry = 0; retry < 3; retry++) {
		err = n2dm_acc_init(acc_obj, 1);
		if (err) {
			ST_ERR("n2dm_acc_device init cilent fail time: %d\n", retry);
			continue;
		}
	}
	if (err != 0)
		goto exit_init_failed;

#ifdef MISC_DEVICE_FACTORY
	err = misc_register(&n2dm_acc_device);
	if (err) {
		ST_ERR("n2dm_acc_device register failed\n");
		goto exit_misc_device_register_failed;
	}
#else
	err = accel_factory_device_register(&n2dm_acc_factory_device);
	if (err) {
		ST_ERR("n2dm_acc_factory_device register failed!\n");
		goto exit_misc_device_register_failed;
	}
#endif

	err = n2dm_acc_create_attr(&(n2dm_acc_init_info.platform_diver_addr->driver));
	if (err) {
		ST_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data	= n2dm_acc_open_report_data_intf;
	ctl.enable_nodata		= n2dm_acc_enable_nodata_intf;
	ctl.set_delay			= n2dm_acc_set_delay_intf;
	ctl.batch				= n2dm_acc_batch_intf;
	ctl.flush				= n2dm_acc_flush_intf;
	ctl.is_use_common_factory  = false;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch	= acc_obj->n2dm_acc_hw->is_batch_supported;

	err = acc_register_control_path(&ctl);
	if (err) {
		ST_ERR("register acc control path err\n");
		goto exit_kfree;
	}

	data.get_data   = n2dm_acc_get_data_intf;
	data.vender_div = 1000;
	err = acc_register_data_path(&data);
	if (err) {
		ST_ERR("register acc data path err\n");
		goto exit_kfree;
	}

	ST_LOG("%s: OK\n", __func__);
	n2dm_acc_init_flag = 0;
	return 0;

exit_create_attr_failed:
#ifdef MISC_DEVICE_FACTORY
	misc_deregister(&n2dm_acc_device);
#else
	accel_factory_device_deregister(&n2dm_acc_factory_device);
#endif
exit_misc_device_register_failed:
exit_init_failed:
exit_kfree:
	kfree(obj);
exit:
	ST_ERR("%s: err = %d\n", __func__, err);
	n2dm_acc_init_flag = -1;
	return n2dm_acc_init_flag;
}

/*----------------------------------------------------------------------------*/
static int n2dm_acc_local_remove(void)
{
	ST_FUN();
#ifdef MISC_DEVICE_FACTORY
	misc_deregister(&n2dm_acc_device);
#else
	accel_factory_device_deregister(&n2dm_acc_factory_device);
#endif
	n2dm_acc_delete_attr(&(n2dm_acc_init_info.platform_diver_addr->driver));

	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
struct acc_init_info n2dm_acc_init_info = {
	.name   = "n2dm",
	.init   = n2dm_acc_local_init,
	.uninit = n2dm_acc_local_remove,
};

MODULE_DESCRIPTION("STMicroelectronics n2dm driver");
MODULE_AUTHOR("William Zeng");
MODULE_LICENSE("GPL v2");
