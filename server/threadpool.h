#include"operate.h"

#define DEFAULT_TIME 1
#define DEFAULT_THREAD_NUM 10
#define MIN_WAIT_TASK_NUM 10

//用来存放任务的结构体，成员为函数指针与任务函数的参数
typedef struct
{
   void *(*function)(void *);   
   void *arg;
} threadpool_task_t;

//线程池管理
typedef struct 
{
   pthread_mutex_t lock;                 // 锁住整个结构体 
   pthread_mutex_t thread_counter;       // 用于使用忙线程数时的锁 
   pthread_cond_t  queue_not_full;       // 条件变量，任务队列不为满 
   pthread_cond_t  queue_not_empty;      // 任务队列不为空 

   pthread_t *threads;                   // 存放线程的tid,实际上就是管理了线程数组 
   pthread_t admin_tid;                  // 管理者线程tid 
   threadpool_task_t *task_queue;        // 任务队列 

   //线程池信息
   int min_thr_num;                      // 线程池中最小线程数 
   int max_thr_num;                      // 线程池中最大线程数 
   int live_thr_num;                     // 线程池中存活的线程数 
   int busy_thr_num;                     // 忙线程，正在工作的线程 
   int wait_exit_thr_num;                // 需要销毁的线程数 

   //任务队列信息
   int queue_front;                      // 队头 
   int queue_rear;                       // 队尾 
   int queue_size; 

   //存在的任务数 
   int queue_max_size;                   // 队列能容纳的最大任务数 
   //线程池状态
   int shutdown;                         // true为关闭 
}threadpool_t;

//回调函数传参使用的结构体
typedef struct 
{
   int epfd;
   struct epoll_event*evs;
   int i;
}Tmp;

//创建线程池
threadpool_t * threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size);

//向线程池的任务队列中添加一个任务
int threadpool_add_task(threadpool_t *pool, void *(*function)(void *arg), void *arg);

//管理者线程
void *admin_thread(void *threadpool);

//工作线程
void * threadpool_thread(void *threadpool);

//线程是否存活
int is_thread_alive(pthread_t tid);

//释放线程池
int threadpool_free(threadpool_t *pool);

//销毁线程池
int threadpool_destroy(threadpool_t *pool);

//接收客户端发来的登录数据并进行处理：参数为epoll池文件描述符，epoll池任务队列与当前任务的队列下标
//本函数的函数体在mainfuc.c中
int recv_login_client(int epfd,struct epoll_event*evs,int i);

//接收客户端发来的聊天数据并进行处理：参数为epoll池文件描述符，epoll池任务队列与当前任务的队列下标
//本函数的函数体在mainfuc.c中
int recv_udp_client(sqlite3 * pdb,int epfd,struct epoll_event*evs,int i);

//线程池回调函数 tcp
void task_func(void * arg);

//线程池回调函数 udp
void task_func2(void * arg);



