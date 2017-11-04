#include<stdio.h>  
#include<stdlib.h>  
#include<string.h>  
#include<errno.h>  
#include<sys/types.h>  
#include<sys/socket.h>  
#include<netinet/in.h>  
#include <pthread.h>  

#define MAXLINE 4096 

int    sockfd, n,rec_len;  
char    recvline[4096], sendline[4096];  
char    buf[MAXLINE];  
struct sockaddr_in    servaddr; 
static int exit_flag=0;
  

void* recv_from(){
    while(1){
        if(exit_flag==1) pthread_exit(0);
        if((rec_len = recv(sockfd, buf, MAXLINE,0)) == -1) {  
            perror("recv error");  
            exit(1);  
        }else{
            buf[rec_len]  = '\0';  
            printf("\033[1;32mRemoteHost:\33[0m\n");
            printf("%s ",buf);
        }
    } 
}
char * out="exit";
void* send_to(){
    while(1){
        fgets(sendline, 4096, stdin); 
        if( send(sockfd, sendline, strlen(sendline), 0) < 0)  
        {  
            printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);  
            exit(0);  
        }
        if(strcmp(sendline,out)==0) {
            exit_flag=1;
            pthread_exit(0);
        }
    }  
}

int main(int argc, char** argv)  
{  
 
  
  
    if( argc != 2){  
    printf("usage: ./client <ipaddress>\n");  
    exit(0);  
    }  
  
  
    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){  
    printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);  
    exit(0);  
    }  
  
  
    memset(&servaddr, 0, sizeof(servaddr));  
    servaddr.sin_family = AF_INET;  
    servaddr.sin_port = htons(10000);  
    if( inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0){  
        printf("inet_pton error for %s\n",argv[1]);  
        exit(0);  
    }  
  
  
    if( connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){  
        printf("connect error: %s(errno: %d)\n",strerror(errno),errno);  
        exit(0);  
    }  


    if((rec_len = recv(sockfd, buf, MAXLINE,0)) == -1) {  
        perror("recv error");  
        exit(1);  
    }else{
        buf[rec_len]  = '\0';  
        printf("\033[1;32mRemoteHost:\33[0m\n");
        printf("%s ",buf);
    }
    
    pthread_t recv_pthread, sned_pthread;
    int rc1=0, rc2=0;
    //创建发送和接收线程
	rc2 = pthread_create(&recv_pthread, NULL, recv_from, &sockfd);
	if(rc2 != 0)
		printf("%s: %d\n",__func__, strerror(rc2));

	rc1 = pthread_create(&sned_pthread, NULL, send_to, &sockfd);
	if(rc1 != 0)
		printf("%s: %d\n",__func__, strerror(rc1));
    pthread_join(recv_pthread, 0);      
    pthread_join(sned_pthread, 0);  
        
    close(sockfd);  
    exit(0);  
}  

