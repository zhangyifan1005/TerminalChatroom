#include "threadpool.h"

/*线程池工作原理：
    首先，在主函数中创建一个线程池，主函数所在的线程接下来我会称之为主线程。
    在创建线程池时，主线程会创建若干个工作线程与一个管理者线程。
    在线程池中，存在一个任务队列，队列中存放着一个结构体，结构体的成员变量为一个
    函数指针与该函数指针指向的函数的参数。
    主线程负责向任务队列中存放我们需要线程池帮我们处理的任务，
    工作线程按序从任务队列中取出任务并执行，
    管理者线程通过监控 现在正在处理任务的工作线程数， 工作线程的总数 以及 任务队列的长度
    来判断是否需要增加或者减少工作线程的数量（基于某种我们设定的规则）。
    由主线程调用 threadpool_add_task 函数，用来给线程池的任务队列中添加任务，并给工作线程
    发信号，告之工作线程可以去任务队列中取出任务执行了。
    工作线程在没有任务可处理的时候阻塞，在主线程王队列中放任务时被唤醒，于是从任务队列中取出任务，
    并且发通知告之主线程可以继续往任务队列中加任务了（经典生产者消费者问题）。
    然后工作线程通过任务队列中的函数指针与参数执行任务。
    管理者线程上面已经介绍过了，这里就不再赘述。
    */

//创建线程池
threadpool_t * threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size)
{                                                                       //  最小线程数                最大线程数                最大任务数
   int i;
   threadpool_t *pool = NULL;
   do
   {
      //线程池空间开辟 
      if ((pool=(threadpool_t *)malloc(sizeof(threadpool_t))) == NULL)
      {
        printf("malloc threadpool false; \n");
        break;   
      }
      //信息初始化
      pool->min_thr_num = min_thr_num;
      pool->max_thr_num = max_thr_num;
      pool->busy_thr_num = 0;
      pool->live_thr_num = min_thr_num;
      pool->wait_exit_thr_num = 0;
      pool->queue_front = 0;
      pool->queue_rear = 0;
      pool->queue_size = 0;
      pool->queue_max_size = queue_max_size;
      pool->shutdown = false;

    //根据最大线程数，给工作线程数组开空间，清0 
    //因为线程池结构体内只给threads指针分配了空间，所以这里要给数组分配空间
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t)*max_thr_num);
    if (pool->threads == NULL)
    {
       printf("malloc threads false;\n");
       break;
    }
    memset(pool->threads, 0, sizeof(pthread_t)*max_thr_num);

    // 队列开空间 
   //因为线程池结构体内只给task_queue指针分配了空间，所以这里要给队列分配空间
    pool->task_queue =  (threadpool_task_t *)malloc(sizeof(threadpool_task_t)*queue_max_size);
    
    if (pool->task_queue == NULL)
   {
       printf("malloc task queue false;\n");
       break;
   }

    // 初始化互斥锁和条件变量 
    if ( pthread_mutex_init(&(pool->lock), NULL) != 0           ||
       pthread_mutex_init(&(pool->thread_counter), NULL) !=0  || 
     pthread_cond_init(&(pool->queue_not_empty), NULL) !=0  ||
     pthread_cond_init(&(pool->queue_not_full), NULL) !=0)
    {
       printf("init lock or cond false;\n");
       break;
     }

    // 启动min_thr_num个工作线程
    for (i=0; i<min_thr_num; i++)
    {
       // pool指向当前线程池，使用回调函数启动min_thr_num个工作线程
       pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void *)pool);
       //printf("start thread 0x%x... \n", (unsigned int)pool->threads[i]);
    }
    //创建一个管理者线程
    pthread_create(&(pool->admin_tid), NULL, admin_thread, (void *)pool);

    return pool;
 } while(0);

   // 释放pool的空间 (异常处理)
   threadpool_free(pool);
   return NULL;
}

//向线程池的任务队列中添加一个任务（主线程）
int threadpool_add_task(threadpool_t *pool, void *(*function)(void *arg), void *arg)
{
   //上互斥锁，避免其他线程修改线程池变量
   pthread_mutex_lock(&(pool->lock));

   //如果队列满了,调用wait阻塞
   while ((pool->queue_size == pool->queue_max_size) && (!pool->shutdown))
   {
      //生产者线程生产任务，放进任务队列，但是可能因为任务队列满而阻塞
      pthread_cond_wait(&(pool->queue_not_full), &(pool->lock));
   }

   //如果线程池处于关闭状态
   if (pool->shutdown)
   {
      pthread_mutex_unlock(&(pool->lock));
      return -1;
   }

   //清空工作线程的回调函数的参数arg
   //因为arg指向的是字符串常量，避免空间浪费
   if (pool->task_queue[pool->queue_rear].arg != NULL)
   {
      free(pool->task_queue[pool->queue_rear].arg);
      pool->task_queue[pool->queue_rear].arg = NULL;
   }

   //添加任务到任务队列
   pool->task_queue[pool->queue_rear].function = function;
   pool->task_queue[pool->queue_rear].arg = arg;
   pool->queue_rear = (pool->queue_rear + 1) % pool->queue_max_size;  // 循环队列 
   pool->queue_size++;

   //添加完任务后,队列就不为空了,唤醒线程池中的一个线程
   //生产者通知消费者人物队列不空
   pthread_cond_signal(&(pool->queue_not_empty));
   pthread_mutex_unlock(&(pool->lock));

   return 0;
}

//管理线程
void *admin_thread(void *threadpool)
{
   int i;
   threadpool_t *pool = (threadpool_t *)threadpool;
   while (!pool->shutdown)
   {
      //printf("admin -----------------\n");
      sleep(DEFAULT_TIME);                             //隔一段时间再管理
      pthread_mutex_lock(&(pool->lock));               //加锁
      int queue_size = pool->queue_size;               //任务数
      int live_thr_num = pool->live_thr_num;           //存活的线程数
      pthread_mutex_unlock(&(pool->lock));             //解锁

      pthread_mutex_lock(&(pool->thread_counter));
      int busy_thr_num = pool->busy_thr_num;           //忙线程数
      pthread_mutex_unlock(&(pool->thread_counter));

     // printf("admin busy live -%d--%d-\n", busy_thr_num, live_thr_num);
      //创建新线程 实际任务数量大于 最小正在等待的任务数量，存活线程数小于最大线程数
      if (queue_size >= MIN_WAIT_TASK_NUM && live_thr_num <= pool->max_thr_num)
      //if (queue_size >= 3 && live_thr_num <= pool->max_thr_num)
      {
        // printf("admin add-----------\n");
         pthread_mutex_lock(&(pool->lock));
         int add=0;

         //一次增加 DEFAULT_THREAD_NUM 个线程
         //如果当前添加的线程数量与当前线程池中的线程总数量都小于线程池创建时的最大数量
         //并且这次添加的数量小于规定的一次最多添加的个数，就创建新的工作线程
         for (i=0; i<pool->max_thr_num && add<DEFAULT_THREAD_NUM 
              && pool->live_thr_num < pool->max_thr_num; i++)
         {
            if (pool->threads[i] == 0 || !is_thread_alive(pool->threads[i]))
           {
              pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void *)pool);
              add++;
              pool->live_thr_num++;
              //printf("new thread -----------------------\n");
           }
         }

         pthread_mutex_unlock(&(pool->lock));
      }

      //销毁多余的线程 忙线程x2 都小于 存活线程，并且存活的大于最小线程数
      if ((busy_thr_num*2) < live_thr_num  &&  live_thr_num > pool->min_thr_num)
      {
         // printf("admin busy --%d--%d----\n", busy_thr_num, live_thr_num);
         //一次给DEFAULT_THREAD_NUM个线程发自杀信号
         //具体是否自杀由工作线程自己控制
         pthread_mutex_lock(&(pool->lock));
         pool->wait_exit_thr_num = DEFAULT_THREAD_NUM;
         pthread_mutex_unlock(&(pool->lock));

         for (i=0; i<DEFAULT_THREAD_NUM; i++)
        {
           //通知正在处于空闲的线程，自杀
           pthread_cond_signal(&(pool->queue_not_empty));
           //printf("admin cler --\n");
        }
      }
    }
   return NULL;
}

//工作线程
void * threadpool_thread(void *threadpool)
{
  threadpool_t *pool = (threadpool_t *)threadpool;
  threadpool_task_t task;

  while (true)
  {
    pthread_mutex_lock(&(pool->lock)); 

    //无任务则阻塞在 “任务队列不为空” 上，有任务则跳出
    while ((pool->queue_size == 0) && (!pool->shutdown))
    { 
     // printf("thread 0x%x is waiting \n", (unsigned int)pthread_self());
      //消费者线程等待消任务队列不空
      pthread_cond_wait(&(pool->queue_not_empty), &(pool->lock));

      // 判断管理者线程是否通知该工作线程自杀
      if (pool->wait_exit_thr_num > 0)
      {
        pool->wait_exit_thr_num--;
        //由工作线程自己判断现在的线程是否小于规定的最小线程数
        //若小于就不执行自杀操作，反之自杀
        if (pool->live_thr_num > pool->min_thr_num)
        {
           //printf("thread 0x%x is exiting \n", (unsigned int)pthread_self());
           pool->live_thr_num--;
           pthread_mutex_unlock(&(pool->lock));
           pthread_exit(NULL);//结束线程
        }
      }
   }

   // 线程池开关状态
   if (pool->shutdown) //关闭线程池
   {
      pthread_mutex_unlock(&(pool->lock));
     // printf("thread 0x%x is exiting \n", (unsigned int)pthread_self());
      pthread_exit(NULL); //线程自己结束自己
   }

    //否则该线程可以拿出任务
    task.function = pool->task_queue[pool->queue_front].function; //出队操作
    task.arg = pool->task_queue[pool->queue_front].arg;

    pool->queue_front = (pool->queue_front + 1) % pool->queue_max_size;  //环型结构
    pool->queue_size--;

    //通知可以添加新任务
    pthread_cond_broadcast(&(pool->queue_not_full));

    //释放线程锁
    pthread_mutex_unlock(&(pool->lock));

    //执行刚才取出的任务
   // printf("thread 0x%x start working \n", (unsigned int)pthread_self());
    pthread_mutex_lock(&(pool->thread_counter));            //锁住忙线程变量
    pool->busy_thr_num++;
    pthread_mutex_unlock(&(pool->thread_counter));

   (*(task.function))(task.arg);                           //执行任务

    //任务结束处理
    //printf("thread 0x%x end working \n", (unsigned int)pthread_self());
    pthread_mutex_lock(&(pool->thread_counter));
    pool->busy_thr_num--;
    pthread_mutex_unlock(&(pool->thread_counter));
  }

  pthread_exit(NULL);
}


//线程是否存活
int is_thread_alive(pthread_t tid)
{
   int kill_rc = pthread_kill(tid, 0);     //发送0号信号，测试是否存活
   if (kill_rc == ESRCH)  //线程不存在
   {
      return false;
   }
   return true;
}

//释放线程池
int threadpool_free(threadpool_t *pool)
{
   if (pool == NULL)
     return -1;
   if (pool->task_queue)
      free(pool->task_queue);
   if (pool->threads)
   {
      free(pool->threads);
      pthread_mutex_lock(&(pool->lock));               //先锁住再销毁
      pthread_mutex_destroy(&(pool->lock));
      pthread_mutex_lock(&(pool->thread_counter));
      pthread_mutex_destroy(&(pool->thread_counter));
      pthread_cond_destroy(&(pool->queue_not_empty));
      pthread_cond_destroy(&(pool->queue_not_full));
   }
   free(pool);
   pool = NULL;

   return 0;
}

//销毁线程池
int threadpool_destroy(threadpool_t *pool)
{
   int i;
   if (pool == NULL)
   {
     return -1;
   }
   pool->shutdown = true;

   //销毁管理者线程
   pthread_join(pool->admin_tid, NULL);

   //通知所有线程去自杀(在自己领任务的过程中)
   for (i=0; i<pool->live_thr_num; i++)
   {
     pthread_cond_broadcast(&(pool->queue_not_empty));
   }

   //等待线程结束 先是pthread_exit 然后等待其结束
   for (i=0; i<pool->live_thr_num; i++)
   {
     pthread_join(pool->threads[i], NULL);
   }

   threadpool_free(pool);
   return 0;
}

//线程池回调函数 tcp
void task_func(void * arg)
{
   Tmp tmp = *((Tmp *)arg);
   int epfd = tmp.epfd;
   struct epoll_event*evs = tmp.evs;
   int i = tmp.i;
   int ret;

   ret = recv_login_client(epfd,evs,i);
   if(ret < 0)
   {
      exit(-1);
   }
}

//线程池回调函数 udp
void task_func2(void * arg)
{
   Tmp tmp = *((Tmp *)arg);
   int epfd = tmp.epfd;
   struct epoll_event*evs = tmp.evs;
   int i = tmp.i;
   
   recv_udp_client(pdb,epfd,evs,i);
}

/*
void task_func(void * arg)
{
    int num = *(int*)arg;

    printf("thread %ld is working,number = %d\n",pthread_self(),num);
    sleep(1);	
}

int main()
{
	int i;

	printf("hello from thread_pooc!\n");
	
	threadpool_t * pool = threadpool_create(3,50,100);
	
	for(i = 0;i < 100;i++)
	{
		int *num = (int *)malloc(sizeof(int));
		*num = i + 100;
		threadpool_add_task(pool,(void *)task_func,(void *)num);
	}
	
	sleep(10);
	threadpool_destroy(pool);
	
	return 0;
}*/






