#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAXFD 3//线程个数
#define MD5_BUFF_SIZE 33
#define BUFF_SIZE 512
#define ARGV_COUNT 10
#define PATH_SIZE 128
#define COMMAND_SIZE 128
#define MD5_BUFF_SIZE 33
#define FILE_NAME_SIZE 50
#define LOCAL_DIR "/home/wy/Desktop/yunpan"

pthread_mutex_t mutex;//定义锁//防止两个线程同时对链表进行操作
sem_t sem;//定义信号量//控制多个线程同时获取c
int create_sockfd();//创建tcp流式套接字
void* fun(void *arg);//子线程,arg为已链接用户的fd
void down_file(int c,char *argv[]);//下载文件
void put_file(int c,char *argv[]);//上传普通文件或上传文件夹

/*
 * *判断要上传的文件是否为已经存在的文件，
 ** put_file_md5为要上传文件的md5值，
 **local为当前文件的目录
 **函数返回1表示可以秒传，返回0表示不可以秒传
 */
int flashfile(char *put_file_md5,char *local);

int main()
{
	int sockfd = create_sockfd();
	pthread_mutex_init(&mutex,NULL);//初始化锁
	sem_init(&sem,0,0);//初始化信号量

	struct node *head = NULL;//定义链表头指针
	list_init(&head);

	//创建线程池
	int i = 0;
	pthread_t id[MAXFD];
	for(;i<MAXFD;i++)
	{
		pthread_create(&id[i],NULL,fun,(void *)head);
	}

	while(1)
	{
		struct sockaddr_in caddr;
		int len = sizeof(caddr);
		int c = accept(sockfd,(struct sockaddr *)&caddr,&len);
		if(c < 0)
		{
			continue;
		}
		pthread_mutex_lock(&mutex);	//lock
		list_add(head,c);//add list
		pthread_mutex_unlock(&mutex);//unlock
		sem_post(&sem);//v
	}
}

void down_file(int c,char *argv[])
{
	printf("下载文件\n");

	int fd = open(argv[1],O_RDONLY);
	if(fd == -1)
	{
		send(c,"ERR",3,0);
		return;
	}
	struct stat st;
	if(lstat(argv[1],&st) == -1)//得到文件大小
	{
		send(c,"ERR",3,0);
		return;
	}
	char buff[32] = {"ok#"};
	sprintf(buff+3,"%d",st.st_size);

	send(c,buff,strlen(buff),0);
	memset(buff,0,32);
	if( recv(c,buff,31,0) <= 0)
	{
		printf("client close\n");
		return;
	}
	if(strncmp(buff,"ok",2) != 0)
	{
		printf("client stop downfile\n");
		return;
	}
	char sendbuff[512] = {0};
	int num = 0;
	while( (num=read(fd,sendbuff,512)) > 0)
	{
		send(c,sendbuff,num,0);
	}
	printf("send file over\n");

	close(fd);
	return;
}

void put_file(int c,char *argv[])//上传文件或文件夹
{
	if(strcmp(argv[2],"directory") == 0)//上传文件夹
	{
		char path[PATH_SIZE] = {0};
		sprintf(path,"./%s",argv[1]);
		mkdir(path,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);//创建普通文件
		send(c,"ok",2,0);
	}
	else//普通文件上传
	{
		if(flashfile(argv[3],LOCAL_DIR) == 1)
		{
			send(c,"final",5,0);//秒传
			return;
		}

		char oldname[FILE_NAME_SIZE] = {0};
		strcpy(oldname,argv[1]);
		char newname[FILE_NAME_SIZE] = {0};
		strcpy(newname,argv[1]);
		strcat(newname,".tmp");

		printf("oldname:%s\n",oldname);
		printf("newname:%s\n",newname);
	
		int fd = open(newname,O_CREAT|O_WRONLY,0600);
		assert(fd != -1);
	
		int offsize = lseek(fd,0,SEEK_END);
		char recv_ok[BUFF_SIZE] = {0};
		sprintf(recv_ok,"okok#%d",offsize);
	
		printf("recv_ok:%s\n",recv_ok);

		send(c,recv_ok,strlen(recv_ok),0);//服务器确认可以上传 


		int filesize = atoi(argv[2])-offsize;


		while(filesize > 0)
		{
			int count = 0;
			char buff[BUFF_SIZE] = {0};
			if(filesize > BUFF_SIZE)
			{
				count = recv(c,buff,BUFF_SIZE-1,0);
			}
			else
			{
				count = recv(c,buff,filesize,0);
			}
			if(count <= 0)
			{
				close(fd);
				close(c);
				printf("one client over //duan dian\n");
				return;
			}
			write(fd,buff,count);
			filesize -= count;//防止接收发送黏包
		}
		send(c,"final",5,0);
		rename(newname,oldname);
		close(fd);
	}
}

void* fun(void *arg)
{
	printf("thread run !\n");
	struct node *head = (struct node*)arg;
	while(1)
	{
		sem_wait(&sem);//p

		pthread_mutex_lock(&mutex);
	    int c =	list_get_c(head); //get c from list node
		pthread_mutex_unlock(&mutex);

		if(c < 0)
		{
			continue;
		}
		while(1) //connect with client
		{
			char buff[BUFF_SIZE] = {0};
			if( recv(c,buff,BUFF_SIZE-1,0) <= 0)///客户端是否断开
			{
				close(c);
				printf("one client over\n");
				break;
			}
		//	printf("recv:%s\n",buff);

			char *myargv[ARGV_COUNT] = {0};
			char *p = NULL;
			int i = 1;
			myargv[0] = strtok_r(buff," ",&p);//分割收到的信息
			for(i;i<ARGV_COUNT;i++)
				myargv[i] = strtok_r(NULL," ",&p);
			
			if(strcmp(myargv[0],"get") == 0)//下载文件
			{
				/////////////////down
				down_file(c,myargv);
			}
			else if(strcmp(myargv[0],"put") == 0)//上传文件
			{
				/////put
				put_file(c,myargv);
			}
			else
			{
				int pipefd[2];
				pipe(pipefd);//匿名管道进行父子进程间通信

				pid_t pid = fork();
				assert(pid != -1);

				if(pid == 0)
				{
					close(pipefd[0]);
					//dup2//复制文件描述符
					dup2(pipefd[1],1);//绑定标准输出和标准错误为写端。
					dup2(pipefd[1],2);
				 	int res = execvp(myargv[0],myargv);//替换要执行的命令
					if(res == -1)
						perror("execvp error");
					exit(0);
				}
				close(pipefd[1]);
				wait(NULL);
				char readbuff[512] = {"ok#"};
				read(pipefd[0],readbuff+3,500);
				send(c,readbuff,strlen(readbuff),0);
			}

			//ls,cp,rm,mv
			//printf("c=%d,buff=%s",c,buff);
			//send(c,"ok",2,0);
		}
	}
}

int flashfile(char *put_file_md5,char *local)
{
	DIR *dp;
	struct dirent *dir;
	struct stat st;
	dp = opendir(local);//打开当前目录流
	if(dp == NULL)//打开文件不能为空
	{
		printf("opendir error %s cant't open\n",local);
		return 0;
	}

	while((dir = readdir(dp)) != NULL)
	{
		char path[PATH_SIZE] = {0};
		sprintf(path,"%s/%s",local,dir->d_name);
		lstat(path,&st);//文件属性判断       
		if(strcmp(dir->d_name,".") == 0
				|| strcmp(dir->d_name,"..") == 0)
			continue;
		if(S_ISDIR(st.st_mode))
		{
			char new_local[PATH_SIZE] = {0};
			sprintf(new_local,"%s/%s",local,dir->d_name);
			flashfile(put_file_md5,new_local);
		}
		else if(S_ISREG(st.st_mode))
		{
			char md5_buff[MD5_BUFF_SIZE] = {0};
			Compute_file_md5(path,md5_buff);
			if(strcmp(md5_buff,put_file_md5) == 0)
				return 1;//可秒传
		}
	}
	closedir(dp);//关闭目录流
	return 0;//不可秒传
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

	int res = bind(sockfd,(struct sockaddr*)&saddr,sizeof(saddr));
	assert(res != -1);

	listen(sockfd,5);

	return sockfd;
}
