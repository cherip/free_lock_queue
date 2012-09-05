#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "time_count.h"
#include "stdint.h"

//const int count_per_thread_push = 100000000;
const int count_per_thread_push = 100000000;

const float CPU_MHZ = 2797; //use cat /proc/cpuinfo get the value
const float CPU_tick_count_per_second = CPU_MHZ*1000*1000;

const int CACAH_SIZE = 0xFFFFFF; 
int num_thread_pop = 2;
int num_thread_push = 5;
int real_queue_num = 5;

struct real_lock_free_queue {
    void push(unsigned long long push_time) {
        int cur = end;
        int next = (cur + 1) & CACAH_SIZE;
        if (next == head) {
            printf("error\n");
        }
        cache[cur] = push_time;
        end = next;
        if (push_time != 0)
            push_count++;
    }

    unsigned long long get_val() {
        if (head == end) return 0xFFFFFFFFFFFFFFF;
        if (cache[head] == 0) {
        //    printf("get %d %d\n", head, end);
        }
        return cache[head];
    }

    bool pop() {
        if (head == end) return true;
        if (cache[head] == 0) {
            printf ("pop %d\n", head);
            return false;
        }
        //printf("pop %lld\n", cache[head]);
        head = (head + 1) & CACAH_SIZE;
        pop_count++;
        return true;
    }

    real_lock_free_queue() {
        head = 0;
        end = 0;
        push_count = 0;
        pop_count = 0;
        cache = new unsigned long long[CACAH_SIZE];
    }

    int head, end;
    unsigned long long *cache;
    int push_count;
    int pop_count;
};

struct lock_free_queue   //我把实现部分去掉了，框架供参考，其中打印部分帮助调试，确定延迟。正式比拼可以注释掉。
{
	void push(unsigned long long pop_time)
	{
        int idx = get_index(push_count);
        push_count++;
        if (push_count % 10000000 == 0) {
            printf("%.4f\n", (float)push_count / (count_per_thread_push * 10));
        }

        real_lock_free_queue *c_queue = &real_queue[idx];
        //printf("push %d\n", idx);
        for (;;) {
            int cur = push_lock_flag[idx];
            int new_val = 1 - cur;
            if (cur == 0) {
                if (__sync_bool_compare_and_swap(&push_lock_flag[idx], cur, new_val)) {
                    c_queue->push(pop_time);
                    push_lock_flag[idx] = 0;
                    return;
                }
            }
        }
	}
	bool pop()
	{
        for(;;) {
            int flag = 0;
            int quit = 1;
            unsigned long long min_val = 0xFFFFFFFFFFFFFFF;
            //printf("new round...\n");
            for (int i = 0; i < real_queue_num; i++) {
                unsigned long long tmp = real_queue[i].get_val(); 
                if (tmp != 0 && tmp < min_val) {
                    min_val = tmp;
                    flag = i;
                }
                if (tmp != 0) {
                    //printf("queue num %d %lld\n", i, tmp);
                    quit = 0;
                }
            }
            if (quit) return false;
            //if (min_val == 0) return false;
            //printf("%d %lld\n", flag, min_val);
            if (pop_lock_flag[flag] == 0) {
                if (__sync_bool_compare_and_swap(&pop_lock_flag[flag], 0, 1)) {
                    bool ret = real_queue[flag].pop();
                    pop_lock_flag[flag] = 0;
                    return true;
                }
            }
        }
	};
    real_lock_free_queue* get_queue(int k) {
        return &real_queue[k];
    }

    int get_index(int k) {
        //printf("idx: %d\n", (k * 3) % real_queue_num);
        return (k * 3) % real_queue_num;
    }

    int calc_total_push() {
        unsigned long long total = 0;
        unsigned long long total_pop = 0;
        for (int i = 0; i < real_queue_num; ++i) {
            total += real_queue[i].push_count;
            total_pop += real_queue[i].pop_count;
        }
        printf("%lld %lld\n", total, total_pop);
    }

	lock_free_queue()
	{
        push_count = 0;
        real_queue = new real_lock_free_queue[real_queue_num];
        push_lock_flag = new int[real_queue_num];
        pop_lock_flag = new int[real_queue_num];
        for (int i = 0; i < real_queue_num; i++) {
            push_lock_flag[i] = 0;
            pop_lock_flag[i] = 0;
        }
	};

	~lock_free_queue()
	{
	}

    volatile int push_count;
    real_lock_free_queue *real_queue;
    volatile int *push_lock_flag;
    volatile int *pop_lock_flag;
};

void* pop(void* queue)
{
	lock_free_queue* lfq = (lock_free_queue*)queue;
	do{
	}while(lfq->pop());	
    printf("pop thread quit\n");
};

void* push(void* queue)
{
        lock_free_queue* lfq = (lock_free_queue*)queue;
	for(int i=0;i<count_per_thread_push/4;++i)
	{
		unsigned long long now = rdtsc();
        if (now == 0) {
            printf("   Error!\n");
            continue;
        }
        lfq->push(now);
		lfq->push(now);
		lfq->push(now);
		lfq->push(now);
	}
    printf("push thread quit\n");
};

void* push_end(void* queue)
{
	lock_free_queue* lfq = (lock_free_queue*)queue;
	for(int i=0;i<1000;++i)
	{
		lfq->push(0);
	}
}


int main(void)
{
    clock_t start = clock();
    pthread_t* thread_pop = (pthread_t*) malloc(num_thread_pop*sizeof( pthread_t));
	pthread_t* thread_push = (pthread_t*) malloc(num_thread_push*sizeof( pthread_t));
	pthread_t* thread_push_end = (pthread_t*) malloc(sizeof( pthread_t));
	lock_free_queue lfq;
	
	for(int i=0;i<num_thread_push;++i)
        {
                pthread_create(&thread_push[i],NULL,push,&lfq);
        }
	
	for(int i=0;i<num_thread_pop;++i)
       	{
		pthread_create(&thread_pop[i],NULL,pop,&lfq);
	}
			
 
	for(int i=0;i<num_thread_push;++i)		//make push end
	{
		pthread_join(thread_push[i],NULL);
	}	

    printf(">>>>>>>>>>>>>>>>>>>> Finish Input!\n");
	
	pthread_create(thread_push_end,NULL,push_end,&lfq); //push end signal
	

	for(int i=0;i<num_thread_pop;++i)	//wait pop quit
        {
                pthread_join(thread_pop[i],NULL);
        }
	
    printf("total push:\n");
    lfq.calc_total_push();
    printf("costs %.3f\n", (float)(clock() - start) / CLOCKS_PER_SEC);

	if( NULL != thread_pop )
	{
		free(thread_pop);
		thread_pop = NULL;
	}
	if( NULL != thread_pop )
	{
		free(thread_push);
		thread_push = NULL;
	}
	if( NULL != thread_push_end )
        {
                free( thread_push_end );
                thread_push_end = NULL;
        }

	
}
