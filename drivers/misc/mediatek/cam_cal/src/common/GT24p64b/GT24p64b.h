/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   GT24p64b.h
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
 * Dream Yeh (MTK08783)
 *
 *============================================================================
 */
#ifndef __GT24P64B_H
#define __GT24P64B_H
#include <linux/i2c.h>


unsigned int gt24p64b_selective_read_region(struct i2c_client *client, unsigned int addr,
	unsigned char *data, unsigned int size);


#endif /* __CAM_CAL_H */

