#include "server.h"

//创建用户表
int user_create(sqlite3 * pdb);

//创建聊天记录表
int record_create(sqlite3 * pdb);

//查询自动生成的id是否存在重复
int user_exit_sameid(sqlite3 * pdb,int id);

//注册用户
int user_add(sqlite3 * pdb,int id,Regist_p reg);

//用户登录
int user_login(sqlite3 * pdb,Login_p log,Login_OK_p ok);

//保存聊天记录  flag为1代表该消息是私聊记录，flag为2代表该消息是群聊记录
int save_record(sqlite3 * pdb,Chat_one_p chat_o,Chat_all_p chat_a,int flag);

//查找服务器端保存的聊天记录
int search_record(sqlite3 * pdb,int connfd,int id);

//执行查找并向客户端发送聊天记录数据的回调函数  参数需要：向客户端发送的套接口
int send_record(void* para,int columnCount,char** columnValue,char** columnName);