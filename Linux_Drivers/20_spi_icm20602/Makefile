KERNELDIR := /home/yeap/linux/IMX6ULL/linux_nxp
CURRENT_PATH := $(shell pwd)
obj-m := icm20602.o

build: kernel_modules

kernel_modules: 
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean