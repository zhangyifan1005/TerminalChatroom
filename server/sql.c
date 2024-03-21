#include "sql.h"

//创建用户表
int user_create(sqlite3 *pdb)
{
    char *sql = NULL;
    char *errmsg = NULL;
    int ret;

    sql = "create table if not exists user (id int primary key,name text,pwd text,authority int);";
    ret = sqlite3_exec(pdb, sql, NULL, NULL, &errmsg);

    if (SQLITE_OK != ret)
    {
        printf("创建用户表失败！ %s\n", errmsg);
        return -1;
    }
    return SQLITE_OK;
}

//创建聊天记录表
int record_create(sqlite3 *pdb)
{
    char *sql = NULL;
    char *errmsg = NULL;
    int ret;

    sql = "create table if not exists record (send int,recv int,records text,flag int);";
    ret = sqlite3_exec(pdb, sql, NULL, NULL, &errmsg);

    if (SQLITE_OK != ret)
    {
        printf("创建聊天记录表失败！ %s\n", errmsg);
        return -1;
    }
    return SQLITE_OK;
}

//查询自动生成的id是否存在重复
int user_exit_sameid(sqlite3 *pdb, int id)
{
    char sql[100];
    int ret;
    char **result;
    int row, colnum;
    char *errmsg;

    sprintf(sql, "select id from user where id = %d;", id);
    ret = sqlite3_get_table(pdb, sql, &result, &row, &colnum, &errmsg);
    if (ret != SQLITE_OK)
    {
        printf("get_table error! %s\n", errmsg);
        return -1;
    }
    sqlite3_free_table(result);
    if (row == 0) //row为0代表表里没有任何一行数据满足要求，所以id是不重复的
    {
        return SQLITE_OK;
    }
    else
    {
        return -1;
    }
}

//添加用户表数据
int user_add(sqlite3 *pdb, int id, Regist_p reg)
{
    char sql[100];
    int ret;
    char *errmsg;

    sprintf(sql, "insert into user (id,name,pwd,authority) values (%d,'%s','%s',%d);",
            id, reg->name, reg->pwd, 2);

    ret = sqlite3_exec(pdb, sql, NULL, NULL, &errmsg);
    if (ret != SQLITE_OK)
    {
        printf("add user error! %s\n", errmsg);
        return REGIST_FAIL;
    }
    return REGIST_OK;
}

//用户登录
int user_login(sqlite3 *pdb, Login_p log, Login_OK_p ok)
{
    char sql[100];
    int ret;
    char *errmsg;
    char **result;
    int row, colnum;

    sprintf(sql, "select * from user where id = %d;", log->id);

    printf("log.id = %d\n", log->id);

    ret = sqlite3_get_table(pdb, sql, &result, &row, &colnum, &errmsg);
    if (ret != SQLITE_OK)
    {
        printf("get table error! %s\n", errmsg);
        return -1;
    }

    if (row == 0) //若表里没有符合的数据说明该id并不存在在表中
    {
        sqlite3_free_table(result);
        return LOGIN_FAIL_IDERROR;
    }

    ret = strcmp(log->pwd, result[6]); //比较密码是否正确
    if (ret != 0)
    {
        sqlite3_free_table(result);
        return LOGIN_FAIL_PWDERROR;
    }
    //以下是密码正确的结果
    ok->cmd = LOGIN_OK;
    strcpy(ok->name, result[5]);
    ok->authority = atoi(result[7]);
    sqlite3_free_table(result);
    return LOGIN_OK;
}

//保存聊天记录  flag为1代表该消息是私聊记录，flag为2代表该消息是群聊记录
int save_record(sqlite3 *pdb, Chat_one_p chat_o, Chat_all_p chat_a, int flag)
{
    char sql[200];
    int ret;
    char *errmsg;

    if (flag == 1)
    {
        sprintf(sql, "insert into record (send,recv,records,flag) values (%d,%d,'%s',%d);",
                chat_o->user_send, chat_o->user_recv, chat_o->text, flag);
        ret = sqlite3_exec(pdb, sql, NULL, NULL, &errmsg);
        if (ret != SQLITE_OK)
        {
            printf("add chat_one record error! %s\n", errmsg);
            return -1;
        }
        return 0;
    }
    else if (flag == 2)
    {
        sprintf(sql, "insert into record (send,recv,records,flag) values (%d,%d,'%s',%d);",
                chat_a->user_send, 0, chat_a->text, flag);
        ret = sqlite3_exec(pdb, sql, NULL, NULL, &errmsg);
        if (ret != SQLITE_OK)
        {
            printf("add chat_all record error! %s\n", errmsg);
            return -1;
        }
        return 0;
    }
    else
    {
        return -1;
    }
}

//查找服务器端保存的聊天记录
int search_record(sqlite3 * pdb,int connfd,int id)
{
    char sql[200];
    int ret;
    char * errmsg = NULL;
    int cmd;

    sprintf(sql,"select * from record where send = %d or recv = %d or flag = 2;",id,id);
    ret = sqlite3_exec(pdb,sql,send_record,(void *)&connfd,&errmsg);
    if(ret != SQLITE_OK)
    {
        printf("search record error! %s\n",errmsg);
        return -1;
    }

    cmd = RECORD_END;
    send(connfd,&cmd,sizeof(cmd),0);
    return 0;
}

//执行查找并向客户端发送聊天记录数据的回调函数
int send_record(void* para,int columnCount,char** columnValue,char** columnName)
{
    Record record;
    int connfd = *((int *)para);
    int cmd;

    cmd = atoi(columnValue[3]);
    if(cmd == 1)
    {
        record.send_id = atoi(columnValue[0]);
        record.recv_id = atoi(columnValue[1]);
        strcpy(record.text,columnValue[2]);
        record.cmd = RECORD_ONE;
        send(connfd,&record,sizeof(record),0);
        printf("send record success! cmd = %d\n",record.cmd);
        printf("text = %s\n",record.text);
    }
    else if(cmd == 2)
    {
        record.send_id = atoi(columnValue[0]);
        strcpy(record.text,columnValue[2]);
        record.cmd = RECORD_ALL;
        send(connfd,&record,sizeof(record),0);
        printf("send record success! cmd = %d\n",record.cmd);
        printf("text = %s\n",record.text);
    }
    else
    {
        printf("cmd = %d",cmd);
    }
    usleep(500);
    return 0;
}