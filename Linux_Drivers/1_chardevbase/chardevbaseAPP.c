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
*./ChardevbaseAPP <filename> <1:2> 1表示写，2表示读
*/
int main(int argc, char *argv[])
{
    int fd = 0;
    int ret = 0;
    char *file_name;
    char readbuf[100], writebuf[100];
    char usrdata[] = "usr data!"; 

    if(argc != 3){
        printf("Error usage\r\n");
        return -1;
    }
    file_name = argv[1];
    fd = open(file_name, O_RDWR);
    if (fd < 0){
        printf("open file %s failed",file_name);
        return -1;
    }

    if(atoi(argv[2]) == 1){
        ret = read(fd, readbuf, 50);
        if(ret < 0){
            printf("read %s failed\r\n",file_name);
        }
        else{
            printf("APP read data %s\r\n",readbuf);
        }   
    }
    else if(atoi(argv[2]) == 2){
        memcpy(writebuf, usrdata, sizeof(usrdata));
        ret =  write(fd, writebuf, 50);
        if(ret < 0){
            printf("read file %s failed",file_name);
        }
        else{
            printf("write successfully!\r\n");
        }
    }

    ret = close(fd);
    if (ret < 0){
        printf("close %s falied\r\n",file_name);
    }

    return 0;
}