// h文件
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "gpioled_my.c"
#include "fops.h"
/*
* @description		: 打开设备
* @param - inode 	: 传递给驱动的inode
* @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
* 					  一般在open的时候将private_data指向设备结构体。
* @return 			: 0 成功;其他 失败
*/
static int led_open(struct inode *inode, struct file *filp)
{
	 /* 设置私有数据 */
	return 0;
}

/*
 * @description		: 从设备读取数据
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

void led_ctrl(struct gpioled_dev *dev, unsigned char ledstat)
{
	if (ledstat == LEDON)
	{
		gpio_set_value(dev->led_gpio, 0); /* 打开LED灯 *****************************************/
	}
	else if (ledstat == LEDOFF)
	{
		gpio_set_value(dev->led_gpio, 1); /* 关闭LED灯 *****************************************/
	}
	else if (ledstat == LEDTWINKE)
	{
		// 灯闪烁
		ssleep(1);
		gpio_set_value(dev->led_gpio, 0);
		ssleep(1);
		gpio_set_value(dev->led_gpio, 1);
	}
}

/*
 * @description		: 向设备写数据
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	unsigned char databuf[1];
	unsigned char ledstat;
	struct gpioled_dev *dev = filp->private_data;

	retvalue = copy_from_user(databuf, buf, cnt); /* 接收APP发送过来的数据 */
	if (retvalue < 0)
	{
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0]; /* 获取状态值 */

	led_ctrl(dev, ledstat);
	// if (ledstat == LEDON)
	// {
	// 	gpio_set_value(dev->led_gpio, 0); /* 打开LED灯 *****************************************/
	// }
	// else if (ledstat == LEDOFF)
	// {
	// 	gpio_set_value(dev->led_gpio, 1); /* 关闭LED灯 *****************************************/
	// }
	// else if (ledstat == LEDtwinkle)
	// {
	// 	// 灯闪烁
	// 	ssleep(1);
	// 	gpio_set_value(dev->led_gpio, 0);
	// 	ssleep(1);
	// 	gpio_set_value(dev->led_gpio, 1);
	// }

	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 4. 定义设备操作函数 fops */
static struct file_operations gpioled_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};
