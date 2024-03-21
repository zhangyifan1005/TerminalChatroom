#include "login.h"

//用户的登录注册
int client_login(int sockfd,char * name)
{
    int ret;
    Regist reg;
    Login log;
    char buffer[1024];
    int cmd;
    int select = 0;

    //注册成功后继续选择注册或登录
    //登录失败也是继续选择注册或登录
    //登录成功后跳出该循环
    while (1)
    {
        /*
        printf("请选择你的操作！\n");
        printf("1.注册新账号\n");
        printf("2.用户登录\n");*/

        usleep(500 * 1000);
    system("clear");
    printf("\033[0;46m*===================================================================*\033[0m\n");
    printf("\033[0;46m|                                                                   |\033[0m\n");
    printf("\033[0;46m|*********************\033[0;33m欢迎来到zyf聊天室\033[0;46m************************|\033[0m\n");
    printf("\033[0;46m|                                                版本:epoll+线程池  |\033[0m\n");
    printf("\033[0;46m|-------------------------------------------------------------------|\033[0m\n");
    printf("\033[0;46m|                                                                   |\033[0m\n");
    printf("\033[0;46m|                         \033[0;35m1.帐号注册\033[0;46m                              |\033[0m\n");
    printf("\033[0;46m|                                                                   |\033[0m\n");
    printf("\033[0;46m|                         \033[0;32m2.用户登录\033[0;46m                              |\033[0m\n");
    printf("\033[0;46m|                                                                   |\033[0m\n");
    printf("\033[0;46m*===================================================================*\033[0m\n");


        scanf("%d", &select);
        if (select == 1)
        {
            printf("请输入用户名！\n");
            scanf("%s", reg.name);
            printf("请输入密码！\n");
            scanf("%s", reg.pwd);
            reg.cmd = REGIST;
            send(sockfd, &reg, sizeof(reg), 0);  

            recv(sockfd, buffer, sizeof(buffer), 0);

            memcpy(&cmd, buffer, sizeof(cmd));  //对服务器回发的操作码判断，看是否注册成功
            if (cmd == REGIST_OK)
            {
                Regist_OK ok;
                printf("注册成功！\n");
                memcpy(&ok, buffer, sizeof(ok));
                printf("你的新账号为：%d\n", ok.id);  //打印服务器随机生成的账号
            }
            else
            {
                printf("注册失败！\n");
            }
        }
        else if (select == 2)
        {
             printf("请输入账号！\n");
            scanf("%d", &log.id);
            printf("请输入密码！\n");
            scanf("%s", log.pwd);
            log.cmd = LOGIN;
           // printf("id = %d,pwd = %s\n",log.id,log.pwd);
            send(sockfd, &log, sizeof(log), 0);

            recv(sockfd,buffer,sizeof(buffer),0);
            memcpy(&cmd,buffer,sizeof(cmd));  //对服务器回发的操作码判断，看是否登录成功
            if(cmd == LOGIN_FAIL_IDERROR)
            {
                printf("账号不存在！\n");
            }
            else if(cmd == LOGIN_FAIL_PWDERROR)
            {
                printf("密码错误！\n");
            }
            else if (cmd == LOGIN_OK)
            {
                Login_OK ok;

               // printf("登陆成功！\n");
                memcpy(&ok,buffer,sizeof(ok));
               // printf("hello %s\n",ok.name);
                strcpy(name,ok.name); //通过输出参数返回用户登录的名字
                user_id = log.id;
                return ok.authority;
            }
            else
            {
                printf("error cmd = %d\n",cmd);
            }
        }
        else
        {
            printf("非法输入！\n");
        }
    }
}