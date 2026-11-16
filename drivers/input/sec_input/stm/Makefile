stm_ts-y := stm_i2c.o stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o
ifeq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
  stm_ts_spi-y := stm_spi.o stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o stm_ioctl.o
else
  stm_ts_spi-y := stm_spi.o stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o stm_ioctl.o stm_vendor.o
endif

obj-$(CONFIG_TOUCHSCREEN_STM) += stm_ts.o
obj-$(CONFIG_TOUCHSCREEN_STM_SPI) += stm_ts_spi.o

ccflags-y += -Wformat


