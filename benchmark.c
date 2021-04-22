//changes by kartik 
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
int main()
{
    int i,pidp[10],j,k,a=5;
    int avgwait=0;

    for(i=0;i<10;i++)
    {
        pidp[i]=fork();
        if(pidp[i]==0)
        {
            for(j=0;j<10;j++)
            {
                if(j<=i)
                    sleep(200);
                else
                {
                    for(k=0;k<100000000;k++)
                    {
                        a=a+1;
                        a=a*10;
                        a=a/10;
                        a=a-1;
                        a=a/5;
                        a=a*5;
                        a=a-5;
                        a=a+5;        
                    }
                }
            }
            exit();      
        }
        else
        {
            #ifdef PBS
            set_priority(90-(35-i),pidp[i]);
            #endif
        }
    }
    sleep(150);
    for(i=0;i<5;i++)
    {
        sleep(150);
        printf(1,"ps output coming up now\n");
        pscall();
    }
    for(i=0;i<10;i++)
    {
        int wtime,rtime;
        waitx(&wtime,&rtime);
        avgwait+=wtime;
        //printf(1,"rtime is %d\n",rtime);
    }
    avgwait=avgwait/20;
    printf(1,"average waiting time for the processes is %d\nValue may have precissional errors because float is not defined in xv6\n",avgwait);
    exit();
}
//changes finished