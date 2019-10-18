
/************************************************************************
*
* File Name: himax_firmware_config.h
*
*  *   Version: v1.0
*
************************************************************************/
#ifndef _HIMAX_FIRMWARE_CONFIG_H_
#define _HIMAX_FIRMWARE_CONFIG_H_

/********************** Upgrade ***************************

  auto upgrade, please keep enable
*********************************************************/
/*#define HX_AUTO_UPDATE_FW */
/*#define HX_SMART_WAKEUP */

/* id[1:0]: 0x00 0x01 0x10 0x11 */
#define HX_VENDOR_ID_0 0
#define HX_VENDOR_ID_1 1
#define HX_VENDOR_ID_2 2
#define HX_VENDOR_ID_3 3

#define HXTS_VENDOR_0_NAME                         "unknown"
#define HXTS_VENDOR_1_NAME                         "unknown"
#define HXTS_VENDOR_2_NAME                         "unknown"
#define HXTS_VENDOR_3_NAME                         "unknown"
/*
 *Himax_firmware.bin file for auto upgrade, you must replace it with your own
 * define your own fw_bin
 */
#define HX_UPGRADE_FW0                  "Himax_firmware.bin"

#define HX_UPGRADE_FW1                  "Himax_firmware.bin"

#define HX_UPGRADE_FW2                  "Himax_firmware.bin"

#define HX_UPGRADE_FW3                  "Himax_firmware.bin"

#endif
