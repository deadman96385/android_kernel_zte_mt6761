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

static struct i2c_driver n2dm_i2c_driver;

struct n2dm_data *obj_i2c_data = NULL;

static DEFINE_MUTEX(n2dm_i2c_mutex);
static DEFINE_MUTEX(n2dm_op_mutex);

/*--------------------read function----------------------------------*/
int n2dm_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	u8 beg = addr;
	int err;
	struct i2c_msg msgs[2] = { {0}, {0} };

	mutex_lock(&n2dm_i2c_mutex);

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	if (!client) {
		mutex_unlock(&n2dm_i2c_mutex);
		return -EINVAL;
	} else if (len > C_I2C_FIFO_SIZE) {
		ST_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&n2dm_i2c_mutex);
		return -EINVAL;
	}
	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));

	if (err < 0) {
		ST_ERR("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;
	}
	mutex_unlock(&n2dm_i2c_mutex);
	return err; /*if success will return 0*/
}

int n2dm_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	/*because address also occupies one byte, the maximum length for write is 7 bytes*/
	int err, idx, num;
	u8 buf[C_I2C_FIFO_SIZE];

	err = 0;
	mutex_lock(&n2dm_i2c_mutex);
	if (!client) {
		mutex_unlock(&n2dm_i2c_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		ST_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&n2dm_i2c_mutex);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++) {
		buf[num++] = data[idx];
	}

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		ST_ERR("send command error!!\n");
		mutex_unlock(&n2dm_i2c_mutex);
		return -EFAULT;
	}
	mutex_unlock(&n2dm_i2c_mutex);
	return err; /*if success will return transfer length*/
}
/*----------------------------------------------------------------------------*/

void dumpReg(struct n2dm_data *obj)
{
	struct i2c_client *client = obj->client;
	int i = 0;
	u8 addr = 0x20, regdata = 0;

	for (i = 0; i < 16; i++) {
		/*dump all*/
		n2dm_i2c_read_block(client, addr, &regdata, 1);
		ST_LOG("Reg addr=%x regdata=%x\n", addr, regdata);
		addr++;
	}
}

/*--------------------ADXL power control function----------------------------------*/
#ifdef CONFIG_N2DM_ACC_DRY
int n2dm_set_interrupt(struct n2dm_data *obj, u8 intenable)
{
	struct i2c_client *client = obj->client;

	u8 databuf[2];
	u8 addr = N2DM_REG_CTL_REG3;
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 2);

	if ((n2dm_i2c_read_block(client, addr, databuf, 0x01)) < 0) {
		ST_ERR("read reg_ctl_reg1 register err!\n");
		return N2DM_ERR_I2C;
	}

	databuf[0] = 0x00;

	res = n2dm_i2c_write_block(client, N2DM_REG_CTL_REG3, databuf, 0x01);
	if (res < 0) {
		return N2DM_ERR_I2C;
	}

	return N2DM_SUCCESS;
}
#endif

int n2dm_check_device_id(struct n2dm_data *obj)
{
	struct i2c_client *client = obj->client;
	u8 databuf[2];
	int err;

	err = n2dm_i2c_read_block(client, N2DM_REG_WHO_AM_I, databuf, 0x01);
	if (err < 0) {
		return N2DM_ERR_I2C;
	}

	ST_LOG("N2DM who am I = 0x%x\n", databuf[0]);
	if (databuf[0] != N2DM_FIXED_DEVID) {
		return N2DM_ERR_IDENTIFICATION;
	}

	return N2DM_SUCCESS;
}

static ssize_t n2dm_attr_i2c_show_reg_value(struct device_driver *ddri, char *buf)
{
	struct n2dm_data *obj = obj_i2c_data;

	u8 reg_value;
	ssize_t res;
	int err;

	if (obj == NULL) {
		ST_ERR("i2c_data obj is null!!\n");
		return 0;
	}
	err = n2dm_i2c_read_block(obj->client, obj->reg_addr, &reg_value, 0x01);
	if (err < 0) {
		res = snprintf(buf, PAGE_SIZE, "i2c read error!!\n");
		return res;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", reg_value);
	return res;
}

static ssize_t n2dm_attr_i2c_store_reg_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct n2dm_data *obj = obj_i2c_data;

	u8 reg_value;
	int res;

	if (obj == NULL) {
		ST_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "0x%hhx", &reg_value) == 1) {
		res = n2dm_i2c_write_block(obj->client, obj->reg_addr, &reg_value, 0x01);
		if (res < 0) {
			return N2DM_ERR_I2C;
		}
	} else {
		ST_ERR("invalid content: '%s', length = %d\n", buf, (int)count);
	}

	return count;
}

static ssize_t n2dm_attr_acc_show_reg_addr(struct device_driver *ddri, char *buf)
{
	struct n2dm_data *obj = obj_i2c_data;
	/*struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;*/

	ssize_t res;

	if (obj == NULL) {
		ST_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", obj->reg_addr);
	return res;
}

static ssize_t n2dm_attr_i2c_store_reg_addr(struct device_driver *ddri, const char *buf, size_t count)
{
	struct n2dm_data *obj = obj_i2c_data;
	/*struct n2dm_acc *acc_obj = &obj->n2dm_acc_data;*/
	u8 reg_addr;

	if (obj == NULL) {
		ST_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "0x%hhx", &reg_addr) == 1) {
		obj->reg_addr = reg_addr;
	} else {
		ST_ERR("invalid content: '%s', length = %d\n", buf, (int)count);
	}

	return count;
}
static DRIVER_ATTR(reg_value, S_IWUSR | S_IRUGO,
					n2dm_attr_i2c_show_reg_value, n2dm_attr_i2c_store_reg_value);
static DRIVER_ATTR(reg_addr,  S_IWUSR | S_IRUGO,
					n2dm_attr_acc_show_reg_addr,  n2dm_attr_i2c_store_reg_addr);

static struct driver_attribute *n2dm_attr_i2c_list[] = {
	&driver_attr_reg_value,
	&driver_attr_reg_addr,
};

int n2dm_i2c_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(n2dm_attr_i2c_list);

	if (driver == NULL) {
		return -EINVAL;
	}

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, n2dm_attr_i2c_list[idx]);
		if (err) {
			ST_ERR("driver_create_file (%s) = %d\n", n2dm_attr_i2c_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

int n2dm_i2c_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(n2dm_attr_i2c_list);

	if (driver == NULL) {
		return -EINVAL;
	}


	for (idx = 0; idx < num; idx++) {
		driver_remove_file(driver, n2dm_attr_i2c_list[idx]);
	}


	return err;
}

/*----------------------------------------------------------------------------*/
static int n2dm_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strlcpy(info->type, N2DM_DEV_NAME, 16);
	return 0;
}

/*----------------------------------------------------------------------------*/

static int n2dm_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct n2dm_data *obj;
	int err = 0;

	ST_FUN();

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		return err;
	}
	memset(obj, 0, sizeof(struct n2dm_data));
	obj_i2c_data = obj;
	obj->client = client;
	i2c_set_clientdata(client, obj);

	err = n2dm_check_device_id(obj);
	if (err) {
		ST_ERR("check device error!\n");
		goto exit_check_device_failed;
	}

	err = n2dm_i2c_create_attr(&n2dm_i2c_driver.driver);
	if (err) {
		ST_ERR("create attr error!\n");
		goto exit_create_attr_failed;
	}
#ifdef CONFIG_N2DM_ACC_DRY
	/*TODO: request irq for data ready of SMD, TILT, etc.*/
#endif
	acc_driver_add(&n2dm_acc_init_info);

	ST_LOG("n2dm_i2c_probe exit\n");
	return 0;
exit_create_attr_failed:
exit_check_device_failed:
	kfree(obj);
	return err;

}
/*----------------------------------------------------------------------------*/
static int n2dm_i2c_remove(struct i2c_client *client)
{
	n2dm_i2c_delete_attr(&n2dm_i2c_driver.driver);
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id n2dm_i2c_id[] = { {N2DM_DEV_NAME, 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id n2dm_of_match[] = {
	{.compatible = "mediatek,gsensor_n2dm"},
	{}
};
#endif

static struct i2c_driver n2dm_i2c_driver = {
	.driver = {
		.name		= N2DM_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = n2dm_of_match,
#endif
	},
	.probe			= n2dm_i2c_probe,
	.remove			= n2dm_i2c_remove,
	.detect			= n2dm_i2c_detect,
	.id_table		= n2dm_i2c_id,
};

/*----------------------------------------------------------------------------*/
static int __init n2dm_module_init(void)
{
	ST_FUN();

	if (i2c_add_driver(&n2dm_i2c_driver)) {
		ST_ERR("add driver error\n");
		return -EPERM;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit n2dm_module_exit(void)
{
	i2c_del_driver(&n2dm_i2c_driver);

	ST_FUN();
}
/*----------------------------------------------------------------------------*/
module_init(n2dm_module_init);
module_exit(n2dm_module_exit);

/*----------------------------------------------------------------------------*/
MODULE_DESCRIPTION("STMicroelectronics n2dm driver");
MODULE_AUTHOR("William Zeng");
MODULE_LICENSE("GPL v2");
