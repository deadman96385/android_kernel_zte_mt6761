# Makefile for the focaltech touchscreen drivers.
ccflags-y += -I$(srctree)/drivers/input/touchscreen/vendor/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/vendor/focaltech_touch_v3/focaltech_test/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/vendor/focaltech_touch_v3/firmware_config/$(CONFIG_TS_FIRMWARE)/
obj-$(CONFIG_TOUCHSCREEN_FTS_V3)	+=  focaltech_core.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V3)	+=  focaltech_ex_fun.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V3)	+=  focaltech_ex_mode.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V3)	+=  focaltech_gesture.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V3)	+=  focaltech_esdcheck.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V3)   +=  focaltech_point_report_check.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V3)   +=  focaltech_test/

obj-$(CONFIG_TOUCHSCREEN_FTS_V3)   +=  focaltech_i2c.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V3)	+=  focaltech_flash.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V3)	+= focaltech_common_interface.o
obj-$(CONFIG_TOUCHSCREEN_FTS_V3)   +=  focaltech_flash/
