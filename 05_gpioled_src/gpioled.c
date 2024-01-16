#include "src/driver.hpp"
/***************************************************************
文件名		: gpioled.c
作者	  	: zhong
版本	   	: V1.0
描述	   	: gpio子系统驱动LED灯。
描述	   	: 不同部分进行了分解
***************************************************************/

/* 0. 构建设备结构体 */

/* 1. 实例化一个设备 */
struct gpioled_dev gpioled;	

/* 2. 构建设备操作函数 fops */
static struct file_operations gpioled_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.read = led_read,
	.write = led_write,
	.release = 	led_release,
};

/* 3. 驱动入口函数 */
static int __init led_init(void)
{
	/* 1. 使用dts初始化LED所使用的GPIO */
	dts_led_init(gpioled);

	/* 2. 注册字符设备驱动 */
	regi_chr_dev(gpioled, gpioled_fops);
	
	return -EIO;
}

/* 4. 驱动出口函数 */
static void __exit led_exit(void)
{
	// 注销字符设备驱动
	cdev_del(&gpioled.cdev);/*  删除cdev */
	unregister_chrdev_region(gpioled.devid, GPIOLED_CNT); /* 注销设备号 */
	device_destroy(gpioled.class, gpioled.devid);/* 注销设备 */
	class_destroy(gpioled.class);/* 注销类 */
	gpio_free(gpioled.led_gpio); /* 释放GPIO */
}

/* 5. 驱动入口与出口模块 */
module_init(led_init);
module_exit(led_exit);

/* 6. 驱动模块信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhong");
MODULE_INFO(intree, "Y");