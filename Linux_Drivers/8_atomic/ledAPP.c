#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*
*argc:应用程序具体参数
*argv[]:具体的参数内容，所有都是字符串形式
* ./ledAPP <filename> <0:1> 0表示关灯，1表示开灯
*/

int main(int argc, char *argv[])
{
    int ret,fd,cnt;
    char *file_name;
    file_name = argv[1];
    unsigned char databuf[1];
    databuf[0] = atoi(argv[2]);

    if (argc != 3){
        printf("Error Usage!\r\n");
        return -1;
    }
    fd = open(file_name, O_RDWR);
    if(fd < 0){
        printf("file--%s open failed\r\n", file_name);
        return -1;
    }

    ret = write(fd, databuf, 1);
    if (ret < 0){
        printf("LED control failed!\r\n");
    }

    while(1){
        sleep(3);
        printf("嘿嘿嘿\n");
        cnt++;
        if(cnt > 5) break;
    }
    printf("OVER!\n");

    close(fd);



    return 0;
}