#
# Makefile for the Fingerprint sensor driver.
#

ccflags-y := -Wformat

obj-$(CONFIG_SENSORS_FINGERPRINT)		+= fingerprint_common.o fingerprint_sysfs.o
obj-$(CONFIG_SENSORS_FINGERPRINT_LSI)		+= fingerprint_common_lsi.o
obj-$(CONFIG_SENSORS_FINGERPRINT_QCOM)		+= fingerprint_common_qcom.o
obj-$(CONFIG_SENSORS_FINGERPRINT_MTK)		+= fingerprint_common_mtk.o

obj-$(CONFIG_SENSORS_ET5XX)			+= et5xx.o
et5xx-y						:= et5xx-spi.o et5xx-spi_data_transfer.o

obj-$(CONFIG_SENSORS_ET7XX)			+= et7xx.o
et7xx-y						:= et7xx-spi.o et7xx-spi_data_transfer.o

obj-$(CONFIG_SENSORS_GW9558X)			+= gw9558x.o
gw9558x-y					:= gw9558x_common.o gw9558x_spidev.o
