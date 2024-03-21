#include "mainfun.h"

int main()
{
    int listenfd, confd, epfd,udpfd;
    int ret;
    int i, n;
    int flag = 0;
    struct sockaddr_in server_addr,server_addr2; 

    pdb = NULL;
    
    //打开数据库data.db
    ret = sqlite3_open("./data.db", &pdb);
    if (SQLITE_OK != ret)
    {
        printf("open error %s\n", sqlite3_errmsg(pdb));
        exit(-1);
    }
 
    //创建用户表
    ret = user_create(pdb);
    if (SQLITE_OK != ret)
    {
        exit(-1);
    }

    //创建聊天记录表
    ret = record_create(pdb);
    if (SQLITE_OK != ret)
    {
        exit(-1);
    }

    //创建线程池
    threadpool_t *pool = threadpool_create(3, 50, 100);

    //创建udp连接使用的套接口，并绑定到服务器
    udpfd = start_udpsocket(SERVER_UDP_POT);

    //创建监听套接口，绑定到服务器并开启监听
    listenfd = start_listensocket(SERVER_TCP_POT);

    //创建epoll池
    epfd = epoll_create(MAX_EVENT_NUMS);
    if (epfd < 0)
    {
        perror("epoll");
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    //将listenfd对应的套接口加入到epoll池监听队列中
    epoll_event_add(epfd, listenfd, EPOLLIN);  //listen套接口设置为默认的触发方式

    //将udpfd对应的套接口加入到epoll池监听队列中
    epoll_event_add(epfd, udpfd, EPOLLIN | EPOLLET);  //udp套接口设置为边缘触发，防止epoll多次响应

    //printf("udpfd = %d\n",udpfd);

    //创建一个epoll事件队列
    struct epoll_event evs[MAX_EVENT_NUMS];

    //初始化client_fd数组
    memset(client_fd, -1, sizeof(client_fd));

    //初始化client_addr_in数组
    memset(client_addr_in, 0, sizeof(client_addr_in));

    //初始化哈希表
    for(int i = 0;i < HASHI_MAX;i++)
    {
        online[i] = NULL;
    }

    // 初始化互斥锁
    if (pthread_mutex_init(&(lock_online), NULL) != 0 ||  //lock_online是用来锁住所有对哈希表的增加与删除操作的
        pthread_mutex_init(&(lock_recv), NULL) != 0 ||       //lock_recv是用来锁住对接收缓冲区的读写操作的
        pthread_mutex_init(&(lock_send), NULL) != 0)       //暂时还没用上，先放在这里
    {
        printf("init lock false;\n");
        exit(-1);
    }

    do
    {
        //使用epoll_wait进行阻塞（第三个参数代表阻塞时间，大于0为秒数，等于0为非阻塞状态，等于-1为阻塞状态）
        //epoll_wait的返回值：成功返回需要处理的事件数目。失败返回0，表示等待超时。
        n = epoll_wait(epfd, evs, MAX_EVENT_NUMS, -1);
        if (n == 0) //n=0代表epoll_wait超时
        {
            printf("epoll_wait timeout\n");
            continue;
        }
        else if (n < 0) //n<0代表epoll_wait出错
        {
            perror("epoll_wait");
            if (errno == EAGAIN || errno == EINTR) //对错误码进行判断，这种情况说明是非致命情况，重新进行epoll_wait即可
            {
                continue;
            }
            else //其他情况出错，退出循环
            {
                break;
            }
        }
        else //n>0代表epoll_wait成功，n为需要处理的事件的个数
        {
            //遍历epoll池中的evs队列，查看需要响应的套接口
            for (i = 0; i < n; i++)
            {
                if (evs[i].events & EPOLLIN) //判断该数据是否为接受数据的套接口，若是则可以操作
                {
                    if (evs[i].data.fd == listenfd) //判断该套接口是否为监听套接口
                    {
                        do_accept(listenfd, epfd); //若是监听套接口，说明有新的套接口需要accept
                        printf("accept complet!\n");
                    }
                    else  if (evs[i].data.fd == udpfd) //判断该套接口是否为udp套接口
                    {
                        //定义一个给线程池回调函数传参的结构体，并给结构体赋值
                        Tmp tmp;  
                        tmp.epfd = epfd;
                        tmp.evs = evs;
                        tmp.i = i;

                       //recv_udp_client(pdb,epfd,evs,i);//若是udp套接口，说明需要处理群聊私聊等问题
                       threadpool_add_task(pool, (void *)task_func2, (void *)&tmp); //把任务交给线程池回调函数来实现
                    }
                    else //该套接口为连接套接口，说明服务器需要接收数据并进行操作
                    {
                        //定义一个给线程池回调函数传参的结构体，并给结构体赋值
                        Tmp tmp;  
                        tmp.epfd = epfd;
                        tmp.evs = evs;
                        tmp.i = i;

                        threadpool_add_task(pool, (void *)task_func, (void *)&tmp); //把任务交给线程池回调函数来实现
                        /*
                        ret = recv_client(epfd,evs,i);//接受数据并做处理
                        if(ret < 0)
                        {
                            flag = 1;
                            break;
                        }*/
                    }
                }
            }
            if (flag == 1) //出错处理，关闭服务器
            {
                break;
            }
        }

    } while (1);

    for (i = 0; i < MAX_EVENT_NUMS; ++i) //关闭所有的文件描述符
    {
        if (client_fd[i] >= 0)
        {
            printf("Close sockfd <%d>\n", i);
            close(i);
        }
    }
    close(listenfd); //client_fd中并不包含listenfd
    close(udpfd);

    threadpool_destroy(pool); //销毁线程池
    sqlite3_close(pdb);

    return 0;
}