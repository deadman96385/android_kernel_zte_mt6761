/************************************************************************
*
* File Name: himax_mtk_interface.c
*
*  *   Version: v1.0
*
************************************************************************/
#include <linux/pinctrl/consumer.h>
#include "himax_platform.h"
#include "himax_common.h"

struct himax_i2c_platform_data *himax_pdata = NULL;

int himax_irq_direction_input(void)
{
	int ret = 0;

	ret = pinctrl_select_state(himax_pdata->ts_pinctrl, himax_pdata->pinctrl_int_input);
	if (ret < 0) {
		E("failed to select pin to irq-pin state");
	}

	return ret;
}
int himax_rst_output(int level)
{
	int ret = 0;

	if (level == 0) {
		ret = pinctrl_select_state(himax_pdata->ts_pinctrl, himax_pdata->pinctrl_rst_output0);
		if (ret < 0) {
			E("failed to select pin to rst pin state 0");
		}
	} else {
		ret = pinctrl_select_state(himax_pdata->ts_pinctrl, himax_pdata->pinctrl_rst_output1);
		if (ret < 0) {
			E("failed to select pin to rst pin state 1");
		}
	}

	return ret;
}

