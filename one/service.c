/*================================================================
 *   Copyright (C) 2021 hqyj Ltd. All rights reserved.
 *   
 *   文件名称：service.c
 *   创 建 者：hupanlong
 *   创建日期：2021年03月08日
 *   描    述：
 *
 ================================================================*/
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>
#include <stdlib.h>


#define MAX_EVENTS 10
#define IP "0.0.0.0"
#define PORT 2020

typedef struct message{
	int  mod;
	int  grad;
	int  fgrad;
	int  on;
	char name[20];
	char fname[20];
	char psw[20];
	char msg[10];
	char sex[10];
	int  age;
	char phnum[20];
	int  salary;
	int  id;
	int  fid;
	char utime[64];
}__attribute__((packed)) usemsg;

void close_db(sqlite3* db);
void do_work(int fd,usemsg* use,sqlite3* db);
void log_out(int fd,usemsg* use,sqlite3* db);
int  sign_find_staff(usemsg* use,sqlite3* db,char*** pazResult,int* pnRow,int *pnColumn,char** pzErrmsg);
int  insert_staff(sqlite3* db,int id,usemsg* use);
int  count_staff(usemsg* use,sqlite3* db,char*** pazResult,int* pnRow,int *pnColumn,char** pzErrmsg);
int  log_find_staff(usemsg* use,sqlite3* db,char*** pazResult,int* pnRow,int *pnColumn,char** pzErrmsg);
void chtoi(int* num,char* arr);
void use_find_self(int fd,usemsg* use, sqlite3* db);
void use_find_other(int fd,usemsg* use, sqlite3* db);
void use_history(int fd,usemsg* use,sqlite3* db);
void use_mod(int fd,usemsg* use,sqlite3* db);
void adm_add(int fd,usemsg* use,sqlite3* db);
void adm_del(int fd,usemsg* use,sqlite3* db);
void adm_mof(int fd,usemsg* use,sqlite3* db);
void adm_traversal(int fd,usemsg* use,sqlite3* db);
void adm_history(int fd,usemsg* use,sqlite3* db);
void adm_find(int fd,usemsg* use,sqlite3* db);
void log_in(int fd,usemsg* use,sqlite3* db);
void sign_in(int fd,usemsg* use,sqlite3* db);
int  recv_user(int fd, usemsg* use,sqlite3* db);
int  send_user(int fd,usemsg* use);

struct epoll_event ev,events[MAX_EVENTS];
int conn_sock,epollfd,nfds;

int main(int argc, char *argv[])
{
	int fd;
	int ret;
	char sql[125]= "";
	char* errmsg = NULL;

	//创建或打开数据库
	sqlite3 *db = NULL;
	ret =sqlite3_open("./people.db",&db);
	if(ret != 0){
		printf("数据库打开失败!\n");
		return -1;
	}
	printf("数据库打开成功！\n");

	sprintf(sql,"create table if not exists staff(id int primary key, grade int, name char, sex char, age int, phone char, salary int, password char, online int)");
	if(sqlite3_exec(db,sql,NULL,NULL,&errmsg)!=0)
	{
		fprintf(stderr,"sqlite3_exec:%s\n",errmsg);
		close_db(db);
		return -1;
	}
	//初始化服务器
		sprintf(sql,"UPDATE staff set online=0 ");
		if(sqlite3_exec(db,sql,NULL,NULL,&errmsg)!=0)
		{
			fprintf(stderr,"sqlite3_exec:%s\n",errmsg);
			return -1;
		}

	//socket
	if((fd =socket(AF_INET,SOCK_STREAM,0))<0){
		perror("socket error\n");
		return -1;
	}
	//设置端口运作高速重用
	int reuse;
	if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))==-1){
		perror("setsockopt error\n");
		return -1;
	}
	//定义绑定数据结构体
	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);
	sin.sin_addr.s_addr = inet_addr(IP);
	socklen_t size =sizeof(sin);
	//服务器绑定信息
	if((bind(fd,(struct sockaddr*)&sin,size))<0){
		perror("bind error\n");
		return -1;
	}
	//开启内核监听
	if((listen(fd,10))<0){
		perror("listen error\n");
		return -1;
	}
	//定义已收数据结构体
	struct sockaddr_in cin;
	socklen_t csize = sizeof(cin);

	//epoll 创建一个epoll 对象同时返回描述符（epollfd）
	epollfd = epoll_create(MAX_EVENTS);
	if(epollfd == -1){
		perror("epoll create error\n");
		return -1;
	}
	int maxfd = fd,i,n;
	ev.events = EPOLLIN;
	ev.data.fd = fd;

	if(epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev) == -1){
		perror("epoll ctl :conn_sock error\n");
		return -1;
	}
	int port;
	char ip[20]={0};
	char buf[128];

	usemsg use;
	int evfd;

	while(1)
	{
		nfds = epoll_wait(epollfd,events,MAX_EVENTS,-1);
		if(nfds < -1)
		{
			perror("epoll wait error\n");
			return -1;
		}
		for(n=0;n<nfds;n++){
			if(events[n].data.fd == fd){
				conn_sock = accept(fd,(struct sockaddr*)&cin,&csize);
				if(conn_sock == -1){
					perror("accept error\n");
					return -1;
				}
				ev.events = EPOLLIN;
				ev.data.fd = conn_sock;
				if(epoll_ctl(epollfd,EPOLL_CTL_ADD,conn_sock,&ev) == -1){
					perror("epoll ctl :conn_sock error\n");
					return -1;
				}
				port =0;
				port = ntohs(cin.sin_port);
				bzero(ip,20);
				inet_ntop(AF_INET,&cin.sin_addr,ip,20);
				printf("IP:%s PORT:%d 客户端连接成功！\n",ip,port);
			}else{
				evfd = events[n].data.fd;
				//客户端通讯工作
				do_work(evfd,&use,db);

			}
		}

	}

	close(epollfd);
	close(fd);

	return 0;
}

void close_db(sqlite3* db)
{
	if(sqlite3_close(db) !=0)
	{
		fprintf(stderr,"数据库关闭失败！\n");
		return ;
	}
	fprintf(stderr,"数据库关闭成功！\n");
	return ;
}

void do_work(int fd,usemsg* use,sqlite3* db)
{
	int w_mod;
	char sql[125]= "";
	char errmsg = NULL;

	if(recv_user(fd,use,db)<0)
	{
		printf("数据接收失败！\n");
		return ;
	}
	w_mod = use->mod;
	switch(w_mod){
	case 1:
		//账户注册
		sign_in(fd,use,db);
		break;
	case 2:
		//账户登录 
		log_in(fd,use,db);
		break;
	case 3:
		//退出应用
		break;
	case 4:
		//管理员（增添）
		adm_add(fd,use,db);
		break;
	case 5:
		//管理员（删除）
		adm_del(fd,use,db);
		break;
	case 6:
		//管理员（修改）
		adm_mof(fd,use,db);
		break;
	case 7:
		//管理员（遍历）
		adm_traversal(fd,use,db);
		break;
	case 8:
		//管理员（历史记录查询）
		adm_history(fd,use,db);
		break;
	case 9:
		//管理员（查找用户信息）
		adm_find(fd,use,db);
		break;
	case 10:
		log_out(fd,use,db);
		//管理员（退出登录）

		break;
	case 11:
		//普通职工（查找个人信息）
		use_find_self(fd,use,db);
		break;
	case 12:
		//普通职工（查询他人信息）
		use_find_other(fd,use,db);
		break;
	case 13:
		//普通职工（历史记录查询）
		use_history(fd,use,db);
		break;
	case 14:
		//普通职工（修改密码）
		use_mod(fd,use,db);
		break;
	case 15:
		//普通职工（退出登录）
		log_out(fd,use,db);
		break;
	default:
		break;

	}
}

void log_out(int fd,usemsg* use,sqlite3* db)
{
	char sql[128]="";
	char* s_errmsg;
	sprintf(sql,"UPDATE staff set online=%d where name=\"%s\" ",0,use->name);
	if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
	{
		fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
		strcpy(use->msg,"false");
		use->on = 0;
		send_user(fd,use);
		return ;
	}
	strcpy(use->msg,"true");
	send_user(fd,use);
	return ;

}

int sign_find_staff(usemsg* use,sqlite3* db,char*** pazResult,int* pnRow,int *pnColumn,char** pzErrmsg)
{
	char sql[125]= "";
	sprintf(sql,"select * from staff where name=\"%s\"",use->name);
	if(sqlite3_get_table(db,sql,pazResult,pnRow,pnColumn,pzErrmsg)!=0)
	{
		fprintf(stderr,"查询数据错误:%s\n",*pzErrmsg);
		return -1;
	}
	return 0;
}

int insert_staff(sqlite3* db,int id,usemsg* use)
{
	char sql[128]="";
	char* s_errmsg;
	sprintf(sql,"insert into staff (id, grade, name, sex, age, phone, salary, password, online) values(%d,%d,\"%s\",\"none\",0,\"none\",0,\"%s\",0)",use->id,use->grad,use->name,use->psw);
	printf("%d\t %d\t %s\n",use->id,use->fgrad,use->name);
	if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
	{
		fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
		return -1;
	}
	return 0;

}

int count_staff(usemsg* use,sqlite3* db,char*** pazResult,int* pnRow,int *pnColumn,char** pzErrmsg)
{
	char sql[125]= "";
	sprintf(sql,"select * from staff");
	if(sqlite3_get_table(db,sql,pazResult,pnRow,pnColumn,pzErrmsg)!=0)
	{
		fprintf(stderr,"查询数据错误:%s\n",*pzErrmsg);
		return -1;
	}
	return 0;
}

int log_find_staff(usemsg* use,sqlite3* db,char*** pazResult,int* pnRow,int *pnColumn,char** pzErrmsg)
{
	char sql[125]= "";
	char errmsg = NULL;
	sprintf(sql,"select * from staff where name=\"%s\" and password=\"%s\"",use->name,use->psw);
	if(sqlite3_get_table(db,sql,pazResult,pnRow,pnColumn,pzErrmsg)!=0)
	{
		fprintf(stderr,"查询数据错误！\n");
		return -1;
	}
	return 0;
}

void chtoi(int* num,char* arr)
{
//	sscanf(arr,"%d",num);
	int temp=0,i=0;
	while(arr[i]!='\0')
	{
		temp = temp*10 +((int)(arr[i]-48));
		i++;
	}
	*num = temp;

	return;
}

void use_find_self(int fd,usemsg* use, sqlite3* db)
{
	char** pazResult;
	int pnRow;
	int pnColumn;
	char* pzErrmsg;
	char* s_errmsg;
	char sql[256]="";
	int i,j;
	time_t time1;
	struct tm* tm1;
	char temp;
	sprintf(sql,"create table if not exists \"%s\"(id int, grade int, name char, sex char, age int, phone char, salary int, password char,time char )",use->name);
	if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
	{
		fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
		return ;
	}
	sprintf(sql,"select * from staff where name=\"%s\"",use->name);
	if(sqlite3_get_table(db,sql,&pazResult,&pnRow,&pnColumn,&pzErrmsg)!=0)
	{
		fprintf(stderr,"查询数据错误:%s\n",pzErrmsg);
		return ;
	}
	if(pnRow>0){
		for(i=1;i<=pnRow;i++)
		{
			for(j=0;j<pnColumn;j++)
			{
				switch(j%9){
				case 0:
					//id
					chtoi(&use->fid,pazResult[i*9+j]);
					break;
				case 1:
					//grade
					chtoi(&use->fgrad,pazResult[i*9+j]);
					break;
				case 2:
					//name
					strcpy(use->fname,pazResult[i*9+j]);
					break;
				case 3:
					//sex
					strcpy(use->sex,pazResult[i*9+j]);
					break;
				case 4:
					//age
					chtoi(&use->age,pazResult[i*9+j]);
					break;
				case 5:
					//phone
					strcpy(use->phnum,pazResult[i*9+j]);
					break;
				case 6:
					//salary
					chtoi(&use->salary,pazResult[i*9+j]);
					break;
				case 7:
					//password
					strcpy(use->psw,pazResult[i*9+j]);
					break;
				case 8:
					//online
					chtoi(&use->on,pazResult[i*9+j]);
					break;
				default:
					break;

				}
			}

		}
		strcpy(use->msg,"true");
		send_user(fd,use);
		time1 = time(NULL);
		tm1 = localtime(&time1);
		sprintf(use->utime,"%d-%02d-%02d-%02d-%02d-%02d",tm1->tm_year+1900,tm1->tm_mon+1, tm1->tm_mday, tm1->tm_hour, tm1->tm_min, tm1->tm_sec);
		sprintf(sql,"insert into \"%s\"(id, grade, name, sex, age, phone, salary, password, time) values(%d, %d, \"%s\", \"%s\", %d, \"%s\", %d, \"%s\", \"%s\")",use->name,use->fid,use->fgrad,use->fname,use->sex,use->age,use->phnum,use->salary,use->psw,use->utime);

		if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
		{
			fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
			sqlite3_free_table(pazResult);
			return ;
		}
		sqlite3_free_table(pazResult);
		return ;
	}
	strcpy(use->msg,"false");
	send_user(fd,use);
	sqlite3_free_table(pazResult);
	return ;


}

void use_find_other(int fd,usemsg* use, sqlite3* db)
{
	char** pazResult;
	int pnRow;
	int pnColumn;
	char* pzErrmsg;
	char* s_errmsg;
	char sql[256]="";
	int i,j;
	time_t time1;
	struct tm* tm1;
	sprintf(sql,"create table if not exists \"%s\"(id int, grade int, name char, sex char, age int, phone char, salary int, password char,time char, online int )",use->name);
	if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
	{
		fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
		return ;
	}
	sprintf(sql,"select * from staff where name=\"%s\"",use->fname);
	if(sqlite3_get_table(db,sql,&pazResult,&pnRow,&pnColumn,&pzErrmsg)!=0)
	{
		fprintf(stderr,"查询数据错误:%s\n",pzErrmsg);
		return ;
	}
	if(pnRow>0){
		for(i=1;i<=pnRow;i++)
		{
			for(j=0;j<pnColumn;j++)
			{
				switch(j%9){
				case 0:
					//id
					chtoi(&use->fid,pazResult[i*9+j]);
					break;
				case 1:
					//grade
					use->fgrad = 0;
					break; 
				case 2:
					//name
					strcpy(use->fname, pazResult[i*9+j]);
					break;
				case 3:
					//sex
					strcpy(use->sex, "none");
					break; 
				case 4:
					//age
					use->age = 0;
					break; 
				case 5:
					//phone
					strcpy(use->phnum, pazResult[i*9+j]);
					break;
				case 6:
					//salary
					use->salary = 0;
					break;
				case 7:
					//password
					strcpy(use->psw, pazResult[i*9+j]);
					break;
				case 8:
					//online
					use->on = 0;
					break;
				default:
					break;

				}
			}
		}
		time1 = time(NULL);
		tm1 = localtime(&time1);
		sprintf(use->utime,"%d-%02d-%02d %02d-%02d-%02d",tm1->tm_year+1900,tm1->tm_mon+1, tm1->tm_mday, tm1->tm_hour, tm1->tm_min, tm1->tm_sec);
		sprintf(sql,"insert into \"%s\"(id, grade, name, sex, age, phone, salary, password, time) values(%d, %d, \"%s\", \"%s\", %d, \"%s\", %d, \"%s\", \"%s\")",use->name,use->fid,use->fgrad,use->fname,use->sex,use->age,use->phnum,use->salary,use->psw,use->utime);

		if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
		{
			fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
			strcpy(use->msg,"false");
			send_user(fd,use);
			sqlite3_free_table(pazResult);
			return ;
		}
		strcpy(use->msg,"true");
		send_user(fd,use);
		sqlite3_free_table(pazResult);
		return;
	}
	strcpy(use->msg,"false");
	send_user(fd,use);
	sqlite3_free_table(pazResult);
	return;

}

void use_history(int fd,usemsg* use,sqlite3* db)
{
	char** pazResult;
	int pnRow;
	int pnColumn;
	char* pzErrmsg;
	char sql[128]="";
	char* s_errmsg;
	int i,j;
	sprintf(sql,"select * from \"%s\"",use->name);
	if(sqlite3_get_table(db,sql,&pazResult,&pnRow,&pnColumn,&pzErrmsg)!=0)
	{
		fprintf(stderr,"查询数据错误:%s\n",pzErrmsg);
		return ;
	}
	if(pnRow>0){
		for(i=1;i<=pnRow;i++)
		{
			for(j=0;j<pnColumn;j++)
			{
				switch(j%9){
				case 0:
					//id
					chtoi(&use->fid,pazResult[i*9+j]);
					break;
				case 1:
					//grade
					chtoi(&use->fgrad,pazResult[i*9+j]);
					break;
				case 2:
					//name
					strcpy(use->fname,pazResult[i*9+j]);
					break;
				case 3:
					//sex
					strcpy(use->sex,pazResult[i*9+j]);
					break;
				case 4:
					//age
					chtoi(&use->age,pazResult[i*9+j]);
					break;
				case 5:
					//phone
					strcpy(use->phnum,pazResult[i*9+j]);
					break;
				case 6:
					//salary
					chtoi(&use->salary,pazResult[i*9+j]);
					break;
				case 7:
					//password
					strcpy(use->psw,pazResult[i*9+j]);
					break;
				case 8:
					//time
					strcpy(use->utime,pazResult[i*9+j]);
					break;
				default:
					break;

				}
			}
			strcpy(use->msg,"true");
			send_user(fd,use);
		}
		strcpy(use->msg,"end");
		send_user(fd,use);
		sqlite3_free_table(pazResult);
		return ;
	}
	strcpy(use->msg,"false");
	send_user(fd,use);
	sqlite3_free_table(pazResult);
	return ;
}

void use_mod(int fd,usemsg* use,sqlite3* db)
{
	char sql[128]="";
	char* s_errmsg;
	sprintf(sql,"UPDATE staff set phone=\"%s\"  where name=\"%s\" ",use->phnum,use->name);
	if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
	{
		fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
		strcpy(use->msg,"false");
		send_user(fd,use);
		return ;
	}
	strcpy(use->msg,"true");
	send_user(fd,use);
	return ;
	
}

void adm_add(int fd,usemsg* use,sqlite3* db)
{
	char sql[128]="";
	char* s_errmsg;
	sprintf(sql,"UPDATE staff set sex=\"%s\",  age=%d, phone=\"%s\", salary=%d where name=\"%s\"",use->sex,use->age,use->phnum,use->salary,use->fname);
	if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
	{ 
		fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
		strcpy(use->msg,"false");
		send_user(fd,use);
		return;
	}
	strcpy(use->msg,"true");
	send_user(fd,use);
	return ;
}

void adm_del(int fd,usemsg* use,sqlite3* db)
{
	char** pazResult;
	int pnRow;
	int pnColumn;
	char* pzErrmsg;
	char sql[128]="";
	char* s_errmsg;
	sprintf(sql,"drop table \"%s\" ",use->fname);
	if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
	{ 
		fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
	}
	sprintf(sql,"select * from staff where name=\"%s\"",use->name);
	if(sqlite3_get_table(db,sql,&pazResult,&pnRow,&pnColumn,&pzErrmsg)!=0)
	{
		fprintf(stderr,"查询数据错误:%s\n",pzErrmsg);
		return ;
	}
	if(pnRow>0){
		sprintf(sql,"delete from staff where name=\"%s\" ",use->fname);
		if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
		{
			fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
			strcpy(use->msg,"false");
			send_user(fd,use);
			return ;
		}
		strcpy(use->msg,"true");
		send_user(fd,use);
		return;
	}
	strcpy(use->msg,"false");
	send_user(fd,use);
	sqlite3_free_table(pazResult);
}

void adm_mof(int fd,usemsg* use,sqlite3* db)
{
	char** pazResult;
	int pnRow;
	int pnColumn;
	char* pzErrmsg;
	char sql[128]="";
	char* s_errmsg;
	sprintf(sql,"select * from staff where name=\"%s\"",use->name);
	if(sqlite3_get_table(db,sql,&pazResult,&pnRow,&pnColumn,&pzErrmsg)!=0)
	{
		fprintf(stderr,"查询数据错误:%s\n",pzErrmsg);
		return ;
	}
	if(pnRow>0){
		sprintf(sql,"UPDATE staff set phone=\"%s\", salary=%d,  password=\"%s\" where name=\"%s\"",use->phnum,use->salary,use->psw,use->fname);
		if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
		{
			fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
			strcpy(use->msg,"false");
			send_user(fd,use);
			return ;
		}
		strcpy(use->msg,"true");
		send_user(fd,use);
		return ;
	}
	strcpy(use->msg,"false");
	send_user(fd,use);
	sqlite3_free_table(pazResult);
}

void adm_traversal(int fd,usemsg* use,sqlite3* db)
{
	char** pazResult;
	int pnRow;
	int pnColumn;
	char* pzErrmsg;
	char* s_errmsg;
	char sql[128]="";
	int i,j;
	char arr[64]="";
	sprintf(sql,"select * from staff");
	if(sqlite3_get_table(db,sql,&pazResult,&pnRow,&pnColumn,&pzErrmsg)!=0)
	{
		fprintf(stderr,"查询数据错误:%s\n",pzErrmsg);
		return ;
	}
	
	if(pnRow>0){
	
		for(i=1;i<=pnRow;i++)
		{
			
			for(j=0;j<pnColumn;j++)
			{
				bzero(arr,64);	
				sprintf(arr,"%s",pazResult[i*9+j]);
				printf("%s\n",arr);
				switch(j%9){
				case 0:
					//id
					chtoi(&use->fid,arr);
					break;
				case 1:
					//grade
					chtoi(&use->fgrad,arr);
					break;
				case 2:
					//name
					strcpy(use->fname,arr);
					break;
				case 3:
					//sex
					strcpy(use->sex,arr);
					break;
				case 4:
					//age
					use->age =atoi(arr);
					break;
				case 5:
					//phone
					strcpy(use->phnum,arr);
					break;
				case 6:
					//salary
					use->salary =atoi(arr);
					break;
				case 7:
					//password
					strcpy(use->psw,arr);
					break;
				case 8:
					//online
					use->on =atoi(arr);
					break;
				default:
					break;

				}
			}

			send_user(fd,use);
		}
		strcpy(use->msg,"end");
		send_user(fd,use);
		sqlite3_free_table(pazResult);
		return ;
	}
	strcpy(use->msg,"false");
	send_user(fd,use);
	sqlite3_free_table(pazResult);
}

void adm_history(int fd,usemsg* use,sqlite3* db)
{
	char** pazResult;
	int pnRow;
	int pnColumn;
	char* pzErrmsg;
	char* s_errmsg;
	char sql[128]="";
	int i,j;
	sprintf(sql,"select * from \"%s\"",use->name);
	if(sqlite3_get_table(db,sql,&pazResult,&pnRow,&pnColumn,&pzErrmsg)!=0)
	{
		fprintf(stderr,"查询数据错误:%s\n",pzErrmsg);
		strcpy(use->msg,"false");
		send_user(fd,use);
		return ;
	}
	if(pnRow>0){
		for(i=1;i<=pnRow;i++)
		{
			for(j=0;j<pnColumn;j++)
			{
				switch(j%9){
				case 0:
					//id
					chtoi(&use->fid,pazResult[i*9+j]);
					break;
				case 1:
					//grade
					chtoi(&use->fgrad,pazResult[i*9+j]);
					break;
				case 2:
					//name
					strcpy(use->fname,pazResult[i*9+j]);
					break;
				case 3:
					//sex
					strcpy(use->sex,pazResult[i*9+j]);
					break;
				case 4:
					//age
					chtoi(&use->age,pazResult[i*9+j]);
					break;
				case 5:
					//phone
					strcpy(use->phnum,pazResult[i*9+j]);
					break;
				case 6:
					//salary
					chtoi(&use->salary,pazResult[i*9+j]);
					break;
				case 7:
					//password
					strcpy(use->psw,pazResult[i*9+j]);
					break;
				case 8:
					//time
					strcpy(use->utime,pazResult[i*9+j]);
					break;
				default:
					break;
				
				}
			}
			send_user(fd,use);
		}
		strcpy(use->msg,"end");
		send_user(fd,use);
		sqlite3_free_table(pazResult);
		return ;
	}
	strcpy(use->msg,"false");
	send_user(fd,use);
	sqlite3_free_table(pazResult);
}

void adm_find(int fd,usemsg* use,sqlite3* db)
{
	char** pazResult;
	int pnRow;
	int pnColumn;
	char* pzErrmsg;
	char* s_errmsg;
	char sql[256]="";
	int i,j;
	time_t time1;
	struct tm* tm1;
	sprintf(sql,"create table if not exists \"%s\"(id int, grade int, name char, sex char, age int, phone char, salary int, password char, time char)",use->name);
	if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
	{
		fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
		return ;
	}
	sprintf(sql,"select * from staff where name=\"%s\"",use->fname);
	if(sqlite3_get_table(db,sql,&pazResult,&pnRow,&pnColumn,&pzErrmsg)!=0)
	{
		fprintf(stderr,"查询数据错误:%s\n",pzErrmsg);
		return ;
	}
	if(pnRow >0){
		for(i=1;i<=pnRow;i++)
		{
			for(j=0;j<pnColumn;j++)
			{
				switch(j%9){
				case 0:
					//id
					chtoi(&use->fid,pazResult[i*9+j]);
					break;
				case 1:
					//grade
					chtoi(&use->fgrad,pazResult[i*9+j]);
					break;
				case 2:
					//name
					strcpy(use->fname,pazResult[i*9+j]);
					break;
				case 3:
					//sex
					strcpy(use->sex,pazResult[i*9+j]);
					break;
				case 4:
					//age
					chtoi(&use->age,pazResult[i*9+j]);
					break;
				case 5:
					//phone
					strcpy(use->phnum,pazResult[i*9+j]);
					break;
				case 6:
					//salary
					chtoi(&use->salary,pazResult[i*9+j]);
					break;
				case 7:
					//password
					strcpy(use->psw,pazResult[i*9+j]);
					break;
				case 8:
					//id
					chtoi(&use->on,pazResult[i*9+j]);
					break;
				default:
					break;

				}
			}
		}
		strcpy(use->msg,"true");
		send_user(fd,use);
		time1 = time(NULL);
		tm1 = localtime(&time1);
		sprintf(use->utime,"%d-%02d-%02d-%02d-%02d-%02d",tm1->tm_year+1900,tm1->tm_mon+1, tm1->tm_mday, tm1->tm_hour, tm1->tm_min, tm1->tm_sec);
		sprintf(sql,"insert into \"%s\" values(%d,%d,\"%s\",\"%s\",%d,\"%s\",%d,\"%s\",\"%s\")",use->name,use->fid,use->fgrad,use->fname,use->sex,use->age,use->phnum,use->salary,use->psw,use->utime);

		if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
		{
			fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
			strcpy(use->msg,"false");
			send_user(fd,use);
			return ;
		}
		sqlite3_free_table(pazResult);
		return;
	}
	sprintf(sql,"true");
	strcpy(use->msg,"false");
	send_user(fd,use);
	sqlite3_free_table(pazResult);
}

void log_in(int fd,usemsg* use,sqlite3* db)
{
	char s_name[20]="";
	char** pazResult;
	int pnRow;
	int pnColumn;
	char* pzErrmsg;
	char* s_errmsg;
	char sql[128]="";
	if(log_find_staff(use,db,&pazResult,&pnRow,&pnColumn,&s_errmsg)<0)
	{
		fprintf(stderr,"登录信息查找错误:%s\n",s_errmsg);
		strcpy(use->msg,"false");
		send_user(fd,use);
		sqlite3_free_table(pazResult);
		return ;
	}
	if(pnRow >0)
	{
		chtoi(&use->grad,pazResult[10]);
		chtoi(&use->on,pazResult[17]);
		chtoi(&use->id,pazResult[9]);
		strcpy(use->msg,"true");
		send_user(fd,use);
		use->on = 1;
		sprintf(sql,"UPDATE staff set online=%d where name=\"%s\"",use->on,use->name);
		if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
		{
			fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
			sqlite3_free_table(pazResult);
			return ;
		}
		fprintf(stderr,"%s:用户登录成功\n",use->name);
		sqlite3_free_table(pazResult);
		return;
	}else{
		strcpy(use->msg,"false");
		send_user(fd,use);
		sqlite3_free_table(pazResult);
		return ;
	}
}

void sign_in(int fd,usemsg* use,sqlite3* db)
{
	char s_name[20]="";
	char** pazResult;
	int pnRow;
	int pnColumn;
	char* pzErrmsg;
	char* s_errmsg;
	int id=0;
	if(sign_find_staff(use,db,&pazResult,&pnRow,&pnColumn,&pzErrmsg)<0)
	{
		fprintf(stderr,"查找出错:%s\n",pzErrmsg);
		strcpy(use->msg,"false");
		send_user(fd,use);
		sqlite3_free_table(pazResult);
		return;
	}
	if(pnRow ==0){

		if(count_staff(use,db,&pazResult,&pnRow,&pnColumn,&pzErrmsg)<0)
		{
			fprintf(stderr,"查找出错:%s\n",pzErrmsg);
			strcpy(use->msg,"false");
			send_user(fd,use);
			sqlite3_free_table(pazResult);
			return;
		}
		if(pnRow ==0){
			use->id = 1000;
		}else{
			printf("id:%s",pazResult[pnRow*9]);
			chtoi(&use->id,pazResult[pnRow*9]);
			(use->id)++;
		}
		if(insert_staff(db,id,use)<0)
		{
			fprintf(stderr,"注册出错:%s\n",pzErrmsg);
			strcpy(use->msg,"false");
			send_user(fd,use);
			sqlite3_free_table(pazResult);
			return;
		}
		fprintf(stderr,"注册成功!\n");
		strcpy(use->msg,"true");
		send_user(fd,use);
		sqlite3_free_table(pazResult);
		return;
	}
	else{
		strcpy(use->msg,"false");
		send_user(fd,use);
		sqlite3_free_table(pazResult);
		return;
	}
}


int recv_user(int fd, usemsg* use,sqlite3* db)
{
	int ret;
	char* s_errmsg;
	char sql[128]="";
	ret = recv(fd,use,sizeof(usemsg),0);
	if(ret < 0){
		perror("recv error\n");
		goto ERR1;
	}else if(ret == 0){
		use->on =0;
		sprintf(sql,"UPDATE staff set online=%d where name=\"%s\"",use->on,use->name);
		if(sqlite3_exec(db,sql,NULL,NULL,&s_errmsg)!=0)
		{
			fprintf(stderr,"sqlite3_exec:%s\n",s_errmsg);
			return ;
		}
		printf("%d:客户端已断开\n",fd);
		goto ERR1;
	}else{
	//	printf("用户信息接收成功\n");
		return 0;
	}
ERR1:
	ev.events = EPOLLIN;
	ev.data.fd = fd;
	if(epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,&ev) == -1){
		perror("epoll ctl error \n"); 
		return -1;
	}
	return -1;
}

int send_user(int fd,usemsg* use)
{
	int ret;
	ret = send(fd,use,sizeof(usemsg),0);
	if(ret <0){
		perror("send error\n");
		return -1;
	}else{
	//	printf("用户个人信息发送成功\n");
		return 0;
	}
}
