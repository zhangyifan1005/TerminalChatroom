#include "client.h"


//和服务器同步聊天记录
void record_update(int sockfd,char * filename);

//进行聊天操作
void user_chat(int sockfdint,int udpfd,struct  sockaddr_in server_addr,char * name);

//进行管理员操作
void admin_chat(int sockfd,int udpfd,struct  sockaddr_in server_addr,char * name,int authority);

//监听线程的回调函数
void * chat_listen(void * arg);

//打印聊天页面
void chat_menu();

//打印管理员页面
void admin_menu();

//表情包管理
void emoji_manage(char * text);

//文件传输
void file_trans(int udpfd,char * name);