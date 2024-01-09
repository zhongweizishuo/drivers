#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: led.c
作者	  	: 正点原子
版本	   	: V1.0
描述	   	: LED驱动文件。
其他	   	: 无
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2020/11/23 正点原子团队创建
***************************************************************/
#define LED_MAJOR		200		/* 主设备号 */
#define LED_NAME		"led" 	/* 设备名字 */

#define LEDOFF 	0				/* 关灯 */
#define LEDON 	1				/* 开灯 */
 
/* 寄存器物理地址 */
#define PERIPH_BASE     		     	(0x40000000)
#define MPU_AHB4_PERIPH_BASE			(PERIPH_BASE + 0x10000000)
#define RCC_BASE        		    	(MPU_AHB4_PERIPH_BASE + 0x0000)	
#define RCC_MP_AHB4ENSETR				(RCC_BASE + 0XA28)
#define GPIOI_BASE						(MPU_AHB4_PERIPH_BASE + 0xA000)	
#define GPIOI_MODER      			    (GPIOI_BASE + 0x0000)	
#define GPIOI_OTYPER      			    (GPIOI_BASE + 0x0004)	
#define GPIOI_OSPEEDR      			    (GPIOI_BASE + 0x0008)	
#define GPIOI_PUPDR      			    (GPIOI_BASE + 0x000C)	
#define GPIOI_BSRR      			    (GPIOI_BASE + 0x0018)


/* 内存映射后的寄存器虚拟地址指针 */
static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI;

/*地址映射*/
static void led_ioremap(void)
{
	MPU_AHB4_PERIPH_RCC_PI = ioremap(RCC_MP_AHB4ENSETR, 4);
	GPIOI_MODER_PI = ioremap(GPIOI_MODER, 4);
  	GPIOI_OTYPER_PI = ioremap(GPIOI_OTYPER, 4);
	GPIOI_OSPEEDR_PI = ioremap(GPIOI_OSPEEDR, 4);
	GPIOI_PUPDR_PI = ioremap(GPIOI_PUPDR, 4);
	GPIOI_BSRR_PI = ioremap(GPIOI_BSRR, 4);
}

/*
 * @description		: LED打开/关闭
 * @param - sta 	: LEDON(0) 打开LED，LEDOFF(1) 关闭LED
 * @return 			: 无
 */
void led_switch(u8 sta)
{
	u32 val = 0;
	if(sta == LEDON) {
		val = readl(GPIOI_BSRR_PI);
		val |= (0x1 << 16);	//开灯
		writel(val, GPIOI_BSRR_PI);
	}else if(sta == LEDOFF) {
		val = readl(GPIOI_BSRR_PI);
		val|= (0x1 << 0);		//关灯
		writel(val, GPIOI_BSRR_PI);
	}	
}

/*
 * @description		: 取消映射
 * @return 			: 无
 */
void led_unmap(void)
{
	/* 取消映射 */
	iounmap(MPU_AHB4_PERIPH_RCC_PI);
	iounmap(GPIOI_MODER_PI);
	iounmap(GPIOI_OTYPER_PI);
	iounmap(GPIOI_OSPEEDR_PI);
	iounmap(GPIOI_PUPDR_PI);
	iounmap(GPIOI_BSRR_PI);
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
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
	// 不需要读取设备的状态，所以也可以不用写
	return 0;
}

/*
 * @description		: 向设备写数据,处理开灯与关灯操作
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

	retvalue = copy_from_user(databuf, buf, cnt); // copy_from_user()执行获取应用程序发送过来的操作信息
	if(retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];		/* 获取状态值 */

	if(ledstat == LEDON) {	
		led_switch(LEDON);		/* 打开LED灯 */
	} else if(ledstat == LEDOFF) {
		led_switch(LEDOFF);		/* 关闭LED灯 */
	}
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

/* 设备操作函数 ：包含以上四个函数*/
static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.read = led_read,
	.write = led_write,
	.release = 	led_release,
};

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init led_init(void)
{
	int retvalue = 0;
	u32 val = 0;

	/* 初始化LED */
	/* 1、寄存器地址映射 */
  	led_ioremap();

	/* 2、使能PI时钟,将bit 8 位置 置1 */
	val = readl(MPU_AHB4_PERIPH_RCC_PI);  // val是32位的，所以使用readl()读取； read long
	val &= ~(0x1 << 8);	/* 清除bit 8 位置以前的设置；取反按位与，不会影响其他31位的数据 */
	val |= (0x1 << 8);	/* 设置新值 */
	writel(val, MPU_AHB4_PERIPH_RCC_PI);

	/* 3、设置PI0通用的输出模式。*/
	val = readl(GPIOI_MODER_PI);
	val &= ~(0x3 << 0);	
	/*  32bit内存的bit 0和bit 1清零：0x3是二进制的0011，左移0位还是0011，0011取反1100，按位与得到xx00，目标达成*/
	val |= (0x1 << 0);	/* bit 0和bit 1 设置01 ,也就是输出模式*/
	writel(val, GPIOI_MODER_PI);

	/* 3、设置PI0为推挽模式。*/
	val = readl(GPIOI_OTYPER_PI);
	val &= ~(0X1 << 0);	/* bit0清零，设置为上拉*/
	writel(val, GPIOI_OTYPER_PI);

	/* 4、设置PI0为高速。*/
	val = readl(GPIOI_OSPEEDR_PI);
	val &= ~(0X3 << 0); /* bit0:1 清零 */
	val |= (0x2 << 0); /* bit0:1 设置为10*/
	writel(val, GPIOI_OSPEEDR_PI);

	/* 5、设置PI0为上拉。*/
	val = readl(GPIOI_PUPDR_PI);
	val &= ~(0X3 << 0); /* bit0:1 清零*/
	val |= (0x1 << 0); /*bit0:1 设置为01*/
	writel(val,GPIOI_PUPDR_PI);

	/* 6、默认关闭LED */
	val = readl(GPIOI_BSRR_PI);
	val |= (0x1 << 0);	
	writel(val, GPIOI_BSRR_PI);

	/* 7、注册字符设备驱动 */
	retvalue = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
	if(retvalue < 0) {
		printk("register chrdev failed!\r\n");
		goto fail_map;
		// 有多个出错出口的函数，我们可以使用goto将函数内的出错指定一个统一的出口，统一处理
	}
	return 0;
	
fail_map:
	led_unmap();
	return -EIO;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit led_exit(void)
{
	printk("led exit!\r\n");
	/* 取消映射 */
	led_unmap();
	/* 注销字符设备驱动 */
	unregister_chrdev(LED_MAJOR, LED_NAME);
}

// 驱动模块的注册与卸载
module_init(led_init);
module_exit(led_exit);

// 模块许可
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");