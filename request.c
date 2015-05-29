#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include<fcntl.h>
#include "vmm.h"

CMD cmd;
Prt_MemoryAccessRequest ptr_memAccReq = &(cmd.request);

int main()
{
	char c;
	int fd;
	srandom(time(NULL));
	/* 在循环中模拟访存请求与处理过程 */

	while (TRUE)

	{
		//读取文件
		bzero(&cmd,DATALEN);
		printf("按D手动输入命令，按其他键自动生成命令...\n");

		if ((c = getchar()) == 'D' || c == 'd')

			do_handrequest();
		else
			do_request();

		while (c != '\n')

			c = getchar();

		do_response();

		printf("按Y打印页表，按其他键不打印...\n");

		if ((c = getchar()) == 'y' || c == 'Y')

			do_print_info();

		while (c != '\n')

			c = getchar();


		printf("按A打印实存，按其他键不打印...\n");

		if ((c = getchar()) == 'a' || c == 'A')

			do_print_actual();

		while (c != '\n')

			c = getchar();

		printf("按B打印辅存，按其他键不打印...\n");

		if ((c = getchar()) == 'b' || c == 'B')

			do_print_virtual();

		while (c != '\n')

			c = getchar();

		printf("按X退出程序，按其他键继续...\n");

		if ((c = getchar()) == 'x' || c == 'X')

			break;

		while (c != '\n')

			c = getchar();

		//sleep(5000);

	}
}
void do_handrequest(){

	/* 产生请求地址 */

	int a;
	printf("请输入请求地址:\n");
	scanf("%u",&ptr_memAccReq->virAddr);

	/*产生进程编号*/
	printf("请输入进程编号：0 or 1\n");
	scanf("%u", &ptr_memAccReq->processNum);
	
	printf("请输入请求类型：请求类型中：0-read，1-write，2-execute\n");

	scanf("%d",&a);
	/* 输入产生请求类型 */

	switch (a%3)

	{

		case 0: //读请求

		{

			ptr_memAccReq->reqType = REQUEST_READ;

			printf("产生请求：\n地址：%u\t进程编号 ：%u\t类型：读取\n", ptr_memAccReq->virAddr,prt_memAccReq->processNum);

			break;

		}

		case 1: //写请求

		{

			ptr_memAccReq->reqType = REQUEST_WRITE;

			/* 产生待写入的值 */

			printf("请输入待写入的值...\n");

			scanf("%02x",&ptr_memAccReq->value);

			printf("产生请求：\n地址：%u\t进程编号 ：%u\t类型：写入\t值%02X\n", ptr_memAccReq->virAddr,prt_memAccReq->processNum, ptr_memAccReq->value);

			break;

		}

		case 2:

		{

			ptr_memAccReq->reqType = REQUEST_EXECUTE;

			printf("产生请求：\n地址：%u\t进程编号 ：%u\t类型：执行\n", ptr_memAccReq->virAddr,prt_memAccReq->processNum);

			break;

		}

		default:

			break;

	}	

}



/* 产生访存请求 */

void do_request()

{

	/* 随机产生请求地址 */

	ptr_memAccReq->virAddr = random() % VIRTUAL_MEMORY_SIZE;
	
	//create request processNum
	if (random() % 2 == 0) {
		ptr_memAccReq->processNum = 0;
	}
	else ptr_memAccReq->processNum = 1;

	/* 随机产生请求类型 */

	switch (random() % 3)

	{

		case 0: //读请求

		{

			ptr_memAccReq->reqType = REQUEST_READ;

			printf("产生请求：\n地址：%u\t进程号：%u\t类型：读取\n", ptr_memAccReq->virAddr,ptr_memAccReq->processNum);

			break;

		}

		case 1: //写请求

		{

			ptr_memAccReq->reqType = REQUEST_WRITE;

			/* 随机产生待写入的值 */

			ptr_memAccReq->value = random() % 0xFFu;

			printf("产生请求：\n地址：%u\t进程号：%u\t类型：写入\t值：%02X\n",ptr_memAccReq->virAddr,ptr_memAccReq->processNum, ptr_memAccReq->value);

			break;

		}

		case 2:

		{

			ptr_memAccReq->reqType = REQUEST_EXECUTE;

			printf("产生请求：\n地址：%u\t进程号：%u\t类型：执行\n",ptr_memAccReq->virAddr,ptr_memAccReq->processNum);

			break;

		}

		default:

			break;

	}	

}

