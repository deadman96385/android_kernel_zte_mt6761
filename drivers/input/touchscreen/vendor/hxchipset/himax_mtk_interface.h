/************************************************************************
*
* File Name: himax_mtk_interface.c
*
*  *   Version: v1.0
*
************************************************************************/
#include <linux/of_irq.h>

extern struct himax_i2c_platform_data *himax_pdata;

extern int himax_irq_direction_input(void);
extern int himax_rst_output(int level);

