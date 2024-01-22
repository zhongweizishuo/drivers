/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: key.c
作者	  	: 正点原子Linux团队
版本	   	: V1.0
描述	   	: Linux按键input子系统实验
其他	   	: 无
***************************************************************/
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>

#define KEYINPUT_NAME		"keyinput"	/* 名字 		*/

/* key设备结构体 */
struct key_dev{
	struct input_dev *idev;  /* 按键对应的input_dev指针 */
	struct timer_list timer; /* 消抖定时器 */
	int gpio_key;			 /* 按键对应的GPIO编号 */
	int irq_key;			 /* 按键对应的中断号 */
};

static struct key_dev key;          /* 按键设备 */

/*
 * @description		: 按键中断服务函数
 * @param – irq		: 触发该中断事件对应的中断号
 * @param – arg		: arg参数可以在申请中断的时候进行配置
 * @return			: 中断执行结果
 */
static irqreturn_t key_interrupt(int irq, void *dev_id)
{
	if(key.irq_key != irq)
		return IRQ_NONE;
	
	/* 按键防抖处理，开启定时器延时15ms */
	disable_irq_nosync(irq); /* 禁止按键中断 */
	mod_timer(&key.timer, jiffies + msecs_to_jiffies(15));
	
    return IRQ_HANDLED;
}

/*
 * @description			: 按键初始化函数
 * @param – nd			: device_node设备指针
 * @return				: 成功返回0，失败返回负数
 */
static int key_gpio_init(struct device_node *nd)
{
	int ret;
    unsigned long irq_flags;
	
	/* 从设备树中获取GPIO */
	key.gpio_key = of_get_named_gpio(nd, "key-gpio", 0);
	if(!gpio_is_valid(key.gpio_key)) {
		printk("key:Failed to get key-gpio\n");
		return -EINVAL;
	}
	
	/* 申请使用GPIO */
	ret = gpio_request(key.gpio_key, "KEY0");
    if (ret) {
        printk(KERN_ERR "key: Failed to request key-gpio\n");
        return ret;
	}	
	
	/* 将GPIO设置为输入模式 */
    gpio_direction_input(key.gpio_key);
	
	/* 获取GPIO对应的中断号 */
	key.irq_key = irq_of_parse_and_map(nd, 0);
	if(!key.irq_key){
        return -EINVAL;
    }

    /* 获取设备树中指定的中断触发类型 */
	irq_flags = irq_get_trigger_type(key.irq_key);
	if (IRQF_TRIGGER_NONE == irq_flags)
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
		
	/* 申请中断 */
	ret = request_irq(key.irq_key, key_interrupt, irq_flags, "Key0_IRQ", NULL);
	if (ret) {
        gpio_free(key.gpio_key);
        return ret;
    }

	return 0;
}

/*
 * @description		: 定时器服务函数，用于按键消抖，定时时间到了以后
 * 					  再读取按键值，根据按键的状态上报相应的事件
 * @param – arg		: arg参数就是定时器的结构体
 * @return			: 无
 */
static void key_timer_function(struct timer_list *arg)
{
	int val;
	
	/* 读取按键值并上报按键事件 */
	val = gpio_get_value(key.gpio_key);
	input_report_key(key.idev, KEY_0, !val);
	input_sync(key.idev);
	enable_irq(key.irq_key);
}

/*
 * @description			: platform驱动的probe函数，当驱动与设备
 * 						 匹配成功以后此函数会被执行
 * @param – pdev			: platform设备指针
 * @return				: 0，成功;其他负值,失败
 */
static int atk_key_probe(struct platform_device *pdev)
{
	int ret;
	
	/* 初始化GPIO */
	ret = key_gpio_init(pdev->dev.of_node);
	if(ret < 0)
		return ret;
		
	/* 初始化定时器 */
	timer_setup(&key.timer, key_timer_function, 0);
	
	/* 申请input_dev */
	key.idev = input_allocate_device();
	key.idev->name = KEYINPUT_NAME;
	
#if 0
	/* 初始化input_dev，设置产生哪些事件 */
	__set_bit(EV_KEY, key.idev->evbit);	/* 设置产生按键事件 */
	__set_bit(EV_REP, key.idev->evbit);	/* 重复事件，比如按下去不放开，就会一直输出信息 */

	/* 初始化input_dev，设置产生哪些按键 */
	__set_bit(KEY_0, key.idev->keybit);	
#endif

#if 0
	key.idev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	key.idev->keybit[BIT_WORD(KEY_0)] |= BIT_MASK(KEY_0);
#endif

	key.idev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	input_set_capability(key.idev, EV_KEY, KEY_0);

	/* 注册输入设备 */
	ret = input_register_device(key.idev);
	if (ret) {
		printk("register input device failed!\r\n");
		goto free_gpio;
	}
	
	return 0;
free_gpio:
	free_irq(key.irq_key,NULL);
	gpio_free(key.gpio_key);
	del_timer_sync(&key.timer);
	return -EIO;
	
}

/*
 * @description			: platform驱动的remove函数，当platform驱动模块
 * 						 卸载时此函数会被执行
 * @param – dev			: platform设备指针
 * @return				: 0，成功;其他负值,失败
 */
static int atk_key_remove(struct platform_device *pdev)
{
	free_irq(key.irq_key,NULL);			/* 释放中断号 */
	gpio_free(key.gpio_key);			/* 释放GPIO */
	del_timer_sync(&key.timer);			/* 删除timer */
	input_unregister_device(key.idev);	/* 释放input_dev */
	
	return 0;
}

static const struct of_device_id key_of_match[] = {
	{.compatible = "alientek,key"},
	{/* Sentinel */}
};

static struct platform_driver atk_key_driver = {
	.driver = {
		.name = "stm32mp1-key",
		.of_match_table = key_of_match,
	},
	.probe	= atk_key_probe,
	.remove = atk_key_remove,
};

module_platform_driver(atk_key_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");