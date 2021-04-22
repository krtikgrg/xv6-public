#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
int main()
{
	int a=0;
	for(int i=0;i<50000000;i++)
	{
		a=a++;
		a=a--;
	}
	printf(1,"%d\n",a);
	return 0;
}
