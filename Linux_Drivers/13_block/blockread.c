#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <linux/ioctl.h>
#include <sys/ioctl.h>




#define CLOSE_CMD        _IO(0XEF, 1)       //关闭命令
#define OPEN_CMD         _IO(0XEF, 2)       //打开命令
#define WRITE_CMD        _IOW(0XEF, 3, int) //设置周期


/*
*argc:应用程序具体参数
*argv[]:具体的参数内容，所有都是字符串形式
* ./timerAPP <filename> <0:1> 0表示关灯，1表示开灯
*/

int main(int argc, char *argv[])
{
    int ret,fd;
    unsigned int cmd;
    unsigned int arg;
    int gpiodata[1];
    char *file_name;
    file_name = argv[1];
    // unsigned char databuf[1];
    // databuf[0] = atoi(argv[2]);

    if (argc != 2){
        printf("Error Usage!\r\n");
        return -1;
    }
    fd = open(file_name, O_RDWR);
    if(fd < 0){
        printf("file--%s open failed\r\n", file_name);
        return -1;
    }

    while(1){
        read(fd, gpiodata, 1);
        printf("the key release!\n");

    }

    // ret = write(fd, databuf, 1);
    // if (ret < 0){
    //     printf("LED control failed!\r\n");
    // }

    // while(1){
    //     // printf("init cmd = %d\n",cmd);
    //     printf("Input cmd:");
    //     ret = scanf("%d", &cmd);
    //     if(ret != 0){
    //         gets(str);
    //     }

    //     switch(cmd){
    //     case 1:
    //         ioctl(fd, CLOSE_CMD, &arg);     /*这个地方的arg传的应该就是确切值，而不是地址，这里传地址是运气好所以代码逻辑也没有问题*/
    //         break;
    //     case 2:
    //         ioctl(fd, OPEN_CMD, &arg);
    //         break;
    //     case 3:
    //         printf("Input Timer Period:");
    //         ret = scanf("%d", &arg);
    //         if(ret != 0){
    //             gets(str);
    //         }
    //         ioctl(fd, WRITE_CMD, &arg);
    //         break;
    //     }
    // }

    close(fd);



    return 0;
}