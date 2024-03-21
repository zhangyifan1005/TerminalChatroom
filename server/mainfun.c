#include "mainfun.h"

//创建监听套接口：参数为端口号
int start_listensocket(int port)
{
    int listenfd;
    struct sockaddr_in server_addr;
    int opt = 1;
    int ret;

    //建立监听套接口（tcp套接口)
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        perror("listen error");
        exit(EXIT_FAILURE);
    }

    //设置服务器ip地址与端口号方便绑定
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //设置允许重复绑定
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //服务器绑定ip地址与端口号
    ret = bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0)
    {
        perror("bind error");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    //监听来自客户端的tcp连接请求
    ret = listen(listenfd, 100);
    if (ret < 0)
    {
        perror("listen error");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    return listenfd;
}

//向epoll池中添加事件：参数为epoll池文件描述符，套接口文件描述符，ctl事件
int epoll_event_add(int epfd, int fd, int event)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = event;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

//向epoll池中删除事件：参数为epool池文件描述符，套接口文件描述符，ctl事件
int epoll_event_del(int epfd, int fd, int event)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = event;
    return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev);
}

//接受监听套接口中的信息：参数为监听套接口文件描述符与epoll池文件描述符
int do_accept(int listenfd, int epfd)
{
    struct sockaddr_in client_addr;
    int connfd;
    int len;

    len = sizeof(client_addr);

    //从监听队列中取出一个套接口建立tcp连接
    connfd = accept(listenfd, (struct sockaddr *)&client_addr, &len);
    if (connfd < 0)
    {
        perror("accept error");
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    printf("Accept new client <%s:%d>\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    //给用来记录套接口对应ip地址的数组赋值
    client_addr_in[connfd] = client_addr;

    //将标志客户端连接套接口的数组对应位置标记为开启状态
    client_fd[connfd] = 1;

    //将连接套接口加入epoll池，监听其请求
    epoll_event_add(epfd, connfd, EPOLLIN | EPOLLET); //EPOLLIN为可读事件

    return connfd;
}

//接收客户端发来的登录数据并进行处理：参数为epoll池文件描述符，epoll池任务队列与当前任务的队列下标
int recv_login_client(int epfd, struct epoll_event *evs, int i)
{
    int cmd;
    Regist_p reg = NULL;
    Login_p log = NULL;
    File_trans_p ft = NULL;
    Online_user_p online_user;

    //给接收数据的缓冲器上互斥锁
    pthread_mutex_lock(&lock_recv);

    //初始化数据缓冲区
    bzero(buffer_recv, sizeof(buffer_recv));

    //从套接口中读数据
    int n = read(evs[i].data.fd, buffer_recv, sizeof(buffer_recv));
    if (n > 0)
    {
        //先读取接收缓冲区最开始的cmd，判断客户端发来的是什么命令
        memcpy(&cmd, buffer_recv, sizeof(int));
        printf("cmd = %d\n", cmd);

        switch (cmd)
        {
        //如果客户端发来的是注册请求的话，就执行下面的操作
        case REGIST:
            reg = (Regist_p)malloc(sizeof(Regist));   //给注册结构体分配空间
            memcpy(reg, buffer_recv, sizeof(Regist)); //给注册结构体赋值

            //给接收数据的缓冲区解锁
            pthread_mutex_unlock(&lock_recv); //接收缓冲区使用结束，解锁让其他线程使用提高效率

            regist(evs[i].data.fd, reg); //执行注册操作

            free(reg); //释放之前malloc分配的空间
            break;
        //如果客户端发来的是登录请求的话，就执行下面的操作
        case LOGIN:
            log = (Login_p)malloc(sizeof(Login));    //给登录结构体分配空间
            memcpy(log, buffer_recv, sizeof(Login)); //给登录结构体赋值

            //给接收数据的缓冲区解锁
            pthread_mutex_unlock(&lock_recv); //接收缓冲区使用结束，解锁让其他线程使用提高效率

            printf("id = %d,pwd = %s\n", log->id, log->pwd);
            login(evs[i].data.fd, log); //执行登录操作

            free(log); //释放之前malloc分配的空间
            break;
        case FILETRANS:
            ft = (File_trans_p)malloc(sizeof(File_trans));
            memcpy(ft,buffer_recv,sizeof(File_trans));

            //给接收数据的缓冲区解锁
            pthread_mutex_unlock(&lock_recv); //接收缓冲区使用结束，解锁让其他线程使用提高效率

            online_user = online_search(online[ft->recv_id % HASHI_MAX],ft->recv_id,NULL);
            send(online_user->socketfd,buffer_send,sizeof(File_trans),0);
            free(ft);
            break;
        case FILETRANS_END:
            ft = (File_trans_p)malloc(sizeof(File_trans));
            memcpy(ft,buffer_recv,sizeof(File_trans));

            //给接收数据的缓冲区解锁
            pthread_mutex_unlock(&lock_recv); //接收缓冲区使用结束，解锁让其他线程使用提高效率

            online_user = online_search(online[ft->recv_id % HASHI_MAX],ft->recv_id,NULL);
            send(online_user->socketfd,buffer_send,sizeof(File_trans),0);
            free(ft);
            break;
        default:
            break;
        }
    }
    else if (n == 0) //客户端断开
    {
        //给接收数据的缓冲区解锁
        pthread_mutex_unlock(&lock_recv);

        printf("client <%d> offline!\n", evs[i].data.fd);

        epoll_event_del(epfd, evs[i].data.fd, EPOLLIN); //把和对应的客户端连接的套接口从epoll池中移除
        close(evs[i].data.fd);                          //关闭套接口
        client_fd[i] = -1;                              //给该客户端对应的数组位置置脏值
    }
    return 0;
}

//创建udp套接口：参数为端口号
int start_udpsocket(int port)
{
    int udpfd;
    int ret;
    struct sockaddr_in server_addr;
    int opt = 1;

    //创建udp套接口
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpfd < 0)
    {
        perror("create udpsocket error!");
        exit(-1);
    }

    //初始化服务器ip地址
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //设置允许重复绑定
    setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //绑定
    ret = bind(udpfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0)
    {
        perror("bind error:");
        close(udpfd);
        exit(-1);
    }

    return udpfd;
}

//接收客户端发来的聊天数据并进行处理：参数为epoll池文件描述符，epoll池任务队列与当前任务的队列下标
int recv_udp_client(sqlite3 *pdb, int epfd, struct epoll_event *evs, int i)
{
    int cmd;
    Chat_one_p chat_o;
    Chat_all_p chat_a;
    Udp_con udp_con;
    Udp_con offline;
    Mute mute;
    Remove remove;
    Show_user _show_user;
    struct sockaddr_in client_udp;
    int len;
    Online_user_p user_p;
    int ret;

    //给接收缓冲区上锁
    pthread_mutex_lock(&lock_recv);

    //初始化数据缓冲区
    bzero(buffer_recv, sizeof(buffer_recv));

    //记录客户端地址结构体的长度作参数
    len = sizeof(client_udp);

    //从套接口中读数据
    int n = recvfrom(evs[i].data.fd, buffer_recv, sizeof(buffer_recv), 0,
                     (struct sockaddr *)&client_udp, &len);

    //从缓冲区中取出最前面的cmd，判断客户的发来的是什么请求
    memcpy(&cmd, buffer_recv, sizeof(cmd));
    printf("cmd = %d\n", cmd);

    switch (cmd)
    {
    //如果是私聊请求
    case CHAT_ONE:
        chat_o = (Chat_one_p)malloc(sizeof(Chat_one)); //给私聊结构体分配空间
        memcpy(chat_o, buffer_recv, sizeof(Chat_one)); //给私聊结构体赋值

        pthread_mutex_unlock(&lock_recv); //缓冲区使用结束，解锁

        chat_one(evs[i].data.fd, chat_o); //执行私聊操作

        ret = save_record(pdb, chat_o, NULL, 1);
        if (ret == -1)
        {
            printf("插入聊天记录失败!\n");
        }

        free(chat_o); //释放掉malloc分配的空间
        break;

    //如果是群聊请求
    case CHAT_ALL:
        chat_a = (Chat_all_p)malloc(sizeof(Chat_all)); //给群聊结构体分配空间
        memcpy(chat_a, buffer_recv, sizeof(Chat_all)); //给群聊结构体赋值

        pthread_mutex_unlock(&lock_recv); //缓冲区使用结束，解锁

        //printf("text = %s\n",chat_a->text);

        chat_all(evs[i].data.fd, chat_a); //执行群聊操作

        ret = save_record(pdb, NULL, chat_a, 2);
        if (ret == -1)
        {
            printf("插入聊天记录失败!\n");
        }

        free(chat_a); //释放掉malloc分配的空间
        break;
    /*由于登录注册部分使用的是tcp协议，而群聊私聊部分使用的是udp协议。
            所以在群聊私聊阶段，服务器给客户端回发信息的端口号和登录注册时不同，
            所以这里需要客户端在登录成功后再给服务器发一个udp包，使服务器知道
            该客户端使用的udp端口号是多少便于接下来与客户端通信*/
    case UDP_CONNECT:
        memcpy(&udp_con, buffer_recv, sizeof(udp_con)); //给改结构体赋值

        pthread_mutex_unlock(&lock_recv); //缓冲区使用完，解锁

        //通过id查找哈希表，返回指向对应结点的指针
        user_p = online_search(online[udp_con.id % HASHI_MAX], udp_con.id, NULL);
        //printf("1.%d\n",ntohs(user_p->addr.sin_port));
        // 将该结点中保存的客户端tcp协议的端口号覆写成刚刚新获取的udp端口号
        user_p->addr.sin_port = client_udp.sin_port;
        //printf("2.%d\n",ntohs(user_p->addr.sin_port));
        break;
    case OFFLINE:
        memcpy(&offline, buffer_recv, sizeof(udp_con)); //给改结构体赋值

        pthread_mutex_unlock(&lock_recv); //缓冲区使用完，解锁

        printf("offline id = %d\n", offline.id);

        online_delete(&(online[offline.id % HASHI_MAX]), offline.id);
        //printf("%s is offline!\n", inet_ntoa(client_udp.sin_addr));
        break;
    case SHOW_USER:
        memcpy(&_show_user, buffer_recv, sizeof(Show_user));

        pthread_mutex_unlock(&lock_recv); //缓冲区使用完，解锁
        show_user(evs[i].data.fd, client_udp);
        break;
    case MUTE:
        memcpy(&mute, buffer_recv, sizeof(mute));
        pthread_mutex_unlock(&lock_recv); //缓冲区使用完，解锁

        mute_user(evs[i].data.fd, mute, client_udp);
        break;
    case UNMUTE:
        memcpy(&mute, buffer_recv, sizeof(mute));
        pthread_mutex_unlock(&lock_recv); //缓冲区使用完，解锁

        unmute_user(evs[i].data.fd, mute, client_udp);
        break;
        case REMOVE:
        memcpy(&remove,buffer_recv,sizeof(remove));
        pthread_mutex_unlock(&lock_recv); //缓冲区使用完，解锁

        remove_user(evs[i].data.fd, remove, client_udp);
        break;
    default:
        pthread_mutex_unlock(&lock_recv); //缓冲区使用完，解锁
        break;
    }
}
