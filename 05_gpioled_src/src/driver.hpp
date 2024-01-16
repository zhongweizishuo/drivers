// hpp文件
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

#define GPIOLED_CNT			1		  	/* 设备号个数 */
#define GPIOLED_NAME		"gpioled"	/* 名字 */
#define LEDOFF 				0			/* 关灯 */
#define LEDON 				1			/* 开灯 */
#define LEDtwinkle 			3			/* 灯闪烁 */

/* gpioled设备结构体 */
struct gpioled_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备 	 */
	int major;				/* 主设备号	  */
	int minor;				/* 次设备号   */
	struct device_node	*nd; /* 设备节点 */
	int led_gpio;			/* led所使用的GPIO编号	*/
};

/*1. fops的4个操作函数***************************************************************************************************/

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
	if(retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];		/* 获取状态值 */

	if(ledstat == LEDON) {	
		gpio_set_value(dev->led_gpio, 0);	/* 打开LED灯 */
	} else if(ledstat == LEDOFF) {
		gpio_set_value(dev->led_gpio, 1);	/* 关闭LED灯 */
	}else if(ledstat == LEDtwinkle){
		ssleep(1);							// 灯闪烁
		gpio_set_value(dev->led_gpio, 0);
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
	return 0;
}



/* 2. 设备树读取与设备初始化*************************************************************************************************/
/*
 * @description		: 对设备对应的设备树文件进行获取，并初始化为设备的具体属性
 * @param - gpioled : 设备结构体
 * @return 			: 0 成功;其他 失败
 */
static int __init dts_led_init(struct gpioled_dev dev)
{
	int ret = 0;
	const char *str;
	
	/* 1、获取设备节点：gpioled */
	dev.nd = of_find_node_by_path("/gpioled");
	if(dev.nd == NULL) {
		printk("gpioled node not find!\r\n");
		return -EINVAL;
	}

	/* 2.读取status属性 */
	ret = of_property_read_string(dev.nd, "status", &str);
	if(ret < 0) 
	    return -EINVAL;
	if (strcmp(str, "okay"))
        return -EINVAL;
    
	/* 3、获取compatible属性值并进行匹配 */
	ret = of_property_read_string(dev.nd, "compatible", &str);
	if(ret < 0) {
		printk("gpioled: Failed to get compatible property\n");
		return -EINVAL;
	}
    if (strcmp(str, "alientek,led")) {
        printk("gpioled: Compatible match failed\n");
        return -EINVAL;
    }

	/* 4、 获取设备树中的gpio属性，得到LED所使用的LED编号 */
	dev.led_gpio = of_get_named_gpio(dev.nd, "led-gpio", 0);
	if(dev.led_gpio < 0) {
		printk("can't get led-gpio");
		return -EINVAL;
	}
	printk("led-gpio num = %d\r\n", dev.led_gpio);

	/* 5.向gpio子系统申请使用GPIO */
	ret = gpio_request(dev.led_gpio, "LED-GPIO");
    if (ret) {
        printk(KERN_ERR "gpioled: Failed to request led-gpio\n");
        return ret;
	}

	/* 6、设置PI0为输出，并且输出高电平，默认关闭LED灯 */
	ret = gpio_direction_output(dev.led_gpio, 1);
	if(ret < 0) {
		printk("can't set gpio!\r\n");
	}
	return 0;
}




/* 3. 字符设备注册 *****************************************************************************************************/
/*
 * @description			: 字符设备注册
 * @param - dev 		: 设备结构体
 * @param - dev_fops 	: 操作函数fops
 */
static void __init regi_chr_dev(struct gpioled_dev dev, struct file_operations dev_fops)
{
	int ret = 0;
	
	/* 1、创建设备号 */
	if (dev.major) {		/*  定义了设备号 */
		dev.devid = MKDEV(dev.major, 0);
		ret = register_chrdev_region(dev.devid, GPIOLED_CNT, GPIOLED_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n", GPIOLED_NAME, GPIOLED_CNT);
			goto free_gpio;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&dev.devid, 0, GPIOLED_CNT, GPIOLED_NAME);	/* 申请设备号*/
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", GPIOLED_NAME, ret);
			goto free_gpio;
		}
		dev.major = MAJOR(dev.devid);	/* 获取分配号的主设备号 */
		dev.minor = MINOR(dev.devid);	/* 获取分配号的次设备号 */
	}
	printk("gpioled major=%d,minor=%d\r\n",dev.major, dev.minor);	
	
	/* 2、初始化cdev */
	dev.cdev.owner = THIS_MODULE;
	cdev_init(&dev.cdev, &dev_fops);
	
	/* 3、添加一个cdev */
	cdev_add(&dev.cdev, dev.devid, GPIOLED_CNT);
	if(ret < 0)
		goto del_unregister;
		
	/* 4、创建类 */
	dev.class = class_create(THIS_MODULE, GPIOLED_NAME);
	if (IS_ERR(dev.class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	dev.device = device_create(dev.class, NULL, dev.devid, NULL, GPIOLED_NAME);
	if (IS_ERR(dev.device)) {
		goto destroy_class;
	}

	// 6. 字符设备注册过程报错的处理函数 
destroy_class:
	class_destroy(dev.class);
del_cdev:
	cdev_del(&dev.cdev);
del_unregister:
	unregister_chrdev_region(dev.devid, GPIOLED_CNT);
free_gpio:
	gpio_free(dev.led_gpio);
}
