#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <netinet/in.h>

#define BUFF_SIZE 512
#define ARGV_COUNT 10
#define PATH_SIZE 128
#define FILE_SIZE 128
#define FILE_MD5_SIZE 33

int create_sockfd();
void down_file(int sockfd,char buff[]);//下载文件
void put_file(int sockfd,char buff[],char *path_file);//目录文件
void file(int sockfd,char buff[],char *path_file);//普通文件

int main()
{
	int sockfd = create_sockfd();
	char path[PATH_SIZE] = {0};

	while(1)
	{
		int count = 0;
		char buff[BUFF_SIZE] = {0};
		char recvbuff[BUFF_SIZE] = {0};
		char cur_path[PATH_SIZE] = "./";
		strcat(cur_path,path);
		printf("Connect>>");
		fflush(stdout);
		fgets(buff,BUFF_SIZE-1,stdin);//ls,rm a.c,mv a.c,get a.c
		buff[strlen(buff)-1] = 0;
	//	printf("send:\n",buff);

		if(buff[0] == 0)//输入回车，什么都不做
			continue;
		strtok(buff,"\n");

		if(strncmp(buff,"cd",2) == 0)//切换用户目录
		{
			strcpy(path,buff+3);
		}
		if(strncmp(buff,"end",3) == 0)
		{
			break;
		}
		else if(strncmp(buff,"get",3) == 0)
		{
			down_file(sockfd,buff);
		}
		else if(strncmp(buff,"put",3) == 0)
		{
			char back[BUFF_SIZE] = {0};
			char back_cli[BUFF_SIZE] = {0};
			strncpy(back_cli,buff,strlen(buff));

			char *myargv[ARGV_COUNT] = {0};
			myargv[0] = strtok(buff," ");
			int i = 1;
			for(;i<ARGV_COUNT;i++)
				myargv[i] = strtok(NULL," ");

			////传输文件
			char file_path[PATH_SIZE] = {0};
			strcpy(file_path,myargv[1]);
			char path_cur[PATH_SIZE] = {0};
			strcat(path_cur,cur_path);

			if(myargv[1] != NULL)
			{
				if(strcmp(cur_path,"./") != 0)
				strcat(path_cur,"/");
				strcat(path_cur,myargv[1]);
			}
			myargv[1] = path_cur;
			i = 0;
			for(;i<ARGV_COUNT;i++)
			{
				if(myargv[i] == NULL)
					break;
				strcat(back,myargv[i]);
				strcat(back," ");
			}
			put_file(sockfd,back,file_path);
		}
		else//调用系统命令
		{
			send(sockfd,buff,strlen(buff),0);

			if(recv(sockfd,recvbuff,BUFF_SIZE-1,0) <= 0)
			{
				printf("ser error\n");
				break;
			}
			if(strncmp(recvbuff,"ok#",3) != 0)
			{
				continue;
			}
			printf("%s",recvbuff+3);
		}
	}
	close(sockfd);
}

int create_sockfd()
{
	int sockfd = socket(AF_INET,SOCK_STREAM,0);
	assert(sockfd != -1);

	struct sockaddr_in saddr;
	memset(&saddr,0,sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(6000);
	saddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	int res = connect(sockfd,(struct sockaddr*)&saddr,sizeof(saddr));
	assert(res != -1);

	listen(sockfd,5);

	return sockfd;
}

void down_file(int sockfd,char buff[])
{
	char status[32] = {0};
	int file_size = 0;
	FILE* fp = NULL;
	char op_buff[128] = {0};
	char *s = NULL;
	int curr_size = 0;
	int num = 0;//当前接受
	char recvbuff[BUFF_SIZE] = {0};
	double d = 0.0;//计算百分比

	strcpy(op_buff,buff);
	s = strtok(op_buff," ");
	if(s == NULL)
		return;
	s = strtok(NULL," ");
	if(s == NULL)
	{
		printf("下载的文件名不能为空!\n");
		return;
	}
	send(sockfd,buff,strlen(buff),0);

	//等待服务器回复ok#大小
	if( recv(sockfd,status,31,0) <= 0)
	{
		printf("ser error!\n");
		return;
	}
	if(strncmp(status,"ok#",3) != 0)
	{
		printf("down error!");
		return;
	}
	sscanf(status+3,"%d",&file_size);

	fp = fopen(s,"wb");
	if(fp == NULL)
	{
		send(sockfd,"ERR",3,0);
	}
	else
	{
		send(sockfd,"ok",2,0);
	}

	while(curr_size < file_size)
	{
		num = recv(sockfd,recvbuff,512,0);
		if(num <= 0)
		{
			printf("ser close\n");
			break;
		}
		fwrite(recvbuff,num,1,fp);
		curr_size += num;

		d = curr_size*100.0 / file_size;
		printf("down file:%.2f%%\r",d);//\r挪光标
		fflush(stdout);
	}
	printf("\nfile down finish!\n");
	
	fclose(fp);
}

void put_file(int sockfd,char buff[],char *path_file)
{
	char path[PATH_SIZE] = {0};
	struct stat st;
	sprintf(path,"%s",path_file);
	lstat(path,&st);
	if(S_ISDIR(st.st_mode))//上传文件为目录文件
	{
		DIR *dp;
		dp = opendir(path);

		strcat(buff,"directory");
		send(sockfd,buff,strlen(buff),0);
		char recv_ok[BUFF_SIZE] = {0};
		recv(sockfd,recv_ok,BUFF_SIZE-1,0);
		if(strcmp(recv_ok,"ok") != 0)
			return;

		struct dirent* dir;
		while( (dir = readdir(dp)) != NULL)
		{
			if(strcmp(dir->d_name,".") == 0
					|| strcmp(dir->d_name,"..") == 0)
				continue;
			char path_cur[PATH_SIZE]= {0};
			strcat(path_cur,path);
			strcat(path_cur,"/");
			strcat(path_cur,dir->d_name);
			lstat(path_cur,&st);
			if(S_ISDIR(st.st_mode))//目录文件，递归
			{
				char new_buff[BUFF_SIZE] = "put ";
				char new_path_file[PATH_SIZE] = {0};
				strcat(new_buff,path_cur);
				strcat(new_path_file,path_cur);
				put_file(sockfd,new_buff,new_path_file);
			}
			else
			{
				char buff[BUFF_SIZE] = {0};
				sprintf(buff,"put %s",path_cur);
				file(sockfd,buff,path_cur);
			}
		}
	}
	else//上传文件为普通文件
		file(sockfd,buff,path_file);
}
void file(int sockfd,char buff[],char *path_file)
{
	int fd = open(path_file,O_RDONLY);
	if(fd == -1)
	{
		printf("open file error!");
		return;
	}

	int size = lseek(fd,0,SEEK_END);//文件大小
	char filesize[FILE_SIZE] = {0};
	sprintf(filesize," %d",size);
	strcat(buff,filesize);

	char put_file_md5[FILE_MD5_SIZE] = {0};
	Compute_file_md5(path_file,put_file_md5);
	strcat(buff," ");
	strcat(buff,put_file_md5);
	send(sockfd,buff,strlen(buff),0);//上传文件请求

	char recvbuff[BUFF_SIZE] = {0};
	if(recv(sockfd,recvbuff,BUFF_SIZE-1,0) <= 0)
	{
		close(fd);
		return;
	}
	//秒传
	if(strcmp(recvbuff,"final") == 0)// 说明服务端有文件，不用上传文件了
	{
		printf("一个文件已经秒传!!\n");
		return;
	}

	//设置断点续传
	int offsize = 0;
	if(strncmp(recvbuff,"okok",4) == 0)
	{
		printf("duan dian \n");
		char *s = strtok(recvbuff,"#");
		s = strtok(NULL,"#");
		offsize = atoi(s);
		printf("offsize:%d\n",offsize);
		lseek(fd,offsize,SEEK_SET);//将光标移动到偏移处开始上传
	}

	//开始传输文件
	//
	memset(recvbuff,0,BUFF_SIZE);
	int ccur = size-offsize;//剩余的文件大小（还没有上传）
	int count = read(fd,recvbuff,BUFF_SIZE-1);
	while(count > 0)
	{
		send(sockfd,recvbuff,count,0);
		count = read(fd,recvbuff,BUFF_SIZE);
		ccur -= count;
		float per = (size-ccur)*100.0/size;
		printf("put %0.2f%\r",per);
		fflush(stdout);
	}
	printf("put 100.00%\n");

	//传输完成，关闭文件
	memset(recvbuff,0,BUFF_SIZE);
	recv(sockfd,recvbuff,BUFF_SIZE-1,0);
	if(strcmp(recvbuff,"final") == 0)
		printf("文件上传成功!!\n");
	close(fd);
}
