/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名                 : asyncKeyApp.c
作者                   : 正点原子Linux团队
版本                   : V1.0
描述                   : 异步通知测试程序
其他                   : 无
使用方法               : ./asyncKeyApp /dev/key
论坛                   : www.openedv.com
日志                   : 初版V1.0 2021/01/19 正点原子Linux团队创建
***************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static int fd;

/*
 * SIGIO信号处理函数
 * @param – signum		: 信号值
 * @return				: 无
 */
static void sigio_signal_func(int signum)
{
    unsigned int key_val = 0;

    read(fd, &key_val, sizeof(unsigned int));
    if (0 == key_val)
        printf("Key Press\n");
    else if (1 == key_val)
        printf("Key Release\n");
}


/*
 * @description		: main主程序
 * @param – argc		: argv数组元素个数
 * @param – argv		: 具体参数
 * @return			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
    int flags = 0;

    /* 判断传参个数是否正确 */
    if(2 != argc) {
        printf("Usage:\n"
               "\t./asyncKeyApp /dev/key\n"
              );
        return -1;
    }

    /* 打开设备 */
    fd = open(argv[1], O_RDONLY | O_NONBLOCK);
    if(0 > fd) {
        printf("ERROR: %s file open failed!\n", argv[1]);
        return -1;
    }

    /* 设置信号SIGIO的处理函数 */
    signal(SIGIO, sigio_signal_func);
    fcntl(fd, F_SETOWN, getpid());			// 将当前进程的进程号告诉给内核
    flags = fcntl(fd, F_GETFD);				// 获取当前的进程状态
    fcntl(fd, F_SETFL, flags | FASYNC);		// 设置进程启用异步通知功能


    /* 循环轮询读取按键数据 */
    for ( ; ; ) {

        sleep(2);
    }

    /* 关闭设备 */
    close(fd);
    return 0;
}
