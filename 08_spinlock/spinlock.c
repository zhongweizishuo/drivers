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
文件名		: spinlock.c
版本	   	: V1.0 2024 0114
描述	   	: gpio子系统驱动LED灯。
其他	   	: 自旋锁操作实验，使用自旋锁来实现对实现设备的互斥访问
			: 本节实验重点就是使用spinlock来实现一次只能允许一个应用访问LED;
			：维护dev_stats数值（表示设备占用状态：0 未被使用；>0 被占用），
			dev_stats数值加减操作过程需要上锁，上锁分三步走：上锁，检查or操作数据，解锁
***************************************************************/
#define GPIOLED_CNT 1		   /* 设备号个数 */
#define GPIOLED_NAME "gpioled" /* 名字 */
#define LEDOFF 0			   /* 关灯 */
#define LEDON 1				   /* 开灯 */
#define LEDtwinkle 3		   /* 灯闪烁 */

/* gpioled设备结构体 */
struct gpioled_dev
{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备 	 */
	int major;				/* 主设备号	  */
	int minor;				/* 次设备号   */
	struct device_node *nd; /* 设备节点 */
	int led_gpio;			/* led所使用的GPIO编号*/
	// 上锁分三步：上锁，检查or操作数据，解锁
	int dev_stats;			/* 设备状态：0，未被使用；>0 被占用************************/
	spinlock_t lock;		/* lock 自旋锁 ******************************************/
};

struct gpioled_dev gpioled; /* led设备 */

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
	// 通过检查与操作dev_stats的值来表示设备是否被占用
	unsigned long flags;
	filp->private_data = &gpioled; // 设置私有数据

	// 上锁，检查or操作数值，解锁
	spin_lock_irqsave(&gpioled.lock,flags);
	if(gpioled.dev_stats >0){	// 被使用，解锁并报错码
		spin_unlock_irqrestore(&gpioled.lock,flags);
		return -EBUSY;
	}else if(gpioled.dev_stats == 0){ //未被使用，+1标记使用
		gpioled.dev_stats ++;
		spin_unlock_irqrestore(&gpioled.lock,flags);	//解锁
	}

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

	if (ledstat == LEDON)
	{
		gpio_set_value(dev->led_gpio, 0); /* 打开LED灯 */
	}
	else if (ledstat == LEDOFF)
	{
		gpio_set_value(dev->led_gpio, 1); /* 关闭LED灯 */
	}
	else if (ledstat == LEDtwinkle)
	{
		ssleep(1);
		gpio_set_value(dev->led_gpio, 0); // 灯闪烁
		ssleep(1);
		gpio_set_value(dev->led_gpio, 1);
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
	unsigned long flags;
	struct gpioled_dev *dev = filp->private_data;
	// 关闭驱动时三步:上锁，dev_stats -1 ，解锁****************************************
	spin_lock_irqsave(&dev->lock, flags);// 上锁
	if(dev->dev_stats >0 ){// dev_stats -1 
		dev->dev_stats--;
	}
	spin_unlock_irqrestore(&dev->lock,flags);// 解锁
	return 0;
}

/* 设备操作函数 */
static struct file_operations gpioled_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.read = led_read,
	.write = led_write,
	.release = led_release,
};

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init led_init(void)
{
	int ret = 0;
	const char *str;

	/* 1. initialize spinlock lock *****************************************/
	spin_lock_init(&gpioled.lock);

	/* 设置LED所使用的GPIO */
	/* 1、获取设备节点：gpioled */
	gpioled.nd = of_find_node_by_path("/gpioled");
	if (gpioled.nd == NULL)
	{
		printk("gpioled node not find!\r\n");
		return -EINVAL;
	}

	/* 2.读取status属性 */
	ret = of_property_read_string(gpioled.nd, "status", &str);
	if (ret < 0)
		return -EINVAL;

	if (strcmp(str, "okay"))
		return -EINVAL;

	/* 3、获取compatible属性值并进行匹配 */
	ret = of_property_read_string(gpioled.nd, "compatible", &str);
	if (ret < 0)
	{
		printk("gpioled: Failed to get compatible property\n");
		return -EINVAL;
	}

	if (strcmp(str, "alientek,led"))
	{
		printk("gpioled: Compatible match failed\n");
		return -EINVAL;
	}

	/* 4、 获取设备树中的gpio属性，得到LED所使用的LED编号 */
	gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpio", 0);
	if (gpioled.led_gpio < 0)
	{
		printk("can't get led-gpio");
		return -EINVAL;
	}
	printk("led-gpio num = %d\r\n", gpioled.led_gpio);

	/* 5.向gpio子系统申请使用GPIO ***********************************************************/
	ret = gpio_request(gpioled.led_gpio, "LED-GPIO");
	if (ret)
	{
		printk(KERN_ERR "gpioled: Failed to request led-gpio\n");
		return ret;
	}

	/* 6、设置PI0为输出，并且输出高电平，默认关闭LED灯*****************************************/
	ret = gpio_direction_output(gpioled.led_gpio, 1);
	if (ret < 0)
	{
		printk("can't set gpio!\r\n");
	}

	/* 注册字符设备驱动 *********************************************************************/
	/* 1、创建设备号 */
	if (gpioled.major)
	{ /*  定义了设备号 */
		gpioled.devid = MKDEV(gpioled.major, 0);
		ret = register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
		if (ret < 0)
		{
			pr_err("cannot register %s char driver [ret=%d]\n", GPIOLED_NAME, GPIOLED_CNT);
			goto free_gpio;
		}
	}
	else
	{																			 /* 没有定义设备号 */
		ret = alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME); /* 申请设备号*/
		if (ret < 0)
		{
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", GPIOLED_NAME, ret);
			goto free_gpio;
		}
		gpioled.major = MAJOR(gpioled.devid); /* 获取分配号的主设备号 */
		gpioled.minor = MINOR(gpioled.devid); /* 获取分配号的次设备号 */
	}
	printk("gpioled major=%d,minor=%d\r\n", gpioled.major, gpioled.minor);

	/* 2、初始化cdev */
	gpioled.cdev.owner = THIS_MODULE;
	cdev_init(&gpioled.cdev, &gpioled_fops);

	/* 3、添加一个cdev */
	cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);
	if (ret < 0)
		goto del_unregister;

	/* 4、创建类 */
	gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
	if (IS_ERR(gpioled.class))
	{
		goto del_cdev;
	}

	/* 5、创建设备 */
	gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
	if (IS_ERR(gpioled.device))
	{
		goto destroy_class;
	}
	return 0;

destroy_class:
	class_destroy(gpioled.class);
del_cdev:
	cdev_del(&gpioled.cdev);
del_unregister:
	unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
free_gpio:
	gpio_free(gpioled.led_gpio);
	return -EIO;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit led_exit(void)
{
	/* 注销字符设备驱动 */
	cdev_del(&gpioled.cdev);							  /*  删除cdev */
	unregister_chrdev_region(gpioled.devid, GPIOLED_CNT); /* 注销设备号 */
	device_destroy(gpioled.class, gpioled.devid);		  /* 注销设备 */
	class_destroy(gpioled.class);						  /* 注销类 */
	gpio_free(gpioled.led_gpio);						  /* 释放GPIO */
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhong");
MODULE_INFO(intree, "Y");