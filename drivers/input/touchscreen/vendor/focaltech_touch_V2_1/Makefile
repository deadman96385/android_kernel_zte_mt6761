#
# Makefile for the focaltech touchscreen drivers.
#

# Each configuration option enables a list of files.
ccflags-y += -I$(srctree)/drivers/input/touchscreen/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/vendor
ccflags-y += -I$(srctree)/drivers/input/touchscreen/vendor/focaltech_touch_V2_1/firmware_config/$(CONFIG_TS_FIRMWARE)/

obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1)	+=  focaltech_core.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1)	+=  focaltech_ex_fun.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1)	+=  focaltech_ex_mode.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1)	+=  focaltech_flash.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1)	+=  focaltech_gesture.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1)	+=  focaltech_esdcheck.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1)   +=  focaltech_i2c.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1)   +=  focaltech_point_report_check.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1)   +=  focaltech_flash/
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1)   +=  focaltech_test/
obj-$(CONFIG_TS_FTS_MTK_INTERFACE)	+= focaltech_mtk_interface.o

