#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "vmm.h"

/* 页表 */
//PageTableItem pageTable[PAGE_SUM];
/*二级页表*/
PageTableItem pageTable[ROOT_PAGE_SUM][CHILD_PAGE_SUM];

/* 实存空间 */
BYTE actMem[ACTUAL_MEMORY_SIZE];
/* 用文件模拟辅存空间 */
FILE *ptr_auxMem;
/* 物理块使用标识 */
BOOL blockStatus[BLOCK_SUM];
/* 访存请求 */
Ptr_MemoryAccessRequest ptr_memAccReq;
//LRU的链表
 PageNode head = NULL;


/* 初始化*/
//修改了二级页表
void do_init()
{
	int i, j,k;
	unsigned long auxAddr=0;
	srandom(time(NULL));//设置用于生成随机序列的种子
	for (i = 0; i < ROOT_PAGE_SUM; i++)
	{
		for(k = 0; k<CHILD_PAGE_SUM; k++){
			pageTable[i][k].rootpageNum = i;
			pageTable[i][k].childpageNum = k;
			pageTable[i][k].filled = FALSE;//页面装入的特征位
			pageTable[i][k].edited = FALSE;//页面修改标示
			pageTable[i][k].count = 0;//页面的使用次数
			/* 随机将页面保护类型设置为以下其中情况中的一种 */
			switch (random() % 7)
			{
				case 0:
				{
					pageTable[i][k].proType = READABLE;
					break;
				}
				case 1:
				{
					pageTable[i][k].proType = WRITABLE;
					break;
				}
				case 2:
				{
					pageTable[i][k].proType = EXECUTABLE;
					break;
				}
				case 3:
				{
					pageTable[i][k].proType = READABLE | WRITABLE;
					break;
				}
				case 4:
				{
					pageTable[i][k].proType = READABLE | EXECUTABLE;
					break;
				}
				case 5:
				{
					pageTable[i][k].proType = WRITABLE | EXECUTABLE;
					break;
				}
				case 6:
				{
					pageTable[i][k].proType = READABLE | WRITABLE | EXECUTABLE;
					break;
				}
				default:
					break;
			}
		/* 设置该页对应的辅存地址 */
			//这已经是一个二级页表了，辅存和页表项之间的对应关系已经不再单纯了；
			//pageTable[i][k].auxAddr = (i*16+k)*PAFE_SIZE;
			pageTable[i][k].auxAddr = auxAddr;
			auxAddr += PAGE_SIZE;
		}
	}
	for (j = 0; j < BLOCK_SUM; j++)//用来作为物理块号的
	{
		/* 随机选择一些物理块装入实存地址*/
		if (random() % 2 == 0)
		{
			i = random() % ROOT_PAGE_SUM;
			k = random() % CHILD_PAGE_SUM;
			do_page_in(&pageTable[i][k], j);
			pageTable[i][k].blockNum = j;
			pageTable[i][k].filled = TRUE;
			blockStatus[j] = TRUE;
			LRU_ChangeAdd(j);
		}
		else
			blockStatus[j] = FALSE;
	}
}

//初始化文件
void initFile(){

	int i;
	char *key="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	char buffer[VIRTUAL_MEMORY_SIZE+1];
	
	ptr_auxMem=fopen(AUXILIARY_MEMORY,"w+");
	for(i=0;i<VIRTUAL_MEMORY_SIZE;i++){
		buffer[i]=key[rand()%62];
	}
	buffer[VIRTUAL_MEMORY_SIZE]=0;
	
	fwrite(buffer,sizeof(BYTE),VIRTUAL_MEMORY_SIZE,ptr_auxMem);
	printf("系统提示：初始化辅存模拟文件成功\n");
	fclose(ptr_auxMem);
}

/* 响应请求*/
//已更新二级页表
void do_response()
{
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int RootPageSum, ChildPageSum,offAddr,SinglePageSum;
	unsigned int actAddr;
	
	/* 检查地址是否越界*/
	if (ptr_memAccReq->virAddr < 0 || ptr_memAccReq->virAddr >= VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}
	
	/* 计算页号和页内偏移值 */
	SinglePageSum = PAGE_SIZE*CHILD_PAGE_SUM;
	RootPageSum = ptr_memAccReq->virAddr / SinglePageSum;
	ChildPageSum = (ptr_memAccReq->virAddr % SinglePageSum) / PAGE_SIZE;
	offAddr = ptr_memAccReq->virAddr % PAGE_SIZE;
	printf("页目录号为：%u\t,二级目录页号为：%u\t,页内偏移为：%u\n", RootPageSum,ChildPageSum, offAddr);

	/* 获取对应页表项 */
	ptr_pageTabIt = &pageTable[RootPageSum][ChildPageSum];
	
	/* 根据特征位决定是否产生缺页中断 */
	if (!ptr_pageTabIt->filled)
	{
		do_page_fault(ptr_pageTabIt);
	}else{

		LRU_ChangeAdd(ptr_pageTabIt->blockNum);
	}
	
	actAddr = ptr_pageTabIt->blockNum * PAGE_SIZE + offAddr;
	printf("实地址为%u\n", actAddr);
	
	/* 检查页面访问权限并处理访存请求 */
	switch (ptr_memAccReq->reqType)
	{
		case REQUEST_READ: //读请求
		{
			//ptr_pageTabIt->count++;
			//else 的地方已经加过了；这个count只是为了LFU代码，现在不需要了；
			if (!(ptr_pageTabIt->proType & READABLE)) //页面不可读
			{
				do_error(ERROR_READ_DENY);
				return;
			}
			/* 读取实存中的内容 */
			printf("读操作成功%02X\n", actMem[actAddr]);
			break;
		}
		case REQUEST_WRITE: //写请求
		{
			//ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & WRITABLE)) //页面不可写
			{
				do_error(ERROR_WRITE_DENY);	
				return;
			}
			/* 向实存中写入请求的内容 */
			actMem[actAddr] = ptr_memAccReq->value;
			ptr_pageTabIt->edited = TRUE;			
			printf("写操作成功\n");
			break;
		}
		case REQUEST_EXECUTE: //执行请求
		{
			//ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & EXECUTABLE)) //页面不可执行
			{
				do_error(ERROR_EXECUTE_DENY);
				return;
			}			
			printf("执行成功\n");
			break;
		}
		default: //非法请求类型
		{	
			do_error(ERROR_INVALID_REQUEST);
			return;
		}
	}
}

/* 处理缺页中断 */
void do_page_fault(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i;
	printf("产生缺页中断，开始进行调页...\n");
	for (i = 0; i < BLOCK_SUM; i++)
	{
		if (!blockStatus[i])
		{
			/* 读辅存内容，写入到实存 */
			do_page_in(ptr_pageTabIt, i);
			
			/* 更新页表内容 */
			ptr_pageTabIt->blockNum = i;
			ptr_pageTabIt->filled = TRUE;
			ptr_pageTabIt->edited = FALSE;
			ptr_pageTabIt->count = 0;
			
			blockStatus[i] = TRUE;
		LRU_ChangeAdd(i);
			return;
		}
	}
	/* 没有空闲物理块，进行页面替换 */
	//do_LFU(ptr_pageTabIt);
	do_LRU(ptr_pageTabIt);
}

/* 根据LFU算法进行页面替换 */
//修改了，
void do_LFU(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i,j, min = 0xFFFFFFFF, pagei = 0,pagej = 0;
	printf("没有空闲物理块，开始进行LFU页面替换...\n");
	for (i = 0, min = 0xFFFFFFFF; i < ROOT_PAGE_SUM; i++)
	{
		for(j = 0; j<CHILD_PAGE_SUM; j++)
		{
			if (pageTable[i][j].count < min&&pageTable[i][j].filled==TRUE)//在所有的被装入的页表中查找呦；
			{
				min = pageTable[i][j].count;
				pagei = i;
				pagej = j;
			}
		}
	}
	printf("选择第%u页目录进行替换，选择选择第%u页号进行替换\n", pagei,pagej);
	if (pageTable[pagei][pagej].edited)//被改写过了
	{
		/* 页面内容有修改，需要写回至辅存 */
		printf("该页的内容有修改，写回至辅存\n");
		do_page_out(&pageTable[pagei][pagej]);
	}
	pageTable[pagei][pagej].filled = FALSE;//没有被装入；
	pageTable[pagei][pagej].count = 0;


	/* 将辅存内容写入实存 */
	do_page_in(ptr_pageTabIt, pageTable[pagei][pagej].blockNum);//这个位置的被取代了；
	
	/* 更新页表内容 */
	ptr_pageTabIt->blockNum = pageTable[pagei][pagej].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	printf("页面替换成功\n");
}

unsigned int LRU_get(){
	PageNode p;
	int blockNum;
	p = head;
	blockNum = p->blockNum;
	head = head->link;
	free(p);
	return blockNum;
}

void LRU_ChangeAdd(int blockNum){
	//新进来一个，先判断里面是不是有一样的，如果有一样的，就把它删了，然后再把新进来的补上去；
	PageNode p = head,r = head;
	if(head==NULL){
		LRU_add(blockNum);
	}
	else{
		while(p->blockNum!=blockNum && p!=NULL){
			r = p;
			p = p->link;
		}
		if(p==NULL){//找到最后了也没有找到
			LRU_add(blockNum);
		}
		else{//找到了；
			if(p==head)
				head = head->link;
			else
				r->link = p->link;
			free(p);
			LRU_add(blockNum);
		}
	}
}

void LRU_add(int blockNum){
	PageNode p,q,r = head;
	p = (PageNode)malloc(sizeof(Node));
	p->blockNum = blockNum;
	p->link = NULL;
	if(head==NULL){
		head = p;
	}else{
		while(r->link!=NULL)
			r = r->link;
		r->link = p;
	}
}

//和LFU差不多，就是判断条件变了；怎么知道什么是最早最久使用的呢，建立一个链表，一旦使用了哪个
void do_LRU(Ptr_PageTableItem ptr_pageTabIt){

	unsigned int i,j, pagei = 0,pagej = 0,blockNum;
	int flag = 0;
	printf("没有空闲物理块，开始进行LFU页面替换...\n");
	while(flag==0){
		//如果一直没有找到，说明当前的第一个已经被挪走了，再取一个；
		blockNum = LRU_get();
		for (i = 0; i < ROOT_PAGE_SUM; i++)
		{
			for(j = 0; j<CHILD_PAGE_SUM; j++)
			{
				if (pageTable[i][j].blockNum==blockNum && pageTable[i][j].filled==TRUE)
				//在所有的被装入的页表中查找呦；谁最久没有被使用过了；找到在物理内存中的并且最早最久没被使用过的；
				{
					flag = 1;
					pagei = i;
					pagej = j;
					//如何跳出两个循环?设置一个标示位；
					break;
				}
				if(flag==1)
					break;
			}
		}
	}
	printf("选择第%u页目录进行替换，选择选择第%u页号进行替换\n", pagei,pagej);
	if (pageTable[pagei][pagej].edited)//被改写过了
	{
		/* 页面内容有修改，需要写回至辅存 */
		printf("该页的内容有修改，写回至辅存\n");
		do_page_out(&pageTable[pagei][pagej]);
	}
	pageTable[pagei][pagej].filled = FALSE;//没有被装入；
	pageTable[pagei][pagej].count = 0;


	/* 将辅存内容写入实存 */
	do_page_in(ptr_pageTabIt, pageTable[pagei][pagej].blockNum);//这个位置的被取代了；
	
	/* 更新页表内容 */
	ptr_pageTabIt->blockNum = pageTable[pagei][pagej].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	LRU_ChangeAdd(blockNum);
	printf("页面替换成功\n");
}


/* 将辅存内容写入实存 */
void do_page_in(Ptr_PageTableItem ptr_pageTabIt, unsigned int blockNum)
{
	unsigned int readNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((readNum = fread(actMem + blockNum * PAGE_SIZE, 
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: blockNum=%u\treadNum=%u\n", blockNum, readNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_READ_FAILED);
		exit(1);
	}
	printf("调页成功：辅存地址%u-->>物理块%u\n", ptr_pageTabIt->auxAddr, blockNum);
}

/* 将被替换页面的内容写回辅存 */
void do_page_out(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int writeNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((writeNum = fwrite(actMem + ptr_pageTabIt->blockNum * PAGE_SIZE, 
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: writeNum=%u\n", writeNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_WRITE_FAILED);
		exit(1);
	}
	printf("写回成功：物理块%u-->>辅存地址%03X\n", ptr_pageTabIt->auxAddr, ptr_pageTabIt->blockNum);
}

/* 错误处理 */
void do_error(ERROR_CODE code)
{
	switch (code)
	{
		case ERROR_READ_DENY:
		{
			printf("访存失败：该地址内容不可读\n");
			break;
		}
		case ERROR_WRITE_DENY:
		{
			printf("访存失败：该地址内容不可写\n");
			break;
		}
		case ERROR_EXECUTE_DENY:
		{
			printf("访存失败：该地址内容不可执行\n");
			break;
		}		
		case ERROR_INVALID_REQUEST:
		{
			printf("访存失败：非法访存请求\n");
			break;
		}
		case ERROR_OVER_BOUNDARY:
		{
			printf("访存失败：地址越界\n");
			break;
		}
		case ERROR_FILE_OPEN_FAILED:
		{
			printf("系统错误：打开文件失败\n");
			break;
		}
		case ERROR_FILE_CLOSE_FAILED:
		{
			printf("系统错误：关闭文件失败\n");
			break;
		}
		case ERROR_FILE_SEEK_FAILED:
		{
			printf("系统错误：文件指针定位失败\n");
			break;
		}
		case ERROR_FILE_READ_FAILED:
		{
			printf("系统错误：读取文件失败\n");
			break;
		}
		case ERROR_FILE_WRITE_FAILED:
		{
			printf("系统错误：写入文件失败\n");
			break;
		}
		default:
		{
			printf("未知错误：没有这个错误代码\n");
		}
	}
}

void do_handrequest(){
	/* 产生请求地址 */
	int a;
	unsigned long virAddr;

	printf("请输入请求地址和请求类型：请求类型中：0-read，1-write，2-execute");
	scanf("%d %d",&virAddr,&a);
	ptr_memAccReq->virAddr = virAddr;
	/* 随机产生请求类型 */
	switch (a)
	{
		case 0: //读请求
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("产生请求：\n地址：%u\t类型：读取\n", ptr_memAccReq->virAddr);
			break;
		}
		case 1: //写请求
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* 产生待写入的值 */
			printf("请输入待写入的值...\n");
			scanf("%02x",&ptr_memAccReq->value);
			printf("产生请求：\n地址：%u\t类型：写入\t值%02X\n", ptr_memAccReq->virAddr, ptr_memAccReq->value);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("产生请求：\n地址：%u\t类型：执行\n", ptr_memAccReq->virAddr);
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
	/* 随机产生请求类型 */
	switch (random() % 3)
	{
		case 0: //读请求
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("产生请求：\n地址：%u\t类型：读取\n", ptr_memAccReq->virAddr);
			break;
		}
		case 1: //写请求
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* 随机产生待写入的值 */
			ptr_memAccReq->value = random() % 0xFFu;
			printf("产生请求：\n地址：%u\t类型：写入\t值：%02X\n",ptr_memAccReq->virAddr, ptr_memAccReq->value);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("产生请求：\n地址：%u\t类型：执行\n",ptr_memAccReq->virAddr);
			break;
		}
		default:
			break;
	}	
}

/* 打印页表 */
//修改了二级页表
void do_print_info()

{

	unsigned int i, j, k;

	char str[4];

	printf("页目录\t页号\t块号\t装入\t修改\t保护\t计数\t辅存\n");

	for (i = 0; i < ROOT_PAGE_SUM; i++)

	{

		for(j = 0; j<CHILD_PAGE_SUM; j++)

		{

			printf("%u\t%u\t%u\t%u\t%u\t%s\t%u\t%u\n", i,j,pageTable[i][j].blockNum, pageTable[i][j].filled, 

				pageTable[i][j].edited, get_proType_str(str, pageTable[i][j].proType), 

				pageTable[i][j].count, pageTable[i][j].auxAddr);

		}

	}

}

/*打印辅存内容*/
void do_print_virtual(){
	int i,j,k,readNum,p;
	printf("打印辅存内容:\n");
	
	BYTE temp[VIRTUAL_MEMORY_SIZE];
	if (fseek(ptr_auxMem, 0, SEEK_SET) < 0)
	{
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((readNum = fread(temp, sizeof(BYTE), VIRTUAL_MEMORY_SIZE, ptr_auxMem)) < VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_FILE_READ_FAILED);
		exit(1);
	}
	printf("1级页号\t2级页号\t辅存\t内容\t\n");
	for(i=0,k=0;i<ROOT_PAGE_SUM;i++)
	{
		for(p=0;p<CHILD_PAGE_SUM;p++)
		{
			printf("%d\t%d\t%d\t",i,p,k);
			for(j=0;j<PAGE_SIZE;j++){
				printf("%02x ",temp[k++]);
			}
			printf("\n");
		}
	}

}

/*打印实存内容*/
void do_print_actual(){
	int i,j,k;
	printf("打印实存内容:\n");
	
	printf("块号\t内容\t\n");
	for(i=0,k=0;i<BLOCK_SUM;i++){
		printf("%d\t",i);
		if(blockStatus[i]==TRUE){
			for(j=0;j<PAGE_SIZE;j++)		
				printf("%02x ",actMem[k++]);
		}
		else
		        k+=4;
		printf("\n");
	}

}

/* 获取页面保护类型字符串 */
char *get_proType_str(char *str, BYTE type)
{
	if (type & READABLE)
		str[0] = 'r';
	else
		str[0] = '-';
	if (type & WRITABLE)
		str[1] = 'w';
	else
		str[1] = '-';
	if (type & EXECUTABLE)
		str[2] = 'x';
	else
		str[2] = '-';
	str[3] = '\0';
	return str;
}

int main(int argc, char* argv[])
{
	char c;
	int i;
	initFile();
	if (!(ptr_auxMem = fopen(AUXILIARY_MEMORY, "r+")))
	{
		do_error(ERROR_FILE_OPEN_FAILED);
		exit(1);
	}
	
	do_init();
	do_print_info();
	ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));
	/* 在循环中模拟访存请求与处理过程 */
	while (TRUE)
	{
		do_request();
		do_response();
		printf("按Y打印页表，按其他键不打印...\n");
		if ((c = getchar()) == 'y' || c == 'Y')
			do_print_info();
		while (c != '\n')
			c = getchar();
		printf("按X退出程序，按其他键继续...\n");
		if ((c = getchar()) == 'x' || c == 'X')
			break;
		while (c != '\n')
			c = getchar();
		//sleep(5000);
	}

	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
	return (0);
}
