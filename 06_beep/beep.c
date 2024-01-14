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
/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: beep.c
作者	  	: zhong
版本	   	: V1.0
描述	   	: gpio子系统驱动beep灯。
其他	   	: 无
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2020/12/30 正点原子Linux团队创建
***************************************************************/
#define BEEP_CNT			1		  	/* 设备号个数 */
#define BEEP_NAME		"beep"	/* 名字 */
#define BEEPOFF 0
#define BEEPON 1
#define BEEPtwinkle 3 /* beep 间隔响 */

/* beep设备结构体 */
struct beep_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备 	 */
	int major;				/* 主设备号	  */
	int minor;				/* 次设备号   */
	struct device_node	*nd; /* 设备节点 */
	int beep_gpio;			/* beep所使用的GPIO编号		*****************************************/
};

struct beep_dev beep;	/* beep设备 */

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int beep_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &beep; /* 设置私有数据 */
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
static ssize_t beep_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t beep_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	unsigned char databuf[1];
	unsigned char beepstat;
	struct beep_dev *dev = filp->private_data;

	retvalue = copy_from_user(databuf, buf, cnt); /* 接收APP发送过来的数据 */
	if(retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	beepstat = databuf[0];		/* 获取状态值 */
	// 设置默认输出高电平 1 ，就是beep关闭
	if(beepstat == BEEPON) {				
		gpio_set_value(dev->beep_gpio, 0);	/* 输入BEEPON=1, 设置低电平0：打开beep *****************************************/
	} else if(beepstat == BEEPOFF) {
		gpio_set_value(dev->beep_gpio, 1);	/* 输入BEEPOFF=0, 设置高电平1：关闭beep *****************************************/
	}else if(beepstat == BEEPtwinkle){
		// 间隔响
		ssleep(0.5);
		gpio_set_value(dev->beep_gpio, 0);
		ssleep(0.5);
		gpio_set_value(dev->beep_gpio, 1);
	}
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int beep_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 设备操作函数 */
static struct file_operations beep_fops = {
	.owner = THIS_MODULE,
	.open = beep_open,
	.read = beep_read,
	.write = beep_write,
	.release = 	beep_release,
};

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init beep_init(void)
{
	int ret = 0;
	const char *str;

	/* 设置beep所使用的GPIO */
	/* 1、获取设备节点：beep */
	beep.nd = of_find_node_by_path("/beep");
	if(beep.nd == NULL) {
		printk("beep node not find!\r\n");
		return -EINVAL;
	}

	/* 2.读取status属性 */
	ret = of_property_read_string(beep.nd, "status", &str);
	if(ret < 0) 
	    return -EINVAL;

	if (strcmp(str, "okay"))
        return -EINVAL;
    
	/* 3、获取compatible属性值并进行匹配 */
	ret = of_property_read_string(beep.nd, "compatible", &str);
	if(ret < 0) {
		printk("beep: failed to get compatible property\n");
		return -EINVAL;
	}

    if (strcmp(str, "zhong,beep")) {
        printk("beep: Compatible match failed\n");
        return -EINVAL;
    }

	/* 4、 获取设备树中的gpio属性，得到beep所使用的beep编号 */
	beep.beep_gpio = of_get_named_gpio(beep.nd, "beep-gpio", 0);
	if(beep.beep_gpio < 0) {
		printk("can't get beep-gpio");
		return -EINVAL;
	}
	printk("beep-gpio num = %d\r\n", beep.beep_gpio);

	/* 5.向gpio子系统申请使用GPIO ***********************************************************/
	ret = gpio_request(beep.beep_gpio, "BEEP-GPIO");
    if (ret) {
        printk(KERN_ERR "beep: failed to request beep-gpio\n");
        return ret;
	}

	/* 6、设置PC7为输出，并且输出高电平，默认关闭beep*****************************************/
	ret = gpio_direction_output(beep.beep_gpio, 1);
	if(ret < 0) {
		printk("can't set gpio!\r\n");
	}

	/* 注册字符设备驱动 *********************************************************************/
	/* 1、创建设备号 */
	if (beep.major) {		/*  定义了设备号 */
		beep.devid = MKDEV(beep.major, 0);
		ret = register_chrdev_region(beep.devid, BEEP_CNT, BEEP_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n", BEEP_NAME, BEEP_CNT);
			goto free_gpio;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&beep.devid, 0, BEEP_CNT, BEEP_NAME);	/* 申请设备号*/
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", BEEP_NAME, ret);
			goto free_gpio;
		}
		beep.major = MAJOR(beep.devid);	/* 获取分配号的主设备号 */
		beep.minor = MINOR(beep.devid);	/* 获取分配号的次设备号 */
	}
	printk("beep major=%d,minor=%d\r\n",beep.major, beep.minor);	
	
	/* 2、初始化cdev */
	beep.cdev.owner = THIS_MODULE;
	cdev_init(&beep.cdev, &beep_fops);
	
	/* 3、添加一个cdev */
	cdev_add(&beep.cdev, beep.devid, BEEP_CNT);
	if(ret < 0)
		goto del_unregister;
		
	/* 4、创建类 */
	beep.class = class_create(THIS_MODULE, BEEP_NAME);
	if (IS_ERR(beep.class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	beep.device = device_create(beep.class, NULL, beep.devid, NULL, BEEP_NAME);
	if (IS_ERR(beep.device)) {
		goto destroy_class;
	}
	return 0;
	
destroy_class:
	class_destroy(beep.class);
del_cdev:
	cdev_del(&beep.cdev);
del_unregister:
	unregister_chrdev_region(beep.devid, BEEP_CNT);
free_gpio:
	gpio_free(beep.beep_gpio);//**************************************************
	return -EIO;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit beep_exit(void)
{
	/* 注销字符设备驱动 */
	cdev_del(&beep.cdev);/*  删除cdev */
	unregister_chrdev_region(beep.devid, BEEP_CNT); /* 注销设备号 */
	device_destroy(beep.class, beep.devid);/* 注销设备 */
	class_destroy(beep.class);/* 注销类 */
	gpio_free(beep.beep_gpio); /* 释放GPIO ****************************************/
}

module_init(beep_init);
module_exit(beep_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhong");
MODULE_INFO(intree, "Y");