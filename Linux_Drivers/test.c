#include <stdio.h>

#define MIN(a,b)        ((a) < (b) ? a : b)

int main(int argc, char* argv[])
{
    int (*ptr)[5] = 0;
    int a[5] = {1,2,3,4,5};

    ptr = a;
    for(int i = 0; i < 5; i++){
        printf("%d\n",(*ptr)[i]);
    }

    printf("%d\n", MIN(7,9));
}
