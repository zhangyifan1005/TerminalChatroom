#include "chat.h"

//和服务器同步聊天记录
void record_update(int sockfd, char *filename)
{
    Record record;
    Record_file rf;
    char buffer_record[200];
    int ret;
    int write_num;

    while (1)
    {
        ret = recv(sockfd, buffer_record, sizeof(buffer_record), 0);
        if (ret < 0)
        {
            printf("recv error!\n");
            break;
        }
        else if (ret == 0)
        {
            printf("server offline!\n");
            exit(0);
        }
        else
        {
            memcpy(&record, buffer_record, sizeof(record));
            //printf("cmd = %d\n", record.cmd);
            switch (record.cmd)
            {
            case RECORD_ONE:
                rf.send_id = record.send_id;
                strcpy(rf.text, record.text);
                write_num = write(fp_one, &rf, sizeof(rf));
                // printf("text = %s\n", record.text);
                break;
            case RECORD_ALL:
                rf.send_id = record.send_id;
                strcpy(rf.text, record.text);
                write_num = write(fp_all, &rf, sizeof(rf));
                //printf("text = %s\n", record.text);
                break;
            case RECORD_END:
                return;
                break;
            default:
                //printf("bad recv!\n");
                break;
            }
        }
    }
}

//进行聊天操作
void user_chat(int sockfd,int udpfd, struct sockaddr_in server_addr, char *name)
{
    int select = 0;
    //创建一个读线程
    pthread_t tid;
    Tmp_read tmp;
    tmp.udpfd = udpfd;
    tmp.server_addr = server_addr;
    //该线程专门用来接收服务器发来的数据包，实现读写分离
    pthread_create(&tid, NULL, (void *)&chat_listen, (void *)&tmp);

    Chat_one chat_o;
    Chat_all chat_a;
    Udp_con offline;
    Record_file rf;
    int cmd;
    int recv_data, data_len;
    char record_data[200];
    char *p = NULL;

    while (1)
    {
        chat_menu(); //打印群聊菜单
        scanf("%d", &select);
        getchar();
        if (select == 1) //查询在线用户状态
        {
            cmd = SHOW_USER;
            sendto(udpfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)); //发数据包给服务器
        }
        if (select == 2) //群聊
        {
            if (is_mute == 0)
            {
                printf("请发言！\n");
                fgets(chat_a.text, 100, stdin); //输入聊天内容
                chat_a.cmd = CHAT_ALL;
                chat_a.user_send = user_id; //给发信人赋值
                strcpy(chat_a.user_name_send, name);
                sendto(udpfd, &chat_a, sizeof(chat_a), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)); //发数据包给服务器
                lseek(fp_all, 0, SEEK_END);
                rf.send_id = user_id;
                strcpy(rf.text, chat_a.text);
                write(fp_all, &rf, sizeof(rf));
            }
            else
            {
                printf("你已经被禁言了！\n");
            }
        }
        else if (select == 3)
        {
            if (is_mute == 0)
            {
                printf("请输入你想给谁发消息\n");
                scanf("%d", &chat_o.user_recv); //给收信人赋值
                getchar();
                printf("请发言！\n");
                fgets(chat_o.text, 100, stdin);
                chat_o.cmd = CHAT_ONE;
                chat_o.user_send = user_id; //给发信人赋值
                strcpy(chat_o.user_name_send, name);
                sendto(udpfd, &chat_o, sizeof(chat_o), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)); //发数据包给服务器
                printf("%s 说道:\n", name);
                //fputs(chat_o.text, stdout);
                emoji_manage(chat_o.text);
                lseek(fp_one, 0, SEEK_END);
                rf.send_id = user_id;
                strcpy(rf.text, chat_o.text);
                write(fp_one, &rf, sizeof(rf));
            }
            else
            {
                printf("你已经被禁言了！\n");
            }
        }
        else if (select == 4)
        {
            lseek(fp_one, 0, SEEK_SET);
            data_len = sizeof(rf);
            p = record_data;
            while (recv_data = read(fp_one, record_data, data_len))
            {
                if (recv_data == -1)
                {
                    perror("read error!");
                    break;
                }
                else
                {
                    p += recv_data;
                    data_len = data_len - recv_data;
                    if (data_len == 0)
                    {
                        memcpy(&rf, record_data, sizeof(rf));
                        printf("%d说了：\n", rf.send_id);
                        printf("%s\n", rf.text);
                        p = record_data;
                        data_len = sizeof(rf);
                    }
                }
            }
        }
        else if (select == 5)
        {
            lseek(fp_all, 0, SEEK_SET);
            data_len = sizeof(rf);
            p = record_data;
            while (recv_data = read(fp_all, record_data, data_len))
            {
                if (recv_data == -1)
                {
                    printf("read error!\n");
                    break;
                }
                else
                {
                    p += recv_data;
                    data_len -= recv_data;
                    if (data_len == 0)
                    {
                        memcpy(&rf, record_data, sizeof(rf));
                        printf("%d向大家发送了消息：\n", rf.send_id);
                        printf("%s\n", rf.text);
                        p = record_data;
                        data_len = sizeof(rf);
                    }
                }
            }
        }
        else if (select == 6) 
        {
            file_trans(sockfd,name);
        }
        else if (select == 7) //客户端下线
        {
            offline.cmd = OFFLINE;
            offline.id = user_id;
            sendto(udpfd, &offline, sizeof(offline), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)); //发数据包给服务器
            break;
        }
        else
        {
            //printf("非法输入！\n");
        }
    }
}

//进行管理员操作
void admin_chat(int sockfd,int udpfd, struct sockaddr_in server_addr, char *name, int authority)
{
    int select = 0;
    //创建一个读线程
    pthread_t tid;
    Tmp_read tmp;
    tmp.udpfd = udpfd;
    tmp.server_addr = server_addr;
    //该线程专门用来接收服务器发来的数据包，实现读写分离
    pthread_create(&tid, NULL, (void *)&chat_listen, (void *)&tmp);

    Chat_one chat_o;
    Chat_all chat_a;
    Udp_con offline;
    Record_file rf;
    Mute mute;
    Remove remove;
    int cmd;
    int recv_data, data_len;
    char record_data[200];
    char *p = NULL;

    while (1)
    {
        admin_menu(); //打印群聊菜单
        scanf("%d", &select);
        getchar();
        if (select == 1) //查询在线用户状态
        {
            cmd = SHOW_USER;
            sendto(udpfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)); //发数据包给服务器
        }
        if (select == 2) //群聊
        {
            if (is_mute == 0)
            {
                printf("请发言！\n");
                fgets(chat_a.text, 100, stdin); //输入聊天内容
                chat_a.cmd = CHAT_ALL;
                chat_a.user_send = user_id; //给发信人赋值
                strcpy(chat_a.user_name_send, name);
                sendto(udpfd, &chat_a, sizeof(chat_a), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)); //发数据包给服务器
                lseek(fp_all, 0, SEEK_END);
                rf.send_id = user_id;
                strcpy(rf.text, chat_a.text);
                write(fp_all, &rf, sizeof(rf));
            }
            else
            {
                printf("你已被禁言，无法发送消息！\n");
            }
        }
        else if (select == 3)
        {
            if (is_mute == 0)
            {
                printf("请输入你想给谁发消息\n");
                scanf("%d", &chat_o.user_recv); //给收信人赋值
                getchar();
                printf("请发言！\n");
                fgets(chat_o.text, 100, stdin);
                chat_o.cmd = CHAT_ONE;
                chat_o.user_send = user_id; //给发信人赋值
                strcpy(chat_o.user_name_send, name);
                sendto(udpfd, &chat_o, sizeof(chat_o), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)); //发数据包给服务器
                printf("%s 说道:\n", name);
                //fputs(chat_o.text, stdout);
                emoji_manage(chat_o.text);
                lseek(fp_one, 0, SEEK_END);
                rf.send_id = user_id;
                strcpy(rf.text, chat_o.text);
                write(fp_one, &rf, sizeof(rf));
            }
            else
            {
                printf("你已被禁言，无法发送消息！\n");
            }
        }
        else if (select == 4)
        {
            lseek(fp_one, 0, SEEK_SET);
            data_len = sizeof(rf);
            p = record_data;
            while (recv_data = read(fp_one, record_data, data_len))
            {
                if (recv_data == -1)
                {
                    perror("read error!");
                    break;
                }
                else
                {
                    p += recv_data;
                    data_len = data_len - recv_data;
                    if (data_len == 0)
                    {
                        memcpy(&rf, record_data, sizeof(rf));
                        printf("%d说了：\n", rf.send_id);
                        printf("%s\n", rf.text);
                        p = record_data;
                        data_len = sizeof(rf);
                    }
                }
            }
        }
        else if (select == 5)
        {
            lseek(fp_all, 0, SEEK_SET);
            data_len = sizeof(rf);
            p = record_data;
            while (recv_data = read(fp_all, record_data, data_len))
            {
                if (recv_data == -1)
                {
                    printf("read error!\n");
                    break;
                }
                else
                {
                    p += recv_data;
                    data_len -= recv_data;
                    if (data_len == 0)
                    {
                        memcpy(&rf, record_data, sizeof(rf));
                        printf("%d向大家发送了消息：\n", rf.send_id);
                        printf("%s\n", rf.text);
                        p = record_data;
                        data_len = sizeof(rf);
                    }
                }
            }
        }
        else if (select == 6) //禁言
        {
            printf("你想禁言的用户id\n");
            scanf("%d", &mute.mute_id);
            mute.send_authority = authority;
            //printf("mute_send_authority = %d\n", mute.send_authority);
            mute.cmd = MUTE;
            sendto(udpfd, &mute, sizeof(mute), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)); //发数据包给服务器
        }
        else if (select == 7) //解除禁言
        {
            printf("你想解除禁言的用户id\n");
            scanf("%d", &mute.mute_id);
            mute.send_authority = authority;
            mute.cmd = UNMUTE;
            sendto(udpfd, &mute, sizeof(mute), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)); //发数据包给服务器
        }
        else if (select == 8) //踢人
        {
            printf("你想移除的用户！\n");
            scanf("%d", &remove.id);
            remove.send_authority = authority;
            remove.cmd = REMOVE;
            strcpy(remove.name, name);
            sendto(udpfd, &remove, sizeof(remove), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)); //发数据包给服务器
        }
        else if(select == 9)
        {
            file_trans(sockfd,name);
        }
        else if (select == 10) //客户端下线
        {
            offline.cmd = OFFLINE;
            offline.id = user_id;
            sendto(udpfd, &offline, sizeof(offline), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)); //发数据包给服务器
            break;
        }
        else
        {
            printf("非法输入！\n");
        }
    }
}

//监听线程的回调函数
void *chat_listen(void *arg)
{
    Tmp_read tmp = *((Tmp_read *)arg);
    char buffer_listen[1024];
    int ret, cmd;
    Chat_one chat_o;
    Chat_all chat_a;
    Show_user show_user;
    Record_file rf;
    Mute mute;
    Remove remove;
    int len;
    int file_fd;
    File_trans ft;
    int file_flag = 0;
    char route[30];

    //初始化接收缓冲区
    memset(buffer_listen, 0, sizeof(buffer_listen));

    //printf("server addr <%s:%d>\n",inet_ntoa(tmp.server_addr.sin_addr),ntohs(tmp.server_addr.sin_port));
    //printf("fd = %d\n",tmp.udpfd);

    //给服务器地址长度赋值
    len = sizeof(tmp.server_addr);

    while (1)
    {
        //始终接收服务器发来的数据
        ret = recvfrom(tmp.udpfd, buffer_listen, sizeof(buffer_listen), 0,
                       (struct sockaddr *)&(tmp.server_addr), &len);

        //printf("buffer = %s\n",buffer_listen);
        if (ret < 0) //错误处理
        {
            printf("recv error!\n");
            exit(-1);
        }
        else if (ret == 0)
        {
            printf("server offline!\n");
            exit(-1);
        }
        else //成功接收到服务器发来的消息
        {
            memcpy(&cmd, buffer_listen, sizeof(cmd)); //查看cmd
            switch (cmd)
            {
            case CHAT_ONE: //私聊
                //接收并打印出群聊信息
                memcpy(&chat_o, buffer_listen, sizeof(Chat_one));
                printf("from %s:\n", chat_o.user_name_send);
                //fputs(chat_o.text, stdout);
                emoji_manage(chat_o.text);
                lseek(fp_one, 0, SEEK_END);
                rf.send_id = chat_o.user_send;
                strcpy(rf.text, chat_o.text);
                write(fp_one, &rf, sizeof(rf));
                break;
            case CHAT_ALL: //群聊
                //接收并打印出私聊信息
                memcpy(&chat_a, buffer_listen, sizeof(Chat_all));
                printf("群聊！from %s\n", chat_a.user_name_send);
                //fputs(chat_a.text, stdout);
                emoji_manage(chat_a.text);
                lseek(fp_all, 0, SEEK_END);
                rf.send_id = chat_o.user_send;
                strcpy(rf.text, chat_a.text);
                write(fp_all, &rf, sizeof(rf));
                break;
            case ONLINE_USER:
                memcpy(&show_user, buffer_listen, sizeof(show_user));
                printf("用户id为： %5d,用户名为：%s\n", show_user.id, show_user.name);
                break;
            case MUTE_OK:
                is_mute = 1;
                printf("你被禁言了！\n");
                break;
            case UNMUTE_OK:
                is_mute = 0;
                printf("你被解除禁言！\n");
                break;
            case MUTE_FAIL:
                printf("禁言失败！ 权限不足或该用户已离线！\n");
                break;
            case UNMUTE_FAIL:
                printf("解除禁言失败！ 权限不足或该用户已离线！\n");
                break;
            case REMOVE_OK:
                memcpy(&remove, buffer_listen, sizeof(remove));
                printf("你被踢出聊天室！\n");
                exit(1);
                break;
            case REMOVE_FAIL:
                printf("踢人失败！ 权限不足或该用户已离线！\n");
                break;
            case FILETRANS:
                memcpy(&ft,buffer_listen,sizeof(ft));
                if(file_flag == 0)
                {
                    printf("%s向你发送了一个文件%s\n",ft.send_name,ft.file_name);
                    strcpy(route,"./file/");
                    strcat(route,ft.file_name);
                    file_fd = open(route,O_CREAT | O_WRONLY,777);
                    file_flag = 1;
                }
                write(file_fd,ft.send_buffer,1024);
                break;
            case FILETRANS_END:
                memcpy(&ft,buffer_listen,sizeof(ft));
                if(file_flag == 0)
                {
                    printf("%s向你发送了一个文件%s\n",ft.send_name,ft.file_name);
                }
                write(file_fd,ft.send_buffer,ft.len);
                file_flag = 0;
                break;
            default:
                printf("6\n");
                break;
            }
        }
    }
}

//打印聊天页面
void chat_menu()
{
    /*
    printf("请选择你的操作！\n");
    printf("1.查询在线用户\n");
    printf("2.群聊\n");
    printf("3.私聊\n");
    printf("4.显示私聊聊天记录\n");
    printf("5.显示群聊聊天记录\n");
    printf("6.下线\n");*/

    printf("\033[34m                               \033[0;36m<1.查询在线用户>\033[0;34m            \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;36m<2.群聊>\033[0;34m           \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;36m<3.私聊>\033[0;34m       \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;36m<4.显示私聊聊天记录>\033[0;34m           \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;36m<5.显示群聊聊天记录>\033[0;34m    \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;36m<6.文件传输>\033[0;34m    \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;31m<7.下线>\033[0;34m    \033[0m\n");
}

//打印管理员页面
void admin_menu()
{
    /*
    printf("请选择你的操作！\n");
    printf("1.查询在线用户\n");
    printf("2.群聊\n");
    printf("3.私聊\n");
    printf("4.显示私聊聊天记录\n");
    printf("5.显示群聊聊天记录\n");
    printf("6.禁言用户\n");
    printf("7.解除禁言\n");
    printf("8.将用户踢出群聊\n");
    printf("9.下线\n");*/

    printf("\033[34m                               \033[0;36m<1.查询在线用户>\033[0;34m            \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;36m<2.群聊>\033[0;34m           \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;36m<3.私聊>\033[0;34m       \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;36m<4.显示私聊聊天记录>\033[0;34m           \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;36m<5.显示群聊聊天记录>\033[0;34m    \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;33m<6.禁言用户>\033[0;34m    \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;33m<7.解除禁言>\033[0;34m            \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;33m<8.将用户踢出群聊>\033[0;34m           \033[0m\n");
    putchar(10);
     printf("\033[34m                               \033[0;33m<9.文件传输>\033[0;34m           \033[0m\n");
    putchar(10);
    printf("\033[34m                               \033[0;31m<10.下线>\033[0;34m       \033[0m\n");
}

//表情包管理
void emoji_manage(char *text)
{
    int ret;

    if (*text == '/') //如果是表情包
    {
        if (!(strncmp(text + 1, "微笑", 2)))
        {
            printf("😃\n");
        }
        else if (!(strncmp(text + 1, "流汗", 2)))
        {
            printf("😅\n");
        }
        else if (!(strncmp(text + 1, "乐", 2)))
        {
            printf("🤣\n");
        }
        else
        {
            fputs(text, stdout);
        }
    }
    else
    {
        fputs(text, stdout);
    }
}

void progress_bar() //进度条
{

    // ****************************** 配置 ***************************
    // 最后100%时的输出形式
    const char *LastStr = "[--------------------] 100%";

    // 进度条标志，可以改用"*"或其它符号
    const char Progressicon = '-';

    // 进度每加5，进度条就会加一格，注意Step值要和LastStr中的"-"数量相匹配，两者相乘等于100
    const int Step = 5;

    // 总共需要多少进度标志，即LastStr中"-"的数量
    const int iconMaxNum = 20;

    // 每隔100ms打印一次，这里同时每隔200ms会让进度加1
    const int Printinterval = 20000;
    // ****************************************************************

    for (int i = 0; i <= 100; ++i)
    {
        // -------------- 打印进度条 --------------
        printf("\033[0;34m[\033[0m");
        int currentindex = i / Step;
        for (int j = 0; j < iconMaxNum; ++j)
        {
            if (j < currentindex)
            {
                printf("\033[0;34m%c\033[0m", Progressicon); // 打印进度条标志
            }
            else
            {
                printf(" "); // 未达进度则打印空格
            }
        }

        printf("\033[0;34m]\033[0m ");
        // ----------------------------------------

        // -------- 打印数字进度 ----------
        printf("\033[0;34m%3d%%\033[0m", i);
        fflush(stdout);
        // -------------------------------

        usleep(Printinterval);

        for (int j = 0; j < strlen(LastStr); ++j)
        {
            printf("\b"); // 回删字符，让数字和进度条原地变化
        }
        fflush(stdout);
    }
    printf("\n");
}

//文件传输
void file_trans(int sockfd, char * name)
{
    int trans_fd;
    int bytes_read, file_len, data_len;
    char file_route[100];
    File_trans ft;
    char *p = NULL;
    int i = 0;

    strcpy(ft.send_name,name);

    printf("请输入你想发送的对象！\n");
    scanf("%d",&ft.recv_id);

    printf("请输入你想发送文件的路径！\n");
    scanf("%s", file_route);

    trans_fd = open(file_route, O_RDONLY, 0);
    printf("trans fd = %d\n",trans_fd);
    if (trans_fd < 0)
    {
        printf("文件打开失败或路径不存在！\n");
    }

    p = strstr(file_route,"\\");
    if(p == NULL)
    {
        strcpy(ft.file_name,file_route);
    }
    else
    {
        strcpy(ft.file_name,p);
    }

    file_len = lseek(trans_fd, 0, SEEK_END);
    lseek(trans_fd, 0, SEEK_SET);

    p = ft.send_buffer;

    printf("size = %d\n",sizeof(ft.send_buffer));

    while (bytes_read = read(trans_fd, p, data_len))
    {
        if (bytes_read == -1)
        {
            perror("read error!");
            break;
        }
        else
        {
            p += bytes_read;
            data_len -= bytes_read;
            if (data_len == 0)
            {
                ft.cmd = FILETRANS;
                ft.len = 1024;
                send(sockfd, &ft, sizeof(ft), 0);
                printf("send ok!\n");
                data_len == 1024;
                p = ft.send_buffer;
                i++;
            }
        }
    }

    ft.cmd = FILETRANS_END;
    ft.len = 1024 - data_len;
    send(sockfd, &ft, sizeof(ft), 0);
    printf("send end!\n");
}