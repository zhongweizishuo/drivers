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
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
/***************************************************************
文件名		: timer.c				章节【30.3.2】
作者	  	: 正点原子Linux团队
版本	   	: V1.0
描述	   	: Linux内核定时器实验
其他	   	: 无
***************************************************************/
#define TIMER_CNT		1		/* 设备号个数 	*/
#define TIMER_NAME		"timer"	/* 名字 		*/
/*	使用 _IO 宏来创建一个用于控制设备的 ioctl 命令
	格式：_IO(type, number)
	type 是一个8位无符号整数，通常被用来表示设备类型，用于将 ioctl 命令与不同的设备或模块关联起来。
	number 是一个8位无符号整数，用于表示特定的 ioctl 命令。
*/ 
#define CLOSE_CMD 		(_IO(0XEF, 0x1))	/* 关闭定时器 */
#define OPEN_CMD		(_IO(0XEF, 0x2))	/* 打开定时器 */
#define SETPERIOD_CMD	(_IO(0XEF, 0x3))	/* 设置定时器周期命令 */
#define LEDON 			1		/* 开灯 */
#define LEDOFF 			0		/* 关灯 */

/* timer设备结构体 */
struct timer_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备 	 */
	int major;				/* 主设备号	  */
	int minor;				/* 次设备号   */
	struct device_node	*nd; /* 设备节点 */
	int led_gpio;			/* key所使用的GPIO编号		*/
	
	int timeperiod; 		/* 定时周期,单位为ms *************/
	struct timer_list timer;/* 定义一个定时器*/
	spinlock_t lock;		/* 定义自旋锁 */
};

struct timer_dev timerdev;	/* 实例化timer设备 */

/*
 * @description	: 使用led的dts属性初始化LED灯IO，
 * 				  open函数打开驱动的时候，初始化LED灯所使用的GPIO引脚。
 * @param 		: 无
 * @return 		: 无
 */
static int led_init(void)
{
	int ret;
	const char *str;
	
	/* 使用led的dts属性，设置LED所使用的GPIO */
	/* 1、获取设备节点：timerdev */
	timerdev.nd = of_find_node_by_path("/gpioled");
	if(timerdev.nd == NULL) {
		printk("timerdev node not find!\r\n");
		return -EINVAL;
	}

	/* 2.读取status属性 */
	ret = of_property_read_string(timerdev.nd, "status", &str);
	if(ret < 0) 
	    return -EINVAL;

	if (strcmp(str, "okay"))
        return -EINVAL;
    
	/* 3、获取compatible属性值并进行匹配 */
	ret = of_property_read_string(timerdev.nd, "compatible", &str);
	if(ret < 0) {
		printk("timerdev: Failed to get compatible property\n");
		return -EINVAL;
	}

    if (strcmp(str, "alientek,led")) {
        printk("timerdev: Compatible match failed\n");
        return -EINVAL;
    }

	/* 4、 获取设备树中的gpio属性，得到led-gpio所使用的led编号 */
	timerdev.led_gpio = of_get_named_gpio(timerdev.nd, "led-gpio", 0);
	if(timerdev.led_gpio < 0) {
		printk("can't get led-gpio");
		return -EINVAL;
	}
	printk("led-gpio num = %d\r\n", timerdev.led_gpio);

	/* 5.向gpio子系统申请使用GPIO */
	ret = gpio_request(timerdev.led_gpio, "led");
    if (ret) {
        printk(KERN_ERR "timerdev: Failed to request led-gpio\n");
        return ret;
	}

	/* 6、设置PI0为输出，并且输出高电平，默认关闭LED灯 */
	ret = gpio_direction_output(timerdev.led_gpio, 1);
	if(ret < 0) {
		printk("can't set gpio!\r\n");
		return ret;
	}
	return 0;
}

/*
 * @description		: 打开定时器设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。***************************
 * @return 			: 0 成功;其他 失败
 */
static int timer_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	filp->private_data = &timerdev;	/* 设置私有数据 */

	timerdev.timeperiod = 1000;		/* 默认周期为1s */
	ret = led_init();				/* 初始化LED IO */
	if (ret < 0) {
		return ret;
	}

	return 0;
}

/*
 * @description		: ioctl函数：App调用ioctl(),向驱动发送控制信息
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - cmd 	: 应用程序发送过来的命令
 * @param - arg 	: 参数
 * @return 			: 0 成功;其他 失败
 */
static long timer_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct timer_dev *dev =  (struct timer_dev *)filp->private_data;
	int timerperiod;
	unsigned long flags;
	
	switch (cmd) {
		case CLOSE_CMD:		/* 关闭定时器 */
			del_timer_sync(&dev->timer);
			break;
		case OPEN_CMD:		/* 打开定时器 */
			// 上锁，操做数据timerperiod，解锁
			spin_lock_irqsave(&dev->lock, flags);
			timerperiod = dev->timeperiod;
			spin_unlock_irqrestore(&dev->lock, flags);
			mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
			break;
		case SETPERIOD_CMD: /* 设置定时器周期 */
			// 上锁，操做数据timerperiod，解锁
			spin_lock_irqsave(&dev->lock, flags);
			dev->timeperiod = arg;
			spin_unlock_irqrestore(&dev->lock, flags);
			mod_timer(&dev->timer, jiffies + msecs_to_jiffies(arg));
			break;
		default:
			break;
	}
	return 0;
}

/*
 * @description		: 关闭/释放设备；对应App的close(fd);
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int led_release(struct inode *inode, struct file *filp)
{
	struct timer_dev *dev = filp->private_data;
	gpio_set_value(dev->led_gpio, 1);	/* APP结束的时候关闭LED */
	gpio_free(dev->led_gpio);			/* 释放LED				*/
	del_timer_sync(&dev->timer);		/* 关闭定时器 			*/

	return 0;
}

/* 设备操作函数 */
static struct file_operations timer_fops = {
	.owner = THIS_MODULE,
	.open = timer_open,
	.unlocked_ioctl = timer_unlocked_ioctl,	/* 不一定非的是4大金刚函数************* */
	.release = 	led_release,
};

/* 定时器回调函数 */
void timer_function(struct timer_list *arg)
{
	/* 	from_timer是个宏，是对container_of函数的封装，可以根据结构体的成员地址，获取到这个结构体的首地址。
		第一个参数表示结构体dev，第二个参数表示第一个参数里有一个成员timer，第三个参数表示第二个参数timer的类型timer_list,
		最终得到第一个参数的首地址。
	*/
	struct timer_dev *dev = from_timer(dev, arg, timer);
	static int sta = 1;
	int timerperiod;
	unsigned long flags;

	sta = !sta;		/* 每次都取反，实现LED灯反转 */
	gpio_set_value(dev->led_gpio, sta);
	
	/* 重启定时器 */
	spin_lock_irqsave(&dev->lock, flags);
	timerperiod = dev->timeperiod;
	spin_unlock_irqrestore(&dev->lock, flags);
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->timeperiod)); 
 }

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init timer_init(void)
{
	int ret;
	
	/* 初始化自旋锁 */
	spin_lock_init(&timerdev.lock);

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (timerdev.major) {			/*  定义了设备号 */
		timerdev.devid = MKDEV(timerdev.major, 0);
		ret = register_chrdev_region(timerdev.devid, TIMER_CNT, TIMER_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n", TIMER_NAME, TIMER_CNT);
			return -EIO;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&timerdev.devid, 0, TIMER_CNT, TIMER_NAME);	/* 申请设备号 */
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", TIMER_NAME, ret);
			return -EIO;
		}
		timerdev.major = MAJOR(timerdev.devid);	/* 获取分配号的主设备号 */
		timerdev.minor = MINOR(timerdev.devid);	/* 获取分配号的次设备号 */
	}
	printk("timerdev major=%d,minor=%d\r\n",timerdev.major, timerdev.minor);	
	
	/* 2、初始化cdev */
	timerdev.cdev.owner = THIS_MODULE;
	cdev_init(&timerdev.cdev, &timer_fops);
	
	/* 3、添加一个cdev */
	cdev_add(&timerdev.cdev, timerdev.devid, TIMER_CNT);
	if(ret < 0)
		goto del_unregister;
		
	/* 4、创建类 */
	timerdev.class = class_create(THIS_MODULE, TIMER_NAME);
	if (IS_ERR(timerdev.class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	timerdev.device = device_create(timerdev.class, NULL, timerdev.devid, NULL, TIMER_NAME);
	if (IS_ERR(timerdev.device)) {
		goto destroy_class;
	}
	
	/* 6、初始化timer，设置定时器处理函数,还未设置周期，所有不会激活定时器 */
	timer_setup(&timerdev.timer, timer_function, 0);
	
	return 0;

destroy_class:
	device_destroy(timerdev.class, timerdev.devid);
del_cdev:
	cdev_del(&timerdev.cdev);
del_unregister:
	unregister_chrdev_region(timerdev.devid, TIMER_CNT);
	return -EIO;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit timer_exit(void)
{
	del_timer_sync(&timerdev.timer);		/* 删除timer:会等待其他处理器使用完定时再删除*/
#if 0
	del_timer(&timerdev.tiemr);
#endif

	/* 注销字符设备驱动 */
	cdev_del(&timerdev.cdev);/*  删除cdev */
	unregister_chrdev_region(timerdev.devid, TIMER_CNT); /* 注销设备号 */

	device_destroy(timerdev.class, timerdev.devid);
	class_destroy(timerdev.class);
}

module_init(timer_init);
module_exit(timer_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");