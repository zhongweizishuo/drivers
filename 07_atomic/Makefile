KERNELDIR := /home/alientek/linux/atk-mp1/linux/my_linux/linux-5.4.31
CURRENT_PATH := $(shell pwd)

#  注意:目标文件的xxx.o文件名与源文件xxx.c必须保持一致
obj-m := atomic.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean