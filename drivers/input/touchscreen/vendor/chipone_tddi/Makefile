ccflags-y += -I$(srctree)/drivers/input/touchscreen/vendor/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/vendor/chipone_tddi/firmware_config/$(CONFIG_TS_FIRMWARE)/
obj-$(CONFIG_TOUCHSCREEN_CHIPONE) += chipone-tddi.o

chipone-tddi-objs := cts_i2c_driver.o cts_core.o cts_platform.o cts_sfctrlv2.o cts_spi_flash.o cts_firmware.o cts_tool.o cts_sysfs.o cts_test.o cts_selftest.o cts_selftest_ini.o chipone_common_interface.o

