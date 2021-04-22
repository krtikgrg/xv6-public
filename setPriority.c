//changes by kartik 
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int main(int argc,char *argv[])
{
    if(argc<3)
    {
        printf(1,"Less number of arguments provided that is %d\n",argc);
        exit();
    }
    int new_priority=0,pid=0,k=1,old_priority;
    for(int i=strlen(argv[1])-1;i>=0;i--)
    {
        new_priority = new_priority + (k*((int)(argv[1][i]-'0')));
        k=k*10;
    }
    k=1;
    for(int i=strlen(argv[2])-1;i>=0;i--)
    {
        pid = pid + (k*((int)(argv[2][i]-'0')));
        k=k*10;
    }
    old_priority = set_priority(new_priority,pid);
    printf(1,"%d\n",old_priority);
    exit();
}
//changes done
