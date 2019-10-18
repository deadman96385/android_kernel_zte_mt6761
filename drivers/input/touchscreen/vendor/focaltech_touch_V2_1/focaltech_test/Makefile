#drivers/input/touchscreen/focaltech_touch/focaltech_test
#
#Makefile for test module
#

ccflags-y += -I$(srctree)/drivers/input/touchscreen/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/vendor/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/vendor/focaltech_touch_V2_1/firmware_config/$(CONFIG_TS_FIRMWARE)/
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1) += focaltech_test.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1) += focaltech_test_ini.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1) += focaltech_test_detail_threshold.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1) += supported_ic/

