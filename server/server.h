#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sqlite3.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>

#ifndef _SERVER_H
#define _SERVER_H

#define SERVER_TCP_POT 50003
#define SERVER_UDP_POT 50001

#define MAX_EVENT_NUMS 1024

#define REGIST                00001
#define LOGIN                  00002

#define CHAT_ONE        00003
#define CHAT_ALL          00004
#define OFFLINE              00005
#define SHOW_USER     00006
#define MUTE                    00007
#define UNMUTE              00010
#define REMOVE               00011
#define FILETRANS          00012
#define FILETRANS_END   00013

#define UDP_CONNECT 49999

#define REGIST_OK       50001
#define LOGIN_OK         50002
#define MUTE_OK  50003
#define UNMUTE_OK   50004
#define REMOVE_OK    50005

#define REGIST_FAIL     60001
#define LOGIN_FAIL_IDERROR       60002
#define LOGIN_FAIL_PWDERROR       60003
#define ONLINE_USER                             60004
#define MUTE_FAIL       60005
#define UNMUTE_FAIL   60006
#define REMOVE_FAIL    60007

#define RECORD_ONE                      70001
#define RECORD_ALL                       70002
#define RECORD_END                      70003


/*
typedef struct
{
    int cmd;
    int ret;
}Ret,*Ret_p;*/

sqlite3 * pdb;

int client_fd[1024];                                         //标志客户端套接口的数组
char buffer_recv[1024];                                //服务器接收数据的缓冲区
char buffer_send[1024];                               //服务器发送数据的缓冲区

struct sockaddr_in client_addr_in[1024];  //存放套接口对应的网络地址

pthread_mutex_t lock_online;                 // 锁住在线用户哈希表
pthread_mutex_t lock_recv;                    // 锁住接收数据的缓冲区
pthread_mutex_t lock_send;                   // 锁住发送数据的缓冲区

//客户端给服务器发送的注册结构体
typedef struct 
{
    int cmd;
    char name[10];
    char pwd[15];
}Regist,*Regist_p;

//服务器给客户端回发的注册成功结构体
typedef struct 
{
    int cmd;
    int id;
}Regist_OK,*Regist_OK_p;

//客户端给服务器发送的注册结构体
typedef struct 
{
    int cmd;
    int id;
    char pwd[15];
}Login,*Login_p;

//服务器给客户端回发的注册成功结构体
typedef struct 
{
    int cmd;
    char name[10];
    int authority;
}Login_OK,*Login_OK_p;

//用来进行私聊的结构体
typedef struct
{
    int cmd;
    int user_send;  //发送聊天信息的id
    char user_name_send[10]; 
    int user_recv;   //接收聊天信息的id
    char text[100];  //聊天内容
}Chat_one,*Chat_one_p;

//用来进行群聊的结构体
typedef struct
{
    int cmd;
    int user_send;  //发送聊天信息的id
    char user_name_send[10]; 
    char text[100];  //聊天内容
}Chat_all,*Chat_all_p;

//用来存放在线用户哈希表的结点
typedef struct Online
{
    int id;
    char name[10];
    int authority;
    int say_ok;  //是否被禁言
    struct sockaddr_in addr;
    int socketfd;
    
    struct Online * next;
}Online_user,*Online_user_p;

//用来让服务器记录客户端udp端口号的结构体
typedef struct 
{
    int cmd;
    int id;
}Udp_con;

typedef struct 
{
    int cmd;
    int id;
    char name[10];
}Show_user;

typedef struct 
{
    int cmd;
    int send_id;
    int recv_id;
    char text[100];
}Record;

typedef struct
{
    int cmd;
    int mute_id;
    int send_authority;
}Mute;

typedef struct
{
    int cmd;
    int id;
    char name[10];
    int send_authority;
}Remove;

typedef struct
{
    int cmd;
    int len;
    int recv_id;
    char send_name[10];
    char file_name[20];
    char send_buffer[1024];
}File_trans,*File_trans_p;

#endif
