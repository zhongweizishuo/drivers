# 0. pinctrl 设置更改
1. platform框架下pinctrl配置复用引脚
find ~/linux/atk-mp1/linux/my_linux/linux-5.4.31 -name "pinctrl-stm32.c"
设置stm32_pmx_ops结构体的strict 成员变量默认为 true，我们需要将其改为 false；
重新编译uImage内核：
cd ~/linux/atk-mp1/linux/my_linux/linux-5.4.31
make uImage LOADADDR=0XC2000040 -j8 //编译内核
复制到 ~/linux/tftpboot/ 文件夹
cp arch/arm/boot/uImage  ~/linux/tftpboot/ -f
2. 在此目录下也有uImage的备份

3. 创建pinctrl节点(STM32MP1的引脚pinctrl配置文件)
find ~/linux/atk-mp1/linux/my_linux/linux-5.4.31 -name "stm32mp15-pinctrl.dtsi"
按要求修改


# 1. 注意
### 此文件只是本节课dts修改内容的备份，实际使用时，直接复制并修改以下文件：
/home/alientek/linux/atk-mp1/linux/my_linux/linux-5.4.31/arch/arm/boot/dts/stm32mp157d-atk.dts


1. linux编译完成系统的位置
cd ~/linux/atk-mp1/linux/my_linux/linux-5.4.31/
2. 设备树文件的地址：
cd ~/linux/atk-mp1/linux/my_linux/linux-5.4.31/arch/arm/boot/dts
ls | grep "157d-atk"
2.5 修改设备树文件（参考pdf 24.3.1 ，添加gpio 仅编译修改后的设备树：
cd ~/linux/atk-mp1/linux/my_linux/linux-5.4.31
make dtbs
（3.5同3. 仅编译某个被修改过的设备树：make stm32mp157d-atk.dtb ）
得到新的设备树文件： DTC     arch/arm/boot/dts/stm32mp157d-atk.dtb
5. 使用新的设备树启动内核，新的dtb拷贝到 tftpboot文件夹
cp arch/arm/boot/dts/stm32mp157d-atk.dtb ~/linux/tftpboot/ -f
6.  启动开发板，开发板操作
cd /sys/devices/platform/dtsleds/leds
就可以看到 green red 节点
