#
# Makefile for the Synaptics DSX touchscreen driver.
#

# Each configuration option enables a list of files.
ccflags-y += -I$(srctree)/drivers/input/touchscreen/vendor/synaptics_2.7/firmware_config/$(CONFIG_TS_FIRMWARE)/
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_I2C_V27) += synaptics_dsx_i2c.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_CORE_V27) += synaptics_dsx_core.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_RMI_DEV_V27) += synaptics_dsx_rmi_dev.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_FW_UPDATE_V27) += synaptics_dsx_fw_update.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_TEST_REPORTING_V27) += synaptics_dsx_test_reporting.o
obj-$(CONFIG_TOUCHSCREEN_SPECIAL_INTERFACE) += synaptics_dsx_test.o
obj-$(CONFIG_TOUCHSCREEN_SPECIAL_INTERFACE) += synaptics_dsx_test_entry.o
obj-$(CONFIG_TOUCHSCREEN_SPECIAL_INTERFACE) += synaptics_dsx_test_parse_ini.o
