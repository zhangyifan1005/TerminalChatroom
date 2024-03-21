#include "sql.h"

#define HASHI_MAX 97

//创建哈希表
//使用链地址法来解决冲突
Online_user_p online[HASHI_MAX];

//注册成员
void regist(int connfd,Regist_p reg);

//成员登录
void login(int connfd,Login_p log);

//私聊发消息
void chat_one(int udpfd,Chat_one_p chat_one);

//群聊发消息
void chat_all(int udpfd,Chat_all_p chat_all);

//查看现在所有在线用户的id与用户名
void show_user(int udpfd,struct sockaddr_in client_udp);

//禁言用户
void mute_user(int udpfd,Mute mute,struct sockaddr_in client_udp);

//解除禁言用户
void unmute_user(int udpfd,Mute mute,struct sockaddr_in client_udp);

//移出群聊
void remove_user(int udpfd,Remove remove,struct sockaddr_in client_udp);

//哈希表插入
void online_insert(Online_user_p * head,int id,int authority,int connfd,char * name);

//哈希表查找 返回值为对应的网络地址代表查找成功，返回值为NULL代表查找失败
Online_user_p online_search(Online_user_p head,int id,int * authority);

//哈希表删除
int online_delete(Online_user_p * head,int id);