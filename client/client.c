#include "login.h"
#include "chat.h"

int main(int argc, char **argv)
{
    int sockfd, udpfd;
    struct sockaddr_in server_addr, server_addr2;
    int ret, num;
    char name[10];
    Udp_con udp_con;
    char file_name[20];
    char filename[10];

    //判断主函数参数是否正确
    if (argc != 2)
    {
        printf("参数个数错误！\n");
        exit(0);
    }

    is_mute = 0;//给禁言标志位赋初值

    //tcp网络连接的一系列操作
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("connect socket error:");
        exit(-1);
    }


    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_TCP_POT);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    if (server_addr.sin_addr.s_addr == -1)
    {
        perror("输入ip地址格式错误");
        close(sockfd);
        exit(-1);
    }

    ret = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret == -1)
    {
        perror("connect error:");
        close(sockfd);
        exit(-1);
    }

    //建立udp套接口
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("udp socket error:");
        exit(-1);
    }

    //printf("udp socket = %d\n", udpfd);

    bzero(&server_addr2, sizeof(server_addr2));
    server_addr2.sin_family = AF_INET;
    server_addr2.sin_port = htons(SERVER_UDP_POT);
    server_addr2.sin_addr.s_addr = inet_addr(argv[1]);

    if (server_addr2.sin_addr.s_addr == -1)
    {
        perror("输入ip地址格式错误");
        close(sockfd);
        exit(-1);
    }

    ret = client_login(sockfd, name); //调用登录/注册的函数

    sprintf(filename,"%d",user_id);
    strncpy(file_name, filename, strlen(filename));
    strcpy(file_name + strlen(filename), "_one.txt");
    fp_one = open(file_name, O_CREAT | O_RDWR, 777);
    if(fp_one == -1)
    {
        //perror("fp_one ");
    }
    //printf("fp_one = %d\n",fp_one);

    strncpy(file_name, filename, strlen(filename));
    strcpy(file_name + strlen(filename), "_all.txt");
    fp_all = open(file_name, O_CREAT | O_RDWR, 777);
   // printf("fp_all = %d\n",fp_all);
    if(fp_all == -1)
    {
        perror("fp_all ");
    }

    record_update(sockfd,filename);

    //登录注册使用的是tcp连接，接下来的群聊私聊使用的是udp连接，所以要单独给服务器发一个包来告之客户端udp的端口号
    udp_con.cmd = UDP_CONNECT;
    udp_con.id = user_id;
    num = sendto(udpfd, &udp_con, sizeof(udp_con), 0, (struct sockaddr *)&server_addr2, sizeof(server_addr2));
    //printf("cmd = %d,id = %d, num = %d\n",udp_con.cmd,udp_con.id,num);

    progress_bar(); //进度条

    //对服务器返回的权限做判断，可以拥有不同的权限菜单
    if (ret == 0)
    {
        printf("欢迎你 群主！ %s\n", name);
        admin_chat(sockfd,udpfd, server_addr2, name,ret);
    }
    else if (ret == 1)
    {
        printf("欢迎你 管理员！ %s\n", name);
        admin_chat(sockfd,udpfd, server_addr2, name,ret);
    }
    else if (ret == 2) //目前只写了普通用户的群聊私聊功能
    {
        printf("欢迎你！%s\n", name);
        user_chat(sockfd,udpfd, server_addr2, name);
    }

    return 0;
}
