
/************************************************************************
*
* File Name: tlsc6x_config.h
*
*  Abstract: global configurations
*
*   Version: v1.0
*
************************************************************************/
#ifndef _LINUX_TLSC6X_CONFIG_H_
#define _LINUX_TLSC6X_CONFIG_H_

/* #define TLSC_TPD_PROXIMITY */
#define TLSC_APK_DEBUG		/* apk debugger, close:undef */
/* #define TLSC_AUTO_UPGRADE */
#define TLSC_ESD_HELPER_EN	/* esd helper, close:undef */
/* #defineTLSC_FORCE_UPGRADE */
/* #define TLSC_GESTRUE */
#define TLSC_TP_PROC_SELF_TEST
/* #define TLSC_BUILDIN_BOOT */
/*
 * FW.h file for auto upgrade, you must replace it with your own
 * define your own fw_file, the sample one to be replaced is invalid
 */
#define TLSC_UPGRADE_FW_FILE                       "tlsc_fw_sample.h"

#define TLSC_UPGRADE_FW2_FILE                     "tlsc_fw_sample.h"

#define TLSC_UPGRADE_FW3_FILE                     "tlsc_fw_sample.h"

#define TLSC_UPGRADE_FW4_FILE                     "tlsc_fw_sample.h"
/*********************************************************/

#endif /* _LINUX_TLSC6X_CONFIG_H_ */
