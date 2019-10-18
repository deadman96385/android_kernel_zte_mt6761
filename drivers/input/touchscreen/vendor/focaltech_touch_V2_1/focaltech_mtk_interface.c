/************************************************************************
*
* File Name: focaltech_mtk_interface.c
*
*  *   Version: v1.0
*
************************************************************************/
#include <linux/pinctrl/consumer.h>
#include "focaltech_core.h"
#include "focaltech_common.h"

struct fts_ts_data *fts_pdata = NULL;

int fts_irq_direction_input(void)
{
	int ret = 0;

	ret = pinctrl_select_state(fts_pdata->pinctrl, fts_pdata->pinctrl_int_input);
	if (ret < 0) {
		FTS_ERROR("failed to select pin to irq-pin state");
	}

	return ret;
}
int fts_rst_output(int level)
{
	int ret = 0;

	if (level == 0) {
		ret = pinctrl_select_state(fts_pdata->pinctrl, fts_pdata->pinctrl_rst_output0);
		if (ret < 0) {
			FTS_ERROR("failed to select pin to rst pin state 0");
		}
	} else {
		ret = pinctrl_select_state(fts_pdata->pinctrl, fts_pdata->pinctrl_rst_output1);
		if (ret < 0) {
			FTS_ERROR("failed to select pin to rst pin state 1");
		}
	}

	return ret;
}

