#include "operate.h"

//注册成员
void regist(int connfd, Regist_p reg)
{
    int ret;
    int id;

    printf("enter regist\n");

    srand((unsigned int)time(NULL)); //给随机数种子赋值

    //生成随机id,并保证id与数据库中已存在的id不重复
    while (1)
    {
        id = 10001 + rand() % 89999;     //生成随机数
        ret = user_exit_sameid(pdb, id); //查数据库，保证id不充分
        if (ret == SQLITE_OK)
        {
            break;
        }
    }

    ret = user_add(pdb, id, reg); //添加新用户到数据库
    if (ret != REGIST_OK)         //添加出错
    {
        send(connfd, &ret, sizeof(ret), 0);
    }
    else
    {
        Regist_OK ok;
        ok.cmd = REGIST_OK;
        ok.id = id;                       //把新生成的用户id回发给客户端
        send(connfd, &ok, sizeof(ok), 0); //添加成功
    }
}

//成员登录
void login(int connfd, Login_p log)
{
    int ret;
    Login_OK ok;

    ret = user_login(pdb, log, &ok); //用户登录
    //给客户端回发错误码
    if (ret == -1)
    {
        exit(-1);
    }
    else if (ret == LOGIN_FAIL_IDERROR)
    {
        send(connfd, &ret, sizeof(ret), 0);
    }
    else if (ret == LOGIN_FAIL_PWDERROR)
    {
        send(connfd, &ret, sizeof(ret), 0);
    }
    else
    {
        send(connfd, &ok, sizeof(ok), 0); //给客户端回发插入成功

        search_record(pdb, connfd, log->id);

        //给哈希表操作上锁
        pthread_mutex_lock(&lock_online);

        online_insert(online + (log->id % HASHI_MAX), log->id, ok.authority, connfd, ok.name); //并把该用户加入在线用户哈希表

        //给哈希表操作解锁
        pthread_mutex_unlock(&lock_online);
    }
}

//私聊发消息
void chat_one(int udpfd, Chat_one_p chat_one)
{
    struct sockaddr_in client_addr;
    Online_user_p node_addr;
    int id, authority;
    int ret;

    //通过私聊结构体中消息的收件人在在线用户哈希表中进行查找，并返回指向该结点的指针
    id = chat_one->user_recv;
    node_addr = online_search(online[id % HASHI_MAX], id, &authority);

    if (node_addr != NULL)
    {
        //由此便可以找到收件人客户端的ip地址与端口号
        client_addr = node_addr->addr;

        //服务器转发消息
        ret = sendto(udpfd, chat_one, sizeof(Chat_one), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
        if (ret < 0)
        {
            printf("chat one send error!\n");
        }
    }
}

//群聊发消息
void chat_all(int udpfd, Chat_all_p chat_all)
{
    struct sockaddr_in client_addr;
    Online_user_p node_addr;
    int id, authority;
    int ret;

    //遍历哈希表，寻找每一位在线成员
    for (int i = 0; i < HASHI_MAX; i++)
    {
        node_addr = online[i];
        while (node_addr != NULL)
        {
            client_addr = node_addr->addr; // 找到该用户的ip地址与端口号
            //printf("send to client <%s:%d>\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            ret = sendto(udpfd, chat_all, sizeof(Chat_all), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)); //转发消息
            //printf("send! ret = %d\n",ret);
            node_addr = node_addr->next; //继续遍历
        }
    }
}

void show_user(int udpfd, struct sockaddr_in client_udp)
{
    Show_user show_user;
    Online_user_p user;

    show_user.cmd = ONLINE_USER;

    for (int i = 0; i < HASHI_MAX; i++)
    {
        user = online[i];
        while (user != NULL)
        {
            show_user.id = user->id;
            strcpy(show_user.name, user->name);
            sendto(udpfd, &show_user, sizeof(show_user), 0, (struct sockaddr *)&client_udp, sizeof(client_udp));
            user = user->next;
        }
    }
}

//禁言用户
void mute_user(int udpfd,Mute mute,struct sockaddr_in client_udp)
{
    Online_user_p user = NULL;
    //struct sockaddr_in client_addr;
    int authority;
    int cmd;

    user = online_search(online[mute.mute_id % HASHI_MAX],mute.mute_id,&authority);
    if(user == NULL)
    {
        cmd = MUTE_FAIL;
        sendto(udpfd,&cmd,sizeof(cmd),0, (struct sockaddr *)&client_udp, sizeof(client_udp));
    }
    else if(mute.send_authority < user->authority)
    {
        user->say_ok = 1;
        cmd = MUTE_OK;
        sendto(udpfd,&cmd,sizeof(cmd),0,(struct sockaddr *)&(user->addr),sizeof(user->addr));
    }
    else
    {
        cmd = MUTE_FAIL;
        sendto(udpfd,&cmd,sizeof(cmd),0, (struct sockaddr *)&client_udp, sizeof(client_udp));
    }
}

//解除禁言用户
void unmute_user(int udpfd,Mute mute,struct sockaddr_in client_udp)
{
    Online_user_p user = NULL;
    int authority;
    int cmd;

    user = online_search(online[mute.mute_id % HASHI_MAX],mute.mute_id,&authority);
    if(user == NULL)
    {
        cmd = UNMUTE_FAIL;
        sendto(udpfd,&cmd,sizeof(cmd),0, (struct sockaddr *)&client_udp, sizeof(client_udp));
    }
    else if(mute.send_authority < user->authority)
    {
        user->say_ok = 0;
        cmd = UNMUTE_OK;
        sendto(udpfd,&cmd,sizeof(cmd),0,(struct sockaddr *)&(user->addr),sizeof(user->addr));
    }
    else
    {
        cmd = UNMUTE_FAIL;
        sendto(udpfd,&cmd,sizeof(cmd),0, (struct sockaddr *)&client_udp, sizeof(client_udp));
    }
}

//移出群聊
void remove_user(int udpfd,Remove remove,struct sockaddr_in client_udp)
{
    Online_user_p user = NULL;
    int authority;
    int cmd;

    user = online_search(online[remove.id % HASHI_MAX],remove.id ,&authority);
    if(user == NULL)
    {
        cmd = REMOVE_FAIL;
        sendto(udpfd,&cmd,sizeof(cmd),0, (struct sockaddr *)&client_udp, sizeof(client_udp));
    }
    else if(remove.send_authority < user->authority)
    {
        cmd = REMOVE_OK;
        sendto(udpfd,&cmd,sizeof(cmd),0,(struct sockaddr *)&(user->addr),sizeof(user->addr));
        online_delete(&online[remove.id % HASHI_MAX],remove.id );
    }
    else
    {
        cmd = REMOVE_FAIL;
        sendto(udpfd,&cmd,sizeof(cmd),0, (struct sockaddr *)&client_udp, sizeof(client_udp));
    }
}


//哈希表插入
void online_insert(Online_user_p *head, int id, int authority, int connfd, char *name) //本质是不带头结点的链表头插法
{
    Online_user_p user = NULL;

    user = (Online_user_p)malloc(sizeof(Online_user));
    user->id = id;
    user->authority = authority;
    user->say_ok = 0;
    user->addr = client_addr_in[connfd];
    user->next = NULL;
    user->socketfd = connfd;
    strcpy(user->name, name);
    if (*head == NULL)
    {
        *head = user;
    }
    else
    {
        user->next = *head;
        *head = user;
    }
    printf("insert successful %p\n", user);
}

//哈希表的定义在operate.h中

//哈希表查找
Online_user_p online_search(Online_user_p head, int id, int *authority) //本质是不带头结点的链表查找
{
    if (head == NULL)
    {
        return NULL;
    }
    else
    {
        Online_user_p p = head;
        while (p != NULL)
        {
            if (p->id != id)
            {
                p = p->next;
            }
            else
            {
                return p;
            }
        }
        return NULL;
    }
}

//哈希表删除
int online_delete(Online_user_p *head, int id) //本质是不带头结点的链表的结点删除
{
    Online_user_p p = *head;
    Online_user_p q = NULL;

    if (p->id == id)
    {
        *head = p->next;
        free(p);
    }
    else
    {
        while (p->next != NULL)
        {
            if (p->next->id == id)
            {
                q = p->next;
                p->next = q->next;
                free(q);
                return 0;
            }
            p = p->next;
        }
    }
    return 0;
}