KERNELDIR := /home/alientek/linux/atk-mp1/linux/my_linux/linux-5.4.31
CURRENT_PATH := $(shell pwd)

#  注意:目标文件的dtsled_my.o文件名与源文件dtsled_my.c必须保持一致
# obj-m := gpioled.o
obj-m := spinlock.o

# 依次构建以下4部分
build: kernel_modules clean_files arm_gcc cp2nfs

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean	
	rm -f *App
	
# 新加的部分自动执行脚本------------------------------------------
clean_files:
	rm -f *.o .*.cmd *.mod *.mod.c *.symvers *.order
# 注意，删除所有以.开头，后面跟着任意字符，最后是.cmd的文件;比如删除：
# .spinlock.ko.cmd
# .spinlock.mod.cmd
# 应使用：rm -f .*.cmd；而不是rm -f *.cmd；

# ledApp.c 需要使用公式再提取，并封装
arm_gcc:
	arm-none-linux-gnueabihf-gcc spinlockApp.c -o spinlockApp
cp2nfs:
	cp *.ko *App ~/linux/nfs/rootfs -r