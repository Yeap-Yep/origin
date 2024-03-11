#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>



// #define CLOSE_CMD        _IO(0XEF, 1)       //关闭命令
// #define OPEN_CMD         _IO(0XEF, 2)       //打开命令
// #define WRITE_CMD        _IOW(0XEF, 3, int) //设置周期


/*
*argc:应用程序具体参数
*argv[]:具体的参数内容，所有都是字符串形式
* ./timerAPP <filename> <0:1> 0表示关灯，1表示开灯
*/

static void sigio_signal_func(int num)
{
    printf("the key released!\n");
}


int main(int argc, char *argv[])
{
    // fd_set readfds;                                             //读操作文件描述符
    // struct timeval timeout;                                     //时间结构体

    // struct pollfd fds;                                               //poll函数需要的结构体

    int ret,fd,flag=0;
    // unsigned int cmd;
    // unsigned int arg;
    // int gpiodata[1];
    char *file_name;
    file_name = argv[1];
    // unsigned char databuf[1];
    // databuf[0] = atoi(argv[2]);

    if (argc != 2){
        printf("Error Usage!\r\n");
        return -1;
    }
    fd = open(file_name, O_RDWR|O_NONBLOCK);
    if(fd < 0){
        printf("file--%s open failed\r\n", file_name);
        return -1;
    }

#if 0
    while(1){
        FD_ZERO(&readfds);                                      //清除readfds                                                  
        FD_SET(fd, &readfds);                                     //将文件描述符添加进readfds

        timeout.tv_sec = 0;
        timeout.tv_usec = 500;                                  //超时时间设置为500ms


        ret = select(fd+1, &readfds, NULL, NULL, &timeout);
        switch (ret)
        {
        case 0:
            // printf("timeout");
            break;
        case -1:
            // printf("read error");
            break;
        default:
            if(FD_ISSET(fd, &readfds)){
                read(fd, gpiodata, 1);
                printf("the key release!\n");
            }
            break;
        }


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
#endif


    // while(1){
    //     /*初始化poll函数需要的结构体*/
    //     fds.fd = fd;                                            //要监视的文件描述符
    //     fds.events = POLLIN;                                     //要监听时间的类型，这个记不住到时候上网查就好

    //     /*
    //     *对应的的结构体
    //     *事件数量
    //     *延迟时间
    //     */
    //     ret = poll(&fds, 1, 500);
    //     poll()
    //     switch (ret)
    //     {
    //     case 0:
    //         // printf("timeout");
    //         break;
    //     case -1:
    //         // printf("read error");
    //         break;
    //     default:
    //         if(fds.events | POLLIN){
    //             ret = read(fd, gpiodata, 1);
    //             if(ret < 0)
    //                 printf("read error");
    //             else
    //                 printf("the key release!\n");
    //         }
    //         break;
    //     }


    // }

    signal(SIGIO, sigio_signal_func);   //设置信号处理函数
    fcntl(fd, F_SETOWN, getpid());     //将进程号告诉内核
    
    flag = fcntl(fd, F_GETFL);          //获取但前进程状态
    fcntl(fd, F_SETFL, flag | FASYNC);  //开启当前异步通知功能
    
    while(1){
        sleep(1);
    }

    close(fd);



    return 0;
}