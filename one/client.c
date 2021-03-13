/*================================================================
 *   Copyright (C) 2021 hqyj Ltd. All rights reserved.
 *   
 *   文件名称：client.c
 *   创 建 者：hupanlong
 *   创建日期：2021年03月09日
 *   描    述：
 *
 ================================================================*/
#include <stdio.h>
#include <sys/types.h>        
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#define PORT 2020
#define IP "192.168.8.157"


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

void do_work(int fd);
int sign_in(int fd,usemsg* use);
void log_in(int fd,usemsg* use);
void wait_akey(void);
void input_use_pwd(usemsg* use);
void input_use_name(usemsg* use);
void input_use_fname(usemsg* use);
void input_use_salary(usemsg* use);
void inpt_use_phone(usemsg* use);
void input_use_age(usemsg* use);
void input_use_sex(usemsg* use);
void use_mod_sel(usemsg* use);
void adm_mod_sel(usemsg* use);
void mod_sel(usemsg* use);
void grade_sel(usemsg* use);
int recv_user(int fd, usemsg* use);
int send_user(int fd,usemsg* use);

int main(int argc, char *argv[])
{

	int fd;
	//创建socket套接字
	fd = socket(AF_INET,SOCK_STREAM,0);
	if(fd < 0){
		perror("socket error\n");
		printf("%d:---------",__LINE__);
		return -1;
	}
	int reuse = 1;
	socklen_t len = sizeof(reuse);
	if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuse,len)== -1)
	{
		perror("setsockopt error\n");
		return -1;
	}

	//定义连接信息
	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);
	sin.sin_addr.s_addr = inet_addr(IP);
	//练级服务器
	if(connect(fd,(struct sockaddr*)&sin,sizeof(sin))== -1){
		perror("connect error\n");
		printf("%d:---------",__LINE__);
		return -1;
	}
	char buf[128];
	int ret;
	//数据发送工作
	do_work(fd);
	close(fd);

	return 0;
}

void do_work(int fd)
{
	usemsg w_use;
	int m,g;
	while(1)
	{
		//模式选择
		mod_sel(&w_use);
		m = w_use.mod;
		switch(m){
		case 1:
			//账户注册
			sign_in(fd,&w_use);
			continue;
		case 2:
			//账号登录
			log_in(fd,&w_use);
			continue;
		case 3:
			//软件退出
			return ;
		default:
			continue;
		}
		return;
	}
}
//注册
int sign_in(int fd,usemsg* use)
{
	while(1){
		//获取用户等级
		grade_sel(use);
		if((use->fgrad >2) || (use->fgrad <1))
		{
			break;
		}
		//获取用户姓名和密码
		input_use_name(use);
		input_use_pwd(use);
		//发送注册用户信息
		if(send_user(fd,use)<0)
		{
			printf("注册信息发送失败\n");
			continue;
		}
		//接收注册信息返回信息
		if(recv_user(fd,use)<0)
		{
			printf("接收注册返回信息失败\n");
			continue;
		}
		if(!(strcmp("true",use->msg)))
		{
			printf("%s:用户注册成功\n",use->name);
			break;
		}else{
			printf("用户注册失败！\n");
			return -1;
		}
	}
	return 0;
}

//登录
void log_in(int fd,usemsg* use)
{
	int i=0;
	while(1){
		//获取登录用户姓名和密码
		input_use_name(use);
		input_use_pwd(use);
		//发送用户登录信息
		if(send_user(fd,use)<0)
		{
			printf("用户登录信息发送失败\n");
			i++;
			if(i>=3){
				break;
			}
			continue;
		}
		//接收用户登录反馈信息
		if(recv_user(fd,use)<0)
		{
			printf("用户登录信息反馈信息接收失败\n");
			i++;
			if(i>=3){
				break;
			}
			continue;
		}
		if(strcmp("true",use->msg)==0 && use->mod != 3 && use->on !=1){
			//登录成功
			switch(use->grad){
			case 2:
				while(1){
					//普通职工登录成功
					use_mod_sel(use);
					if((use->mod > 10)&& (use->mod<15))
					{
						//普通用户模式选择成功
						switch((use->mod)-10){
						case 1:
							//查看自己信息
							strcpy(use->msg,"null");
							if(send_user(fd,use)<0){
								perror("用户查询发送失败！\n");
								wait_akey();
								break;
							}
							if(recv_user(fd,use)<0){
								perror("用户查询个人信息接收失败\n");
								wait_akey();
								break;
							}
							if(strcmp(use->msg,"true")==0){
								printf("-----------------------------个人信息----------------------------\n");
								printf("工号\t等级\t姓名\t性别\t年龄\t手机号\t薪资\t密码\n");
								printf("%d\t %d\t %s\t %s\t %d\t %s\t %d\t %s\n",use->fid,use->fgrad,use->fname,use->sex,use->age,use->phnum,use->salary,use->psw);
								wait_akey();
								break;
							}
							printf("查询失败！\n");
							wait_akey();
							break;
						case 2:
							//查看他人信息
							strcpy(use->msg,"null");
							input_use_fname(use);
							if(send_user(fd,use)<0){
								perror("用户查询发送失败！\n");
								wait_akey();
								break;
							}
							if(recv_user(fd,use)<0){
								perror("用户查询个人信息接收失败\n");
								wait_akey();
								break;
							}
							if(strcmp(use->msg,"true")==0){
								printf("-----------------------------他人信息----------------------------\n");
								printf("工号\t等级\t姓名\t性别\t年龄\t手机号\t薪资\t密码\n");
								printf("%d\t %d\t %s\t %s\n",use->fid,use->fgrad,use->fname,use->phnum);
								wait_akey();
								break;
							}
							printf("信息查询失败！\n");
							wait_akey();
							break;
						case 3:
							//查看历史记录
							strcpy(use->msg,"null");
							if(send_user(fd,use)<0){
								perror("用户查询发送失败！\n");
								wait_akey();
								break;
							}
							while(1){
								if(recv_user(fd,use)<0){
									perror("用户查询接收失败！\n");
									wait_akey();
									break;
								}
								if(strcmp(use->msg,"end")==0){
									wait_akey();
									break;
								}

								printf("%d\t %d\t %s\t %s\t %d\t %s\t %d\t %s\t %s\n",use->fid,use->fgrad,use->fname,use->sex,use->age,use->phnum,use->salary,use->psw,use->utime);
								wait_akey();
								break;
							}

							break;
						case 4:
							//修改用户密码
							inpt_use_phone(use);
							strcpy(use->msg,"null");
							if(send_user(fd,use)<0){
								perror("信息发送失败！\n");
								wait_akey();
								break;
							}
							if(recv_user(fd,use)<0){
								perror("信息接收失败\n");
								wait_akey();
								break;
							}
							if(strcmp(use->msg,"true")==0){
								printf("用户信息修改成功！\n");
								wait_akey();
								break;
							}
							printf("用户信息修改失败！\n");
							break;
						default:
							break;
						}
					}
					else if ( use->mod == 15){
						if(send_user(fd,use)<0){
							perror("普通职工退出发送失败！\n");
							wait_akey();	
							continue;
						}
						if(recv_user(fd,use)<0){
							perror("账号退出信息接收失败\n");
							wait_akey();
							continue;
						}
						if(strcmp(use->msg,"true")==0){
							printf("账号退出成功！\n");
							wait_akey();
							break;
						}
						printf("msg:%s",use->msg);
						printf("账号退出失败！\n");
						wait_akey();
						continue;
					}
					else{
						printf("模式选择错误\n");
						wait_akey();
						continue;
					}
				}
				break;
			case 1:
				while(1){
					//管理员登录成功
					adm_mod_sel(use);
					if((use->mod> 3) && (use->mod <10))
					{
						//管理员模式选择成功
						switch((use->mod)-3){
						case 1:
							//增添用户信息
							strcpy(use->msg,"null");
							input_use_fname(use);
							input_use_salary(use);
							inpt_use_phone(use);
							input_use_age(use);
							input_use_sex(use);
							if(send_user(fd,use)<0){
								perror("管理员信息发送失败\n");
								wait_akey();	
								break;
							}
							if(recv_user(fd,use)<0){
								perror("管理员信息接收失败\n");
								wait_akey();
								break;
							}
							if(strcmp(use->msg,"true")==0){
								printf("信息修改成功！\n");
								break;
							}
							printf("信息修改失败！\n");

							break;
						case 2:
							//删除用户信息
							strcpy(use->msg,"null");
							input_use_fname(use);
							if(send_user(fd,use)<0){
								perror("管理员信息发送失败\n");
								wait_akey();	
								break;
							}
							if(recv_user(fd,use)<0){
								perror("管理员信息接收失败\n");
								wait_akey();
								break;
							}
							if(strcmp(use->msg,"true")==0){
								printf("成员删除成功！\n");
								break;
							}
							printf("成员删除失败！\n");

							break;
						case 3:
							//修改用户信息
							strcpy(use->msg,"null");
							input_use_fname(use);
							input_use_salary(use);
							inpt_use_phone(use);
							input_use_pwd(use);
							if(send_user(fd,use)<0){
								perror("管理员信息发送失败\n");
								wait_akey();	
								break;
							}
							if(recv_user(fd,use)<0){
								perror("管理员信息接收失败\n");
								wait_akey();
								break;
							}
							if(strcmp(use->msg,"true")==0){
								printf("成员修改成功！\n");
								break;
							}
							printf("成员修改失败！\n");
							break;
						case 4:
							//遍历用户信息
							strcpy(use->msg,"null");
							if(send_user(fd,use)<0){
								perror("管理员信息发送失败\n");
								wait_akey();	
								break;
							}
							while(1){
								if(recv_user(fd,use)<0){
									perror("管员员接收失败！\n");
									wait_akey();
									break;
								}
								if(strcmp(use->msg,"end")==0){
									perror("----end----！\n");
									wait_akey();
									break;
								}else if(strcmp(use->msg,"false")==0){
									perror("无历史记录！\n");
									wait_akey();
									break;
								}
								printf("%d\t%d\t%s\t%s\t%d\t%s\t%d\t%s\t%d\n",use->fid,use->fgrad,use->fname,use->sex,use->age,use->phnum,use->salary,use->psw,use->on);
							}

							break;
						case 5:
							//查看历史记录
							strcpy(use->msg,"null");
							if(send_user(fd,use)<0){
								perror("管理员信息发送失败\n");
								wait_akey();	
								break;
							}
							while(1){
								if(recv_user(fd,use)<0){
									perror("管理员信息接收失败！\n");
									wait_akey();
									break;
								}
								if(strcmp(use->msg,"end")==0){
									perror("----end----！\n");
									wait_akey();
									break;
								}else if(strcmp(use->msg,"false")==0){
									perror("无历史记录！\n");
									wait_akey();
									break;
								}

								printf("%d\t%d\t%s\t%s\t%d\t %s\t%d\t%s\t%s\n",use->fid,use->fgrad,use->fname,use->sex,use->age,use->phnum,use->salary,use->psw,use->utime);
							}
							break;
						case 6:
							//查询用户信息询
							strcpy(use->msg,"null");
							input_use_fname(use);
							if(send_user(fd,use)<0){
								perror("管理员信息发送失败\n");
								wait_akey();	
								break;
							}
							if(recv_user(fd,use)<0){
								perror("管理员信息接收失败\n");
								wait_akey();
								break;
							}
							if(strcmp(use->msg,"true")==0){
								printf("信息接收成功！\n");
								printf("%d\t%d\t%s\t%s\t%d\t %s\t%d\t%s\t%d\n",use->fid,use->fgrad,use->fname,use->sex,use->age,use->phnum,use->salary,use->psw,use->on);
								wait_akey();
								break;
							}
							printf("信息修改失败！\n");

							break;
						default:
							break;
						}
					}
					else if( use->mod == 10){
						if(send_user(fd,use)<0){
							perror("管理员退出发送信息\n");
							wait_akey();	
							continue;
						}
						if(recv_user(fd,use)<0){
							perror("账号退出信息接收失败\n");
							wait_akey();
							continue;
						}
						if(strcmp(use->msg,"true")==0){
							printf("账号退出成功！\n");
							wait_akey();
							break;
						}
						printf("账号退出失败！\n");
						wait_akey();
						continue;
					}
					else{
						printf("msg:%s mod:%d\n",use->msg,use->mod);
						printf("%d:模式选择错误\n",use->mod);
						wait_akey();
						continue;
					}
					continue;
				}
				break;
			default:
				break;

			}
			break;
		}
		else if(strcmp("true",use->msg)==0 && (use->mod == 3 || use->on == 1)){
			if(use->on == 1){
				printf("账户已登录!\n");
				int n;
				printf("——————————请选择——————————\n--退出:0 --继续：1-->");
				scanf("%d",&n);
				while(getchar()!='\n');
				if(n==0){
					use->mod = 2;
					return;
				}
				i++;
				wait_akey();
			}
			else{
				printf("退出登录成功!\n");
				wait_akey();
				use->mod = 2;
				return;

			}
			if(i>3)
			{
				break;
			}
			continue;
		}
		else{
			int n;
			printf("密码或账户错误\n");
			printf("——————————请选择——————————\n--退出:0 --继续：1-->");
			scanf("%d",&n);
			while(getchar()!='\n');
			wait_akey();
			if(n==0){
				use->mod = 2;
				return;
			}
			i++;
			if(i>=3){
				break;
			}
			continue;
		}
	}
	return ;
}

void wait_akey(void)
{
	printf("输入任意键继续\n");
	getchar();
	return;
}

void input_use_pwd(usemsg* use)
{
	char u_pwd[20];
	//填写密码
	printf("请输用户密码>");
	scanf("%s",u_pwd);
	strcpy(use->psw,u_pwd);
	putchar(10);
	while(getchar()!='\n');
	//返回
	return ;
}

void input_use_name(usemsg* use)
{
	char u_name[20];
	//填写姓名
	printf("请输用户姓名>");
	scanf("%s",u_name);
	strcpy(use->name,u_name);
	putchar(10);
	while(getchar()!='\n');
	//返回
	return ;
}

void input_use_fname(usemsg* use)
{
	char u_name[20];
	//填写姓名
	printf("请输用户姓名>");
	scanf("%s",u_name);
	strcpy(use->fname,u_name);
	putchar(10);
	while(getchar()!='\n');
	//返回
	return ;
}

void input_use_salary(usemsg* use)
{
	int  u_salary;
	//填写薪资
	printf("请输用户薪资>");
	scanf("%d",&u_salary);
	use->salary =u_salary;
	putchar(10);
	while(getchar()!='\n');
	//返回
	return ;
}

void inpt_use_phone(usemsg* use)
{
	char u_phnum[20];
	//填写姓名
	printf("请输入用户手机号>");
	scanf("%s",u_phnum);
	strcpy(use->phnum,u_phnum);
	putchar(10);
	while(getchar()!='\n');
	//返回
	return ;
}

void input_use_age(usemsg* use)
{
	int  u_age;
	//填写年龄
	printf("请输入用户年龄>");
	scanf("%d",&u_age);
	use->age =u_age;
	putchar(10);
	while(getchar()!='\n');
	//返回
	return ;
}

void input_use_sex(usemsg* use)
{
	char u_sex[10];
	//填写性别
	printf("请输入用户性别 male(男)，female(女)>");
	scanf("%s",u_sex);
	strcpy(use->sex,u_sex);
	putchar(10);
	while(getchar()!='\n');
	//返回
	return ;
}

void use_mod_sel(usemsg* use)
{
	int i;
	//模式选择
	system("clear");
	printf("模 式 选 择\n");
	printf("查看自己信息 1\n");
	printf("查看他人信息 2\n");
	printf("历史查看记录 3\n");
	printf("修改用户密码 4\n");
	printf("退 出 选 择  5\n");
	printf("------------------\n");
	printf("请选择>");
	scanf("%d",&i);
	use->mod = i+10;
	while(getchar()!='\n');
	return ;
}

void adm_mod_sel(usemsg* use)
{
	int i;
	//模式选择
	system("clear");
	printf("模 式 选 择\n");
	printf("增加用户信息 1\n");
	printf("删除用户信息 2\n");
	printf("修改用户信息 3\n");
	printf("遍历用户信息 4\n");
	printf("历史记录查看 5\n");
	printf("查询用户信息 6\n");
	printf("退 出 选 择  7\n");
	printf("------------------\n");
	printf("请选择>");
	scanf("%d",&i);
	use->mod = i+3;
	while(getchar()!='\n');
	return ;
}

void mod_sel(usemsg* use)
{
	int i;
	//模式选择
	system("clear");
	printf("模 式 选 择\n");
	printf("账户注册请选 1\n");
	printf("账户登录请选 2\n");
	printf("软件退出请选 3\n");
	printf("------------------\n");
	printf("请选择>");
	scanf("%d",&i);
	use->mod = i;
	while(getchar()!='\n');
	return ;
}

void grade_sel(usemsg* use)
{
	int i;
	//身份选择
	system("clear");
	printf("身 份 选 择\n");
	printf("管理员请选   1\n");
	printf("普通职工请选 2\n");
	printf("退 出 选 择  3\n");
	printf("------------------\n");
	printf("请选择>");
	scanf("%d",&i);
	use->grad = i;
	while(getchar()!='\n');
	return ;
}

int recv_user(int fd, usemsg* use)
{
	int ret;
	ret = recv(fd,use,sizeof(usemsg),0);
	if(ret < 0){
		perror("recv error\n");
		return -1;
	}else{
		printf("用户信息获取成功\n");
		return 0;
	}
}

int send_user(int fd,usemsg* use)
{
	int ret;
	ret = send(fd,use,sizeof(usemsg),0);
	if(ret <0){
		perror("send error\n");
		return -1;
	}else{
		printf("用户个人信息发送成功\n");
		return 0;
	}
}

