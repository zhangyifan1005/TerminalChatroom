#include "threadpool.h"

//创建监听套接口：参数为端口号
int start_listensocket(int port);

//向epoll池中添加事件：参数为epoll池文件描述符，套接口文件描述符，ctl事件
int epoll_event_add(int epfd,int fd,int event);

//向epoll池中删除事件：参数为epool池文件描述符，套接口文件描述符，ctl事件
int epoll_event_del(int epfd,int fd,int event);

//接受监听套接口中的信息：参数为监听套接口文件描述符与epoll池文件描述符
int do_accept(int listenfd,int epfd);

//创建udp套接口：参数为端口号
int start_udpsocket(int port);
