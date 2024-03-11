#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>



static struct input_event inputevent;

/*
*argc:应用程序具体参数
*argv[]:具体的参数内容，所有都是字符串形式
* ./ledAPP <filename> <0:1> 0表示关灯，1表示开灯
*/

int main(int argc, char *argv[])
{
    int err,fd;
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
    while(1){
        err = read(fd, &inputevent, sizeof(inputevent));
        if(err > 0){
            switch(inputevent.type){
                case EV_KEY:
                    if(inputevent.code < BTN_MISC)                                                          //宏定义小于BTN_MISC，linux内核都是虚拟成按键，大于则虚拟成按钮
                        printf("KEY %d %s\n", inputevent.code, inputevent.value?"press":"release");
                    break;
                case EV_SYN:
                    printf("EV_SYN event\n");
                    break;
            }
        }
    }





    return 0;
}