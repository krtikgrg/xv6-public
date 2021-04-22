//changes by kartik 
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int main(int argc,char *argv[])
{
    if(argc<=1)
    {
        printf(1,"Less number of arguments provided that is %d\n",argc);
        exit();
    }
    int ret=fork();    
    if(ret==0)
    {
        //printf(1,"command is %s\n",argv[1]);
        if(argc!=1)
            exec(argv[1],argv+1);
        exit();
    }
    else
    {
        int wtime,rtime;
        //*wtime=-1;
        //*rtime=-1;
        waitx(&wtime,&rtime);
        printf(1,"\n\nwaiting time is %d\nrunning time is %d\n\n",wtime,rtime);
    }
    exit();
}
//chnages finished