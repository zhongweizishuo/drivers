#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <sys/ioctl.h>
/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: timerApp.c
作者	  	: 正点原子Linux团队
版本	   	: V1.0
描述	   	: 定时器测试应用程序
其他	   	: 无
使用方法	：./timertest /dev/timer 打开测试App
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2021/01/5 正点原子Linux团队创建
***************************************************************/

/* 命令值 */
#define CLOSE_CMD (_IO(0XEF, 0x1))	   /* 关闭定时器 */
#define OPEN_CMD (_IO(0XEF, 0x2))	   /* 打开定时器 */
#define SETPERIOD_CMD (_IO(0XEF, 0x3)) /* 设置定时器周期命令 */

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd, ret;
	char *filename;
	unsigned int cmd;
	unsigned int arg;
	unsigned char str[100];

	if (argc != 2)
	{
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];

	fd = open(filename, O_RDWR);
	if (fd < 0)
	{
		printf("Can't open file %s\r\n", filename);
		return -1;
	}

	// 操作逻辑部分
	while (1)
	{
		printf("Input CMD:");
		ret = scanf("%d", &cmd);	//scanf("%d", &cmd); 尝试从标准输入读取一个整数%d，并将其存储在 cmd 变量中
		if (ret != 1)
		{ /* 参数输入错误:不是整数%d */
			fgets(str, sizeof(str), stdin);
			/* 防止卡死:使用 fgets 函数从标准输入中读取并丢弃剩余的字符，
			以防止输入缓冲区中的字符导致后续的 scanf 进入死循环 */
		}

		if (4 == cmd) /* 退出APP	 */
			goto out;
		if (cmd == 1) /* 关闭LED灯 */
			cmd = CLOSE_CMD;
		else if (cmd == 2) /* 打开LED灯 */
			cmd = OPEN_CMD;
		else if (cmd == 3)
		{
			cmd = SETPERIOD_CMD; /* 设置周期值 */
			printf("Input Timer Period:");
			ret = scanf("%d", &arg);
			if (ret != 1)
			{									/* 参数输入错误 */
				fgets(str, sizeof(str), stdin); /* 防止卡死 */
			}
		}
		// ioctl():向fd发送自定义的命令码cmd(一般是整数)，arg参数可选；
		// 将命令or数据发送给驱动程序的fops,由timer_unlocked_ioctl()接受
		ioctl(fd, cmd, arg); /* 控制定时器的打开和关闭 */
	}

out:
	close(fd);
}