#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include "zte_lcd_common.h"

struct i2c_client *i2c_tps65132b_client = NULL;
void tps65132b_set_vsp_vsn_level(u8 level);
/*REG:
0A	0B	0C	0D	0E	0F	10	11	12	13	14
5.0	5.1	5.2	5.3	5.4	5.5	5.6	5.7	5.8	5.9	6.0*/

/*bool ti65132b_true = false;*/
bool ti65132b_probe = false;

#define LCM_VSP_VSN_55V  0x0F
#define LCM_VSP_VSN_50V  0x0a
#define ZTE_LCD_VPOS_ADDRESS		0x00
#define ZTE_LCD_VNEG_ADDRESS		0x01

int tps65132b_read_reg(struct i2c_client *client, u8 *buf, u8 *data)
{
	struct i2c_msg msgs[2];
	int ret;
	u8 retries = 0;

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = client->addr;
	msgs[0].len   = 1;
	msgs[0].buf   = &buf[0];

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = client->addr;
	msgs[1].len   = 1;
	msgs[1].buf   = data;

	while (retries < 3) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
		msleep_interruptible(5);
	}
	pr_info("tps65132b_read_reg retries=%d,ret=%d\n", retries, ret);
	if (ret != 2) {
		pr_err("tps65132b read transfer error\n");
		ret = -1;
	}

	return ret;
}
int tps65132b_write_reg(struct i2c_client *client, u8 *buf, int len)
{
	int err = 0;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = len + 1,
			.buf = buf,
		},
	};

	do {
		err = i2c_transfer(client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(5);
	} while ((err != 1) && (++tries < 3));

	pr_info("tps65132b_write_reg tries=%d,ret=%d\n", tries, err);
	if (err != 1) {
		pr_err("tps65132b write transfer error\n");
		err = -1;
	}

	return err;
}

static int enp_gpio = 0;
static int enn_gpio = 0;
void tps65132b_set_vsp_vsn_gpio(u8 enable)
{
	if (enable) {
		msleep_interruptible(5);
		if (enp_gpio > 0) {
			gpio_set_value(enp_gpio, 1);
		}
		msleep_interruptible(5);
		if (enn_gpio > 0) {
			gpio_set_value(enn_gpio, 1);
		}
		msleep_interruptible(5);
	} else {
		msleep_interruptible(5);
		if (enn_gpio > 0) {
			gpio_set_value(enn_gpio, 0);
		}
		msleep_interruptible(5);
		if (enp_gpio > 0) {
			gpio_set_value(enp_gpio, 0);
		}
		msleep_interruptible(5);
	}
}

void tps65132b_set_vsp_vsn_level(u8 level)
{
	u8 buf_vsp[2] = {0x00, 0x00};
	u8 buf_vsn[2] = {0x01, 0x00};

	/*pr_info("tps65132b probe=%d\n", ti65132b_probe);*/
	if (ti65132b_probe) {
		/*err = tps65132b_read_reg(i2c_tps65132b_client, &addrvsn, &datavsn);
		if (err) {
			pr_info("tps65132b read fail\n");
			ti65132b_true = false;
			return;
		} else {
			pr_info("tps65132b read data=%x\n", datavsn);
			ti65132b_true = true;
		}*/
		buf_vsp[1] = level;
		tps65132b_write_reg(i2c_tps65132b_client, buf_vsp, 1);
		buf_vsn[1] = level;
		tps65132b_write_reg(i2c_tps65132b_client, buf_vsn, 1);
		usleep_range(5000, 5100);
	}
}

static int tps65132b_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct property *prop = NULL;
	int retval = 0;
	struct device_node *np = client->dev.of_node;

	prop = of_find_property(np, "tps65132,enp-gpio", NULL);

	if (prop) {
		enp_gpio = of_get_named_gpio_flags(np, "tps65132,enp-gpio", 0, NULL);
		enn_gpio = of_get_named_gpio_flags(np, "tps65132,enn-gpio", 0, NULL);

		if (!gpio_is_valid(enp_gpio)) {
			pr_err("tps65132  not specified %i\n", __LINE__);
		} else {
			retval = gpio_request(enp_gpio, "lcd-enp");
			if (retval) {
				pr_err("tps65132 gpio_request fail %i\n", __LINE__);
			}

			retval = gpio_direction_output(enp_gpio, 1);
			if (retval) {
				pr_err("tps65132 set gpio direction fail %i\n", __LINE__);
			}
		}

		if (!gpio_is_valid(enn_gpio)) {
			pr_err("tps65132  not specified %i\n", __LINE__);
		} else {
			retval = gpio_request(enn_gpio, "lcd-enn");
			if (retval) {
				pr_err("tps65132 gpio_request fail %i\n", __LINE__);
			}

			retval = gpio_direction_output(enn_gpio, 1);
			if (retval) {
				pr_err("tps65132 set gpio direction fail %i\n", __LINE__);
			}
		}
	}
	/*pr_info("tps65132b_probe addr=0x%x\n", client->addr);*/
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("tps65132b_probe,client not i2c capable\n");
		return -EIO;
	}
	i2c_tps65132b_client = client;

	ti65132b_probe = true;
	pr_info("tps65132b ti65132b_probe ok\n");
	return 0;
}

static int tps65132b_remove(struct i2c_client *client)
{
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id tps65132b_id_table[] = {
	{"ti65132b", 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, tps65132b_id_table);

static const struct of_device_id tps65132b_of_id_table[] = {
	{.compatible = "tps,ti65132b"},
	{ },
};

static struct i2c_driver tps65132b_i2c_driver = {
	.driver = {
		.name = "ti65132b",
		.owner = THIS_MODULE,
		.of_match_table = tps65132b_of_id_table,
	},
	.probe = tps65132b_probe,
	.remove = tps65132b_remove,
	.id_table = tps65132b_id_table,
};

module_i2c_driver(tps65132b_i2c_driver);

MODULE_DESCRIPTION("tps65132b chip driver");
MODULE_LICENSE("GPL");

