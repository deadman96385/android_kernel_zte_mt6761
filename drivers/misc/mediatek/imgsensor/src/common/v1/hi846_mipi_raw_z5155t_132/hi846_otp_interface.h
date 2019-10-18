/*****************************************************************************
 *
 * Filename:
 * ---------
 *   Hi843_SunWinTech_OTP_Interface.h
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   Header file of CAM_CAL driver
 *
 *
 * Author:
 * -------
 *   John Wei (MTK07407)
 *
 *============================================================================*/
#ifndef __CAM_CAL_H
#define __CAM_CAL_H
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"


#define CAM_CAL_DEV_MAJOR_NUMBER 226



void hi846_otp_setting(void);
void hi846_otp_disable(void);
int hi846_otp_read_(u32 offset, u8 *data, u32 size);
int hi846_otp_read(u32 offset);


extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);
#endif /* __CAM_CAL_H */

