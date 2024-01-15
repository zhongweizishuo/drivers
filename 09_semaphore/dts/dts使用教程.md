# 1. 注意
### 此文件只是本节课dts修改内容的备份，实际使用时，直接复制并修改以下文件：
/home/alientek/linux/atk-mp1/linux/my_linux/linux-5.4.31/arch/arm/boot/dts/stm32mp157d-atk.dts

1. linux编译完成系统的位置
cd ~/linux/atk-mp1/linux/my_linux/linux-5.4.31/
2. 设备树文件的地址：
cd ~/linux/atk-mp1/linux/my_linux/linux-5.4.31/arch/arm/boot/dts
2.5 修改设备树文件（参考pdf 24.3.1 ，添加gpio 仅编译修改后的设备树：
cd ~/linux/atk-mp1/linux/my_linux/linux-5.4.31
make dtbs
（3.5同3. 仅编译某个被修改过的设备树：make stm32mp157d-atk.dtb ）
得到新的设备树文件： DTC     arch/arm/boot/dts/stm32mp157d-atk.dtb
5. 使用新的设备树启动内核，新的dtb拷贝到 tftpboot文件夹
cp arch/arm/boot/dts/stm32mp157d-atk.dtb ~/linux/tftpboot/ -f
6.  启动开发板，开发板操作
cd /proc/device-tree/
就可以看到修改后的 gpioled 子节点存在
