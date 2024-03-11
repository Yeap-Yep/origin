#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>



/*
*argc:应用程序具体参数
*argv[]:具体的参数内容，所有都是字符串形式
* ./ledAPP <filename> <0:1> 0表示关灯，1表示开灯
*/

int main(int argc, char *argv[])
{
    int fd, ret;
    unsigned char data[8];
    unsigned short GYRO_XOUT, GYRO_YOUT, GYRO_ZOUT, TEMP_OUT;
    char *file_name;
    file_name = argv[1];


    if (argc != 2){
        printf("Error Usage!\r\n");
        return -1;
    }
    fd = open(file_name, O_RDWR);
    if(fd < 0){
        printf("file--%s open failed\r\n", file_name);
        return -1;
    }
    while (1)
    {
        ret = read(fd, &data, sizeof(data));
        if(ret){
            printf("read error!\n");
        }

        /*数据解析*/
        TEMP_OUT = (data[0] << 8) | data[1];
        GYRO_XOUT = (data[2] << 8) | data[3];
        GYRO_YOUT = (data[4] << 8) | data[5];
        GYRO_ZOUT = (data[6] << 8) | data[7];

        printf("TEMP_OUT = %d\n", TEMP_OUT);
        printf("GYRO_XOUT = %d\n", GYRO_XOUT);
        printf("GYRO_YOUT = %d\n", GYRO_YOUT);
        printf("GYRO_ZOUT = %d\n", GYRO_ZOUT);
    }
    
    

    close(fd);


    return 0;
}