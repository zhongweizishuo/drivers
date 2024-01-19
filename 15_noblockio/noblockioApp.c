/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名           : keyApp.c
作者             : 正点原子Linux团队
版本             : V1.0
描述             : 以非阻塞方式读取按键状态
其他             : 无
使用方法         : ./keyApp /dev/key
论坛             : www.openedv.com
日志             : 初版V1.0 2021/01/19 正点原子Linux团队创建
***************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

/*
 * @description     : main主程序
 * @param – argc        : argv数组元素个数
 * @param – argv        : 具体参数
 * @return          : 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
    fd_set readfds;
    int key_val;
    int fd;
    int ret;

    /* 判断传参个数是否正确 */
    if(2 != argc) {
        printf("Usage:\n"
               "\t./keyApp /dev/key\n"
              );
        return -1;
    }

    /* 打开设备 */
    fd = open(argv[1], O_RDONLY | O_NONBLOCK);
    if(0 > fd) {
        printf("ERROR: %s file open failed!\n", argv[1]);
        return -1;
    }

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    /* 循环轮训读取按键数据 */
    for ( ; ; ) {

        ret = select(fd + 1, &readfds, NULL, NULL, NULL);
        switch (ret) {

        case 0:     // 超时
            /* 用户自定义超时处理 */
            break;

        case -1:        // 错误
            /* 用户自定义错误处理 */
            break;

        default:
            if(FD_ISSET(fd, &readfds)) {
                read(fd, &key_val, sizeof(int));
                if (0 == key_val)
                    printf("Key Press\n");
                else if (1 == key_val)
                    printf("Key Release\n");
            }

            break;
        }
    }

    /* 关闭设备 */
    close(fd);
    return 0;
}