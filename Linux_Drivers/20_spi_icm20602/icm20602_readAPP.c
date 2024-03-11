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
    unsigned char data[12];
    unsigned short ACCEL_XOUT,ACCEL_YOUT,ACCEL_ZOUT,GYRO_XOUT, GYRO_YOUT, GYRO_ZOUT;
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
        ACCEL_XOUT = (data[0] << 8) | data[1];
        ACCEL_YOUT = (data[2] << 8) | data[3];
        ACCEL_ZOUT = (data[4] << 8) | data[5];
        GYRO_XOUT = (data[6] << 8) | data[7];
        GYRO_YOUT = (data[8] << 8) | data[9];
        GYRO_ZOUT = (data[10] << 8) | data[11];

        printf("ACCEL_XOUT = %d\n", ACCEL_XOUT);
        printf("ACCEL_YOUT = %d\n", ACCEL_YOUT);
        printf("ACCEL_ZOUT = %d\n", ACCEL_ZOUT);
        printf("GYRO_XOUT = %d\n", GYRO_XOUT);
        printf("GYRO_YOUT = %d\n", GYRO_YOUT);
        printf("GYRO_ZOUT = %d\n", GYRO_ZOUT);
    }
    
    

    close(fd);


    return 0;
}