KERNELDIR := /home/alientek/linux/atk-mp1/linux/my_linux/linux-5.4.31
CURRENT_PATH := $(shell pwd)

# 将led.c 编译得到led.o文件
obj-m := led.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
