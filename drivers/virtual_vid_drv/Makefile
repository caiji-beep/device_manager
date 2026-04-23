KERNELDIR := /home/lsz/linux/IMX6ULL/linux/linux-imx-rel_imx_4.1.15_2.1.0_ga
CURRENT_PATH := $(shell pwd)


obj-m := virtual_camera.o

virtual_camera-objs := virtual_vid_drv.o red.o green.o blue.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
