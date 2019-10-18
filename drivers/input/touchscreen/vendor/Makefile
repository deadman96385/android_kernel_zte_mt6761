subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/vendor
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
# In case the platform does NOT support this type of sensors

obj-$(CONFIG_TOUCHSCREEN_VENDOR)	+= tpd_sys.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET)	+= hxchipset/
obj-$(CONFIG_TOUCHSCREEN_FTS_V2_1)	+= focaltech_touch_V2_1/
obj-$(CONFIG_TOUCHSCREEN_TLSC6X)	+= tlsc6x/
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_V27) += synaptics_2.7/
obj-$(CONFIG_TOUCHSCREEN_FTS_V3)	+= focaltech_touch_v3/
obj-$(CONFIG_TOUCHSCREEN_CHIPONE)	+= chipone_tddi/