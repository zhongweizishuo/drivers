#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
/***************************************************************
文件名		: beepApp.c
描述	   	: 驱测试APP
使用方法	： ./beepApp /dev/beep  0 关闭
			  ./beepApp /dev/beep  1 打开
			  ./beepApp /dev/beep  3 间隔响一声
日志	   	: 初版V1.0 2024 0114
***************************************************************/

#define BEEPOFF 0
#define BEEPON 1
#define BEEPtwinkle 3 /* beep 间隔响 */

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd, retvalue;		  // fd是文件描述符，retvalue用于存储函数返回值
	char *filename;			  // 文件名字符串:字符指针，通常用于指向字符串的起始地址
	unsigned char databuf[1]; // 是一个长度为1的字符数组，用于存储beep的开关状态

	// 检查命令行参数的数量，如果不是3个，输出错误信息并返回-1表示失败
	if (argc != 3)
	{
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1]; // 将命令行参数中的第一个参数（文件名）赋值给filename

	/* 读写的方式，打开beep驱动 */
	fd = open(filename, O_RDWR);
	if (fd < 0)
	{
		printf("file %s open failed!\r\n", argv[1]);
		return -1;
	}

	databuf[0] = atoi(argv[2]); /* 命令行参数中的第二个参数argv[2]表示要执行的操作：打开或关闭；自动转int */

	/* 向fd（表示的/dev/beep设备）写入数据，写入失败则retvalue < 0并提示 */
	// 最终数据由驱动程序中的 retvalue = copy_from_user(databuf, buf, cnt);中的buf接受APP发送过来的数据，并存入databuf,供内核使用
	retvalue = write(fd, databuf, sizeof(databuf));
	if (retvalue < 0)
	{
		printf("BEEP Control Failed!\r\n");
		close(fd);
		return -1;
	}

	retvalue = close(fd); /* 关闭文件 */
	if (retvalue < 0)
	{
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}
	return 0; // 程序正常执行结束，返回0表示成功。
}