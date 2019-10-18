/*
 * FILE:__TSP_FW_CLASS_H_INCLUDED
 *
 */
#ifndef __TPD_FW_H_INCLUDED
#define __TPD_FW_H_INCLUDED

#include <linux/device.h>
#include <linux/rwsem.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/printk.h>

#ifdef TPD_DMESG
#undef TPD_DMESG
#endif
#define TPD_DMESG(a, arg...) pr_notice("tpd: " a, ##arg)

#define CONFIG_CREATE_TPD_SYS_INTERFACE

#define PROC_TOUCH_DIR					"touchscreen"
#define PROC_TOUCH_INFO					"ts_information"
#define PROC_TOUCH_FW_UPGRADE			"FW_upgrade"
#define PROC_TOUCH_GET_TPRAWDATA		"get_tprawdata"
#define PROC_TOUCH_RW_REG				"rw_reg"
#define PROC_TOUCH_SMART_COVER		"smart_cover"
#define PROC_TOUCH_WAKE_GESTURE		"wake_gesture"
#define PROC_TOUCH_SUSPEND		"suspend"

/*gesture for tp, begin*/
#define  KEY_GESTURE_DOUBLE_CLICK 214
#define  MAX_VENDOR_NAME_LEN 20
#define VENDOR_END 0xff

enum ts_chip {
	TS_CHIP_INDETER		= 0x00,
	TS_CHIP_SYNAPTICS	= 0x01,
	TS_CHIP_ATMEL		= 0x02,
	TS_CHIP_CYTTSP		= 0x03,
	TS_CHIP_FOCAL		= 0x04,
	TS_CHIP_GOODIX		= 0x05,
	TS_CHIP_MELFAS		= 0x06,
	TS_CHIP_MSTAR		= 0x07,
	TS_CHIP_HIMAX		= 0x08,
	TS_CHIP_NOVATEK		= 0x09,
	TS_CHIP_ILITEK		= 0x0A,
	TS_CHIP_TLSC		= 0x0B,
	TS_CHIP_CHIPONE	    = 0x0C,

	TS_CHIP_MAX		= 0xFF,
};

struct tp_rwreg_t {
	int type;		/*  0: read, 1: write */
	int reg;		/*  register */
	int len;		/*  read/write length */
	int val;		/*  length = 1; read: return value, write: op return */
	int res;		/*  0: success, otherwise: fail */
	char *opbuf;	/*  length >= 1, read return value, write: op return */
};
enum {
	REG_OP_READ = 0,
	REG_OP_WRITE = 1,
};

enum {
	REG_CHAR_NUM_2 = 2,
	REG_CHAR_NUM_4 = 4,
	REG_CHAR_NUM_8 = 8,
};

struct tpvendor_t {
	int vendor_id;
	char *vendor_name;
};
/*chip_model_id synaptics 1,atmel 2,cypress 3,focal 4,goodix 5,mefals 6,mstar
7,himax 8;
 *
 */
struct tpd_tpinfo_t {
	unsigned int chip_model_id;
	unsigned int chip_part_id;
	unsigned int chip_ver;
	unsigned int module_id;
	unsigned int firmware_ver;
	unsigned int config_ver;
	unsigned int display_ver;
	unsigned int i2c_addr;
	unsigned int i2c_type;
	char tp_name[MAX_VENDOR_NAME_LEN];
	char vendor_name[MAX_VENDOR_NAME_LEN];
};

struct tpd_classdev_t {
	const char *name;
	int b_force_upgrade;
	int fw_compare_result;
	int b_gesture_enable;
	int b_smart_cover_enable;
	bool TP_have_registered;
	bool tp_suspend;
	bool lcd_reset_processing;
	bool tp_suspend_write_gesture;
	int reg_char_num;

	int (*tp_i2c_reg_read)(struct tpd_classdev_t *cdev, char addr, u8 *data, int len);
	int (*tp_i2c_reg_write)(struct tpd_classdev_t *cdev, char addr, u8 *data, int len);
	int (*tp_i2c_16bor32b_reg_read)(struct tpd_classdev_t *cdev, u32 addr, u8 *data, int len);
	int (*tp_i2c_16bor32b_reg_write)(struct tpd_classdev_t *cdev, u32 addr, u8 *data, int len);
	int (*tp_fw_upgrade)(struct tpd_classdev_t *cdev, char *fwname, int fwname_len);
	int (*read_block)(struct tpd_classdev_t *cdev, u16 addr, u8 *buf, int len);
	int (*write_block)(struct tpd_classdev_t *cdev, u16 addr, u8 *buf, int len);
	int (*get_tpinfo)(struct tpd_classdev_t *cdev);
	int (*get_gesture)(struct tpd_classdev_t *cdev);
	int (*wake_gesture)(struct tpd_classdev_t *cdev, int enable);
	int (*get_smart_cover)(struct tpd_classdev_t *cdev);
	int (*set_smart_cover)(struct tpd_classdev_t *cdev, int enable);
	int (*tp_suspend_show)(struct tpd_classdev_t *cdev);
	int (*set_tp_suspend)(struct tpd_classdev_t *cdev, int enable);
	bool (*tpd_suspend_need_awake)(struct tpd_classdev_t *cdev);
	bool (*tpd_esd_check)(struct tpd_classdev_t *cdev);
	void *private;
	void *test_node;    /*added for tp test.*/

	/**for tpd test*/
	int (*tpd_test_set_save_filepath)(struct tpd_classdev_t *cdev, const char *buf);
	int (*tpd_test_get_save_filepath)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_set_save_filename)(struct tpd_classdev_t *cdev, const char *buf);
	int (*tpd_test_get_save_filename)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_set_ini_filepath)(struct tpd_classdev_t *cdev, const char *buf);
	int (*tpd_test_get_ini_filepath)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_set_filename)(struct tpd_classdev_t *cdev, const char *buf);
	int (*tpd_test_get_filename)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_set_cmd)(struct tpd_classdev_t *cdev, const char *buf);
	int (*tpd_test_get_cmd)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_set_node_data_type)(struct tpd_classdev_t *cdev, const char *buf);
	int (*tpd_test_get_node_data)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_get_channel_info)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_get_result)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_get_tp_rawdata)(char *buffer, int length);
	int (*tpd_gpio_shutdown)(void);

	struct mutex cmd_mutex;
	char lcm_info[64];
	char lcm_chip_info[64];

	struct tpd_tpinfo_t ic_tpinfo;
	struct device		*dev;
	struct list_head	 node;
};

extern struct tpd_classdev_t tpd_fw_cdev;
extern int tpd_classdev_register(struct device *parent, struct tpd_classdev_t
*tsp_fw_cdev);

extern unsigned int DISP_GetScreenHeight(void);
extern unsigned int DISP_GetScreenWidth(void);
#endif	/* __TSP_FW_CLASS_H_INCLUDED */

