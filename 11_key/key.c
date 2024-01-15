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
文件名		: mutex.c
版本	   	: V1.0 2024 0115
描述	   	: gpio子系统驱动按键。
其他	   	: 使用原子操作来实现对实现设备的互斥访问。
			：read()来读取输入（按键值）；实际中使用input子系统用来输入；
***************************************************************/
#define KEY_CNT 1	   /* 设备号个数 */
#define KEY_NAME "key" /* 名字 */
#define KEY0VALUE 0XF0 /* 按键值*/
#define INVAKEY 0X00 /* 无效的按键值*/

/* key设备结构体 */
struct key_dev
{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备 	 */
	int major;				/* 主设备号	  */
	int minor;				/* 次设备号   */
	struct device_node *nd; /* 设备节点 */
	int key_gpio;			/* key所使用的GPIO编号*/
	atomic_t keyvalue;	/*按键值*/
};

struct key_dev key; /* key设备 */

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int key_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &key; // 设置私有数据


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
static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	int ret =0;
	int value;
	struct key_dev *dev = filp->private_data;
	
	if(gpio_get_value(dev->key_gpio)==0){
		while(!gpio_get_value(dev->key_gpio));
		atomic_set(&dev->keyvalue, KEY0VALUE);
	}else{
		atomic_set(&dev->keyvalue, INVAKEY);
	}
	value = atomic_read(&dev->keyvalue);
	ret = copy_to_user(buf, &value, sizeof(value));
	return ret;
	// ret从内核空间转到用户空间，
	// 由read(fd,readbuf,sizeof())的fd接收，并转给readbuf供用户空间使用	
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
	struct key_dev *dev = filp->private_data;
	gpio_free(dev->key_gpio);
	return 0;
}

/* 设备操作函数 */
static struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.read = key_read,
	.write = key_write,
	.release = key_release,
};

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
// 注意不能用key_init，否则会有命名冲突 

static int __init mykey_init(void)
{
	int ret = 0;
	const char *str;

	/* 1. 初始化原子变量  *****************************************/
	key.keyvalue = (atomic_t)ATOMIC_INIT(0);
	// 2. 设置初始值 0x00
	atomic_set(&key.keyvalue, INVAKEY);

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
		printk("key: failed to get compatible property\n");
		return -EINVAL;
	}

	if (strcmp(str, "zhong,key"))
	{
		printk("key: Compatible match failed\n");
		return -EINVAL;
	}

	/* 4、 获取设备树中的gpio属性，得到key所使用的key编号 */
	key.key_gpio = of_get_named_gpio(key.nd, "key-gpio", 0);
	if (key.key_gpio < 0)
	{
		printk("can't get key-gpio");
		return -EINVAL;
	}
	printk("key-gpio num = %d\r\n", key.key_gpio);

	/* 5.向gpio子系统申请使用GPIO *************/
	ret = gpio_request(key.key_gpio, "KEY-GPIO");
	if (ret)
	{
		printk(KERN_ERR "key: failed to request key-gpio\n");
		return ret;
	}

	/* 6、设置PG3为输入模式****************/
	ret = gpio_direction_input(key.key_gpio);
	if (ret < 0){
		printk("can't set gpio!\r\n");
	}

	/* 注册字符设备驱动 *********************************************************************/
	/* 1、创建设备号 */
	if (key.major)
	{ /*  定义了设备号 */
		key.devid = MKDEV(key.major, 0);
		ret = register_chrdev_region(key.devid, KEY_CNT, KEY_NAME);
		if (ret < 0)
		{
			pr_err("cannot register %s char driver [ret=%d]\n", KEY_NAME, KEY_CNT);
			return -EIO;
		}
	}
	else
	{																 /* 没有定义设备号 */
		ret = alloc_chrdev_region(&key.devid, 0, KEY_CNT, KEY_NAME); /* 申请设备号*/
		if (ret < 0)
		{
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", KEY_NAME, ret);
			return -EIO;
		}
		key.major = MAJOR(key.devid); /* 获取分配号的主设备号 */
		key.minor = MINOR(key.devid); /* 获取分配号的次设备号 */
	}
	printk("key major=%d,minor=%d\r\n", key.major, key.minor);

	/* 2、初始化cdev */
	key.cdev.owner = THIS_MODULE;
	cdev_init(&key.cdev, &key_fops);

	/* 3、添加一个cdev */
	cdev_add(&key.cdev, key.devid, KEY_CNT);
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
	return 0;

destroy_class:
	class_destroy(key.class);
del_cdev:
	cdev_del(&key.cdev);
del_unregister:
	unregister_chrdev_region(key.devid, KEY_CNT);
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
	device_destroy(key.class, key.devid);		  /* 注销设备 */
	class_destroy(key.class);					  /* 注销类 */
}

module_init(mykey_init);
module_exit(mykey_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhong");
MODULE_INFO(intree, "Y");