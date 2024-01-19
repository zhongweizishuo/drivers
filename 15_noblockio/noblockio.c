/***************************************************************
章节：		[32.3.2]
文件名		: noblockio.c
作者	  	: zhong
版本	   	: V1.0
描述	   	: Linux  非阻塞IO
其他	   	:select poll epoll选用poll
***************************************************************/
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
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/poll.h> /* poll */

#define KEY_CNT 1	   /* 设备号个数 	*/
#define KEY_NAME "key" /* 名字 		*/

/* 定义按键三种状态************************ */
enum key_status
{
	KEY_PRESS = 0, /* 按键按下 */
	KEY_RELEASE,   /* 按键松开 */
	KEY_KEEP,	   /* 按键状态保持 */
};

/* key设备结构体 */
struct key_dev
{
	dev_t devid;			  /* 设备号 	 */
	struct cdev cdev;		  /* cdev 	*/
	struct class *class;	  /* 类 		*/
	struct device *device;	  /* 设备 	 */
	struct device_node *nd;	  /* 设备节点 */
	int key_gpio;			  /* key所使用的GPIO编号		*/
	struct timer_list timer;  /* 定时器，实现按键去抖 */
	int irq_num;			  /* 按键IO对应的中断号   */
	atomic_t status;		  /*按键状态**************************/
	wait_queue_head_t r_wait; /* 等待队列头**************************/
};

static struct key_dev key;	  /* 按键设备 */
static int status = KEY_KEEP; /* 按键状态 */

// 中断处理函数：触发中断之后，开启计时器，延时15ms
static irqreturn_t key_interrupt(int irq, void *dev_id)
{
	/* 按键防抖处理，开启定时器延时15ms */
	mod_timer(&key.timer, jiffies + msecs_to_jiffies(15));
	return IRQ_HANDLED;
}

/*
 * @description	: 使用dts初始化按键IO:对设备树属性解析，获取key节点
 * 				  open函数打开驱动的时候初始化按键所使用的GPIO引脚。
 * @param 		: 无
 * @return 		: 无
 */
static int key_parse_dt(void)
{
	int ret;
	const char *str;

	/* 设置key所使用的GPIO */
	/* 1、获取设备节点：key */
	key.nd = of_find_node_by_path("/key");
	if (key.nd == NULL)
	{
		printk("key node not find!\r\n");
		return -EINVAL;
	}

	/* 2.读取status属性 */
	ret = of_property_read_string(key.nd, "status", &str);
	if (ret < 0)
		return -EINVAL;

	if (strcmp(str, "okay"))
		return -EINVAL;

	/* 3、获取compatible属性值并进行匹配 */
	ret = of_property_read_string(key.nd, "compatible", &str);
	if (ret < 0)
	{
		printk("key: Failed to get compatible property\n");
		return -EINVAL;
	}

	if (strcmp(str, "zhong,key"))
	{
		printk("key: Compatible match failed\n");
		return -EINVAL;
	}

	/* 4、 获取设备树中的gpio属性，得到KEY0所使用的KYE编号 */
	key.key_gpio = of_get_named_gpio(key.nd, "key-gpio", 0);
	if (key.key_gpio < 0)
	{
		printk("can't get key-gpio");
		return -EINVAL;
	}

	/* 5 、获取GPIO对应的中断号 **************************/
	key.irq_num = irq_of_parse_and_map(key.nd, 0);
	if (!key.irq_num)
	{
		return -EINVAL;
	}

	printk("key-gpio num = %d\r\n", key.key_gpio);
	return 0;
}
/* 对GPIO与对应的中断进行初始化 **************************/
static int key_gpio_init(void)
{
	int ret;
	unsigned long irq_flags;

	// gpio申请
	ret = gpio_request(key.key_gpio, "KEY0");
	if (ret)
	{
		printk(KERN_ERR "key: Failed to request key-gpio\n");
		return ret;
	}

	/* 将GPIO设置为输入模式 */
	gpio_direction_input(key.key_gpio);

	/* 获取设备树中key0节点指定的中断触发类型 */
	irq_flags = irq_get_trigger_type(key.irq_num);
	if (IRQF_TRIGGER_NONE == irq_flags)
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;

	/* 申请中断：默认会自动使能*/
	ret = request_irq(key.irq_num, key_interrupt, irq_flags, "Key0_IRQ", NULL);
	if (ret)
	{
		gpio_free(key.key_gpio);
		return ret;
	}

	return 0;
}

/*
 * @description	: 定时器定时函数
 *
 * @param 	timer_list	:未使用
 * @return 		: 无
 */
static void key_timer_function(struct timer_list *arg)
{
	static int last_val = 1;
	int current_val;

	/* 2. 读取按键值并判断按键当前状态 status */
	current_val = gpio_get_value(key.key_gpio);
	if ((0 == current_val) && last_val)
	{										/* 按下 :1 --> 0 */
		atomic_set(&key.status, KEY_PRESS); // 将key.status设置为KEY_PRESS的值
		wake_up_interruptible(&key.r_wait); // 唤醒r_wait队列头中的所有队列
	}
	else if (1 == current_val && !last_val)
	{										  /* 松开 : 0 -->1*/
		atomic_set(&key.status, KEY_RELEASE); // 将key.status设置为KEY_PRESS的值
		wake_up_interruptible(&key.r_wait);	  // 唤醒r_wait队列头中的所有队列
	}
	else
		atomic_set(&key.status, KEY_KEEP); // 状态保持

	last_val = current_val;
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int key_open(struct inode *inode, struct file *filp)
{
	return 0;
}

/*
 * @description     : 从设备读取数据，对应用户空间的App的read()函数
 * @param – filp        : 要打开的设备文件(文件描述符)
 * @param – buf     : 返回给用户空间的数据缓冲区
 * @param – cnt     : 要读取的数据长度
 * @param – offt        : 相对于文件首地址的偏移
 * @return          : 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t key_read(struct file *filp, char __user *buf,
						size_t cnt, loff_t *offt)
{
	int ret;

	// 判断是不是非阻塞IO，是的话判断按键状态是不是发送变化，没有变化就返回-EAGAIN;
	if (filp->f_flags & O_NONBLOCK)
	{ /* 非阻塞方式访问********************************* */
		if (KEY_KEEP != atomic_read(&key.status))
		{
			return -EAGAIN;
		}
	}
	else
	{
		/* 加入等待序列，只有按键变化时，才会被唤醒 ***************************************/
		ret = wait_event_interruptible(key.r_wait, KEY_KEEP != atomic_read(&key.status));
		if (ret)
		{
			return ret;
		}
	}
	/* 2. 将按键状态信息发送给应用程序 */
	ret = copy_to_user(buf, &status, sizeof(int));

	/* 状态重置 */
	atomic_set(&key.status, KEY_KEEP);

	return ret;
}

/*
 * @description		: 向设备写数据
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t key_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int key_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static unsigned int key_poll(struct file *filp, struct poll_table_struct *wait){
	unsigned int mask =0;
	poll_wait(filp, &key.r_wait, wait);
	if (KEY_KEEP != atomic_read(&key.status))
		mask = POLLIN | POLLRDNORM;	///* 返回PLLIN,b */
	return mask;
}

/* 设备操作函数 fops 结构体*/
static struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.read = key_read, // 按键检测，read()是核心
	.write = key_write,
	.release = key_release,
	.poll = key_poll,
};

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init mykey_init(void)
{
	int ret;

	/* 等待队列头 */
	init_waitqueue_head(&key.r_wait);
	/* 初始化按键状态 */
	atomic_set(&key.status, KEY_KEEP);

	/* 设备树解析 */
	ret = key_parse_dt();
	if (ret)
		return ret;

	/* GPIO 中断初始化 */
	ret = key_gpio_init();
	if (ret)
		return ret;

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	ret = alloc_chrdev_region(&key.devid, 0, KEY_CNT, KEY_NAME); /* 申请设备号 */
	if (ret < 0)
	{
		pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", KEY_NAME, ret);
		goto free_gpio;
	}

	/* 2、初始化cdev */
	key.cdev.owner = THIS_MODULE;
	cdev_init(&key.cdev, &key_fops);

	/* 3、添加一个cdev */
	ret = cdev_add(&key.cdev, key.devid, KEY_CNT);
	if (ret < 0)
		goto del_unregister;

	/* 4、创建类 */
	key.class = class_create(THIS_MODULE, KEY_NAME);
	if (IS_ERR(key.class))
	{
		goto del_cdev;
	}

	/* 5、创建设备 */
	key.device = device_create(key.class, NULL, key.devid, NULL, KEY_NAME);
	if (IS_ERR(key.device))
	{
		goto destroy_class;
	}

	/* 6、初始化timer，设置定时器处理函数,还未设置周期，所有不会激活定时器 **************/
	timer_setup(&key.timer, key_timer_function, 0);

	return 0;

destroy_class:
	class_destroy(key.class);
del_cdev:
	cdev_del(&key.cdev);
del_unregister:
	unregister_chrdev_region(key.devid, KEY_CNT);
free_gpio:
	free_irq(key.irq_num, NULL);
	gpio_free(key.key_gpio);
	return -EIO;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit mykey_exit(void)
{
	/* 注销字符设备驱动 */
	cdev_del(&key.cdev);						  /*  删除cdev */
	unregister_chrdev_region(key.devid, KEY_CNT); /* 注销设备号 */
	del_timer_sync(&key.timer);					  /* 删除timer ********************/
	device_destroy(key.class, key.devid);		  /*注销设备 */
	class_destroy(key.class);					  /* 注销类 */
	free_irq(key.irq_num, NULL);				  /* 释放中断 ***********************/
	gpio_free(key.key_gpio);					  /* 释放IO */
}

module_init(mykey_init);
module_exit(mykey_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");