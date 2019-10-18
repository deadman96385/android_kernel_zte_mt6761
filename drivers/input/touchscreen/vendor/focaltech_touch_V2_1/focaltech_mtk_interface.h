/************************************************************************
*
* File Name: focaltech_mtk_interface.c
*
*  *   Version: v1.0
*
************************************************************************/
#include <linux/of_irq.h>

extern struct fts_ts_data *fts_pdata;

extern int fts_irq_direction_input(void);
extern int fts_rst_output(int level);

