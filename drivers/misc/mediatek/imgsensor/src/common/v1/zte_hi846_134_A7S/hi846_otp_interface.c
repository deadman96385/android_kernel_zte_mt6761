/*
 * Driver for CAM_CAL
 *
 *
 */

#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "cam_cal.h"
#include "cam_cal_define.h"
#include "hi846_otp_interface.h"


/* #include <asm/system.h>  // for SMP */
#include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif


#define PFX "HI846_OTP_BASE"

#define LOG_INF(format, args...)    \
	pr_err(PFX "[%s] " format, __func__, ##args)

#define i2c_write_id 0x40

/*************************************************************************************************
**************************************************************************************************/
static void i2c_write_sensor(unsigned int  addr, unsigned int  para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para)};

	iWriteRegI2C(pu_send_cmd, 3, i2c_write_id);
}

static unsigned short i2c_read_sensor(unsigned int  addr)
{
	unsigned short get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF)};

	kdSetI2CSpeed(400);
	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, i2c_write_id);
	return get_byte;
}

unsigned short i2c_OTP_read(u32 otp_addr)
{
	u32 i, data;

	i = otp_addr;
	i2c_write_sensor(0x070a, (i >> 8) & 0xFF); /*start address H*/
	i2c_write_sensor(0x070b, i & 0xFF); /*start address L*/
	i2c_write_sensor(0x0702, 0x01); /*read enable*/
	data = i2c_read_sensor(0x0708); /*OTP data read*/
	LOG_INF("addr:0x%x,val:0x%x\n", otp_addr, data);
	return data;
}

void hi846_otp_setting(void)
{
	LOG_INF("%s enter\n", __func__);
	i2c_write_sensor(0x0A02, 0x01); /*Fast sleep on*/
	i2c_write_sensor(0x0A00, 0x00);/*stand by on*/
	mdelay(10);
	i2c_write_sensor(0x0f02, 0x00);/*pll disable*/
	i2c_write_sensor(0x071a, 0x01);/*CP TRIM_H*/
	i2c_write_sensor(0x071b, 0x09);/*IPGM TRIM_H*/
	i2c_write_sensor(0x0d04, 0x01);/*Fsync(OTP busy)Output Enable*/
	i2c_write_sensor(0x0d00, 0x07);/*Fsync(OTP busy)Output Drivability*/
	i2c_write_sensor(0x003e, 0x10);/*OTP r/w mode*/
	i2c_write_sensor(0x070f, 0x05);/*OTP data rewrite*/
	i2c_write_sensor(0x0a00, 0x01);/*standby off*/
	LOG_INF("%s exit\n", __func__);
}

void hi846_otp_disable(void)
{
	i2c_write_sensor(0x0a00, 0x00); /*sleep on*/
	mdelay(100);
	i2c_write_sensor(0x003e, 0x00); /*display mode*/
	i2c_write_sensor(0x0a00, 0x01); /*sleep off*/
}

int hi846_otp_read_(u32 offset, u8 *data, u32 size)
{
	int i = 0;

	for (i = 0; i < size; i++) {
		*(data+i) = i2c_OTP_read(offset+i);
		LOG_INF("read addr:0x%x, value:%x,size:%d\n", offset+i, *(data+i), size);
	}
	return 0;
}

int hi846_otp_read(u32 offset)
{
	return i2c_OTP_read(offset);
}

