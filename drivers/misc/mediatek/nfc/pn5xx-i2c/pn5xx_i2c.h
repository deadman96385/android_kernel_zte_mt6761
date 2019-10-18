/*
 * Driver include for the PN544 NFC chip.
 *
 * Copyright (C) Nokia Corporation
 *
 * Author: Jari Vanhala <ext-jari.vanhala@nokia.com>
 * Contact: Matti Aaltoenn <matti.j.aaltonen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _PN544_H_
#define _PN544_H_

#include <linux/i2c.h>

#define PN544_MAGIC	0xE9

/*
 * PN544 power control via ioctl
 * PN544_SET_PWR(0): power off
 * PN544_SET_PWR(1): power on
 * PN544_SET_PWR(2): reset and power on with firmware download enabled
 */
#define PN5XX_SET_PWR	_IOW(PN544_MAGIC, 0x01, long)
/*
 * SPI Request NFCC to enable p61 power, only in param
 * Only for SPI
 * level 1 = Enable power
 * level 0 = Disable power
 */
#define P61_SET_SPI_PWR    _IOW(PN544_MAGIC, 0x02, long)

/* SPI or DWP can call this ioctl to get the current
 * power state of P61
 *
*/
#define P61_GET_PWR_STATUS    _IOR(PN544_MAGIC, 0x03, long)

/* DWP side this ioctl will be called
 * level 1 = Wired access is enabled/ongoing
 * level 0 = Wired access is disalbed/stopped
*/
#define P61_SET_WIRED_ACCESS _IOW(PN544_MAGIC, 0x04, long)

/*
 * NFC Init will call the ioctl to register the PID with the i2c driver
*/
#define P544_SET_NFC_SERVICE_PID _IOW(PN544_MAGIC, 0x05, long)

typedef enum p61_access_state {
	P61_STATE_INVALID = 0x0000,
	P61_STATE_IDLE = 0x0100, /* p61 is free to use */
	P61_STATE_WIRED = 0x0200,  /* p61 is being accessed by DWP (NFCC)*/
	P61_STATE_SPI = 0x0400, /* P61 is being accessed by SPI */
	P61_STATE_DWNLD = 0x0800, /* NFCC fw download is in progress */
	P61_STATE_SPI_PRIO = 0x1000, /*Start of p61 access by SPI on priority*/
	P61_STATE_SPI_PRIO_END = 0x2000, /*End of p61 access by SPI on priority*/
	P61_STATE_SPI_END = 0x4000,
	P61_STATE_JCP_DWNLD = 0x8000,/* JCOP download in progress */
	P61_STATE_SECURE_MODE = 0x100000, /* secure mode state*/
	P61_STATE_SPI_SVDD_SYNC_START = 0x0001, /*ESE_VDD Low req by SPI*/
	P61_STATE_SPI_SVDD_SYNC_END = 0x0002, /*ESE_VDD is Low by SPI*/
	P61_STATE_DWP_SVDD_SYNC_START = 0x0004, /*ESE_VDD  Low req by Nfc*/
	P61_STATE_DWP_SVDD_SYNC_END = 0x0008 /*ESE_VDD is Low by Nfc*/
} p61_access_state_t;


#endif /* _PN544_H_ */
