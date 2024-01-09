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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/***************************************************************
文件名		: dtsled.c
教程		: 24.3.1
版本	   	: V1.0
描述	   	: LED驱动文件。(使用设备树描述led的寄存器的物理地址)
其他	   	: 使用设备树来向内核传递相关的寄存器物理地址， 
			  Linux驱动文件使用内核提供的OF函数从设备树获取属性值，然后使用属性值来初始化相关的IO
日志	   	: 初版V1.0 2024

注意		: 需要修改设备树文件stm32mp157d-atk.dts，给设备树添加led硬件的寄存器地址，然后编译设备树文件
			  设备树文件最好添加到好区分的地方
***************************************************************/
#define DTSLED_CNT			1		  	/* 设备号个数 */
#define DTSLED_NAME			"dtsled"	/* 名字 */
#define LEDOFF 					0			/* 关灯 */
#define LEDON 					1			/* 开灯 */

/* 映射后的寄存器虚拟地址指针 */
static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI;

/* dtsled设备结构体 */
struct dtsled_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;		/* 类 		*/
	struct device *device;	/* 设备 	 */
	int major;				/* 主设备号	  */
	int minor;				/* 次设备号   */
	struct device_node	*nd; /* 设备节点 */
};

struct dtsled_dev dtsled;	/* led设备 */

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
		val |= (1 << 16);	
		writel(val, GPIOI_BSRR_PI);
	}else if(sta == LEDOFF) {
		val = readl(GPIOI_BSRR_PI);
		val|= (1 << 0);	
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
	filp->private_data = &dtsled; /* 设置私有数据 */
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

	retvalue = copy_from_user(databuf, buf, cnt);
	if(retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];		/* 获取状态值 */

	if(ledstat == LEDON) {	
		led_switch(LEDON);		/* 打开LED灯 */
	} else if(ledstat == LEDOFF) {
		led_switch(LEDOFF);	/* 关闭LED灯 */
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

/* 设备操作函数 */
static struct file_operations dtsled_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.read = led_read,
	.write = led_write,
	.release = 	led_release,
};

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init led_init(void)
{
	u32 val = 0;
	int ret;
	u32 regdata[12];
	const char *str;
	struct property *proper;

	/* 获取设备树中的属性数据，使用内核的OF函数来读取设备树文件的属性 */
	/* 1、获取设备节点：stm32mp1_led */
	dtsled.nd = of_find_node_by_path("/stm32mp1_led");
	if(dtsled.nd == NULL) {
		printk("stm32mp1_led node nost find!\r\n");
		return -EINVAL;
	} else {
		printk("stm32mp1_lcd node find!\r\n");
	}

	/* 2、获取compatible属性内容 */
	proper = of_find_property(dtsled.nd, "compatible", NULL);
	if(proper == NULL) {
		printk("compatible property find failed\r\n");
	} else {
		printk("compatible = %s\r\n", (char*)proper->value);
	}

	/* 3、获取status属性内容 */
	ret = of_property_read_string(dtsled.nd, "status", &str);
	if(ret < 0){
		printk("status read failed!\r\n");
	} else {
		printk("status = %s\r\n",str);
	}

	/* 4、获取reg属性内容 */
	ret = of_property_read_u32_array(dtsled.nd, "reg", regdata, 12);
	if(ret < 0) {
		printk("reg property read failed!\r\n");
	} else {
		u8 i = 0;
		printk("reg data:\r\n");
		for(i = 0; i < 12; i++)
			printk("%#X ", regdata[i]);
		printk("\r\n");
	}

	/* 初始化LED */
	/* 1、寄存器地址映射， 使用OF函数 */
	MPU_AHB4_PERIPH_RCC_PI = of_iomap(dtsled.nd, 0);
  	GPIOI_MODER_PI = of_iomap(dtsled.nd, 1);
	GPIOI_OTYPER_PI = of_iomap(dtsled.nd, 2);
	GPIOI_OSPEEDR_PI = of_iomap(dtsled.nd, 3);
	GPIOI_PUPDR_PI = of_iomap(dtsled.nd, 4);
	GPIOI_BSRR_PI = of_iomap(dtsled.nd, 5);

	/* 2、使能PI时钟 */
    val = readl(MPU_AHB4_PERIPH_RCC_PI);
    val &= ~(0X1 << 8); /* 清除以前的设置 */
    val |= (0X1 << 8);  /* 设置新值 */
    writel(val, MPU_AHB4_PERIPH_RCC_PI);

    /* 3、设置PI0通用的输出模式。*/
    val = readl(GPIOI_MODER_PI);
    val &= ~(0X3 << 0); /* bit0:1清零 */
    val |= (0X1 << 0);  /* bit0:1设置01 */
    writel(val, GPIOI_MODER_PI);

    /* 3、设置PI0为推挽模式。*/
    val = readl(GPIOI_OTYPER_PI);
    val &= ~(0X1 << 0); /* bit0清零，设置为上拉*/
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

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (dtsled.major) {		/*  定义了设备号 */
		dtsled.devid = MKDEV(dtsled.major, 0);
		ret = register_chrdev_region(dtsled.devid, DTSLED_CNT, DTSLED_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n",DTSLED_NAME, DTSLED_CNT);
			goto fail_map;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&dtsled.devid, 0, DTSLED_CNT, DTSLED_NAME);	/* 申请设备号 */
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", DTSLED_NAME, ret);
			goto fail_map;
		}
		dtsled.major = MAJOR(dtsled.devid);	/* 获取分配号的主设备号 */
		dtsled.minor = MINOR(dtsled.devid);	/* 获取分配号的次设备号 */

	}
	printk("dtsled major=%d,minor=%d\r\n",dtsled.major, dtsled.minor);	
	
	/* 2、初始化cdev */
	dtsled.cdev.owner = THIS_MODULE;
	cdev_init(&dtsled.cdev, &dtsled_fops);
	
	/* 3、添加一个cdev */
	ret = cdev_add(&dtsled.cdev, dtsled.devid, DTSLED_CNT);
	if(ret < 0)
		goto del_unregister;

	/* 4、创建类 */
	dtsled.class = class_create(THIS_MODULE, DTSLED_NAME);
	if (IS_ERR(dtsled.class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	dtsled.device = device_create(dtsled.class, NULL, dtsled.devid, NULL, DTSLED_NAME);
	if (IS_ERR(dtsled.device)) {
		goto destroy_class;
	}

	return 0;

// goto 统一处理报错
destroy_class:
	class_destroy(dtsled.class);
del_cdev:
	cdev_del(&dtsled.cdev);
del_unregister:
	unregister_chrdev_region(dtsled.devid, DTSLED_CNT);
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
	/* 取消映射 */
	led_unmap();

	/* 注销字符设备驱动 */
	cdev_del(&dtsled.cdev);/*  删除cdev */
	unregister_chrdev_region(dtsled.devid, DTSLED_CNT); /* 注销设备号 */

	device_destroy(dtsled.class, dtsled.devid);
	class_destroy(dtsled.class);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");
