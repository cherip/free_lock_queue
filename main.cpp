#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include "time_count.h"
#include "stdint.h"

//const int count_per_thread_push = 100000000;
const int count_per_thread_push = 100000000;

const float CPU_MHZ = 2797; //use cat /proc/cpuinfo get the value
const float CPU_tick_count_per_second = CPU_MHZ*1000*1000;

const int CACAH_SIZE = 0xFFFFFF; 

struct lock_free_queue   //我把实现部分去掉了，框架供参考，其中打印部分帮助调试，确定延迟。正式比拼可以注释掉。
{
	void push(unsigned long long pop_time)
	{
        int next_pos = get_next_push_position();   
  //      printf("%d %d\n", next_pos, size);
        cache[next_pos] = pop_time;
	};
	bool pop()
	{
        int next_pos = get_next_pop_position();
        //printf("%d\n", next_pos);
        if (next_pos != -1 && cache[next_pos] == 0) {
            printf("%d %d %d %d %d\n", next_pos, size, head, end, cache[next_pos]);
            return false;
        }
        if (next_pos != -1) {
            now++;
            if(now%(100000) == 0) {
//              printf("task get:%u,task write:%u,latency:%.3f\n",
//                      rdtsc(),
//                      cache[next_pos],
//                      (float)(cache[next_pos] - rdtsc()) / CPU_tick_count_per_second);
                        //(cache[next_pos] - rdtsc())/CPU_tick_count_per_second);	
                //printf("%.3f %d\n", (float)now / (count_per_thread_push * 10), (end + CACAH_SIZE - head) & CACAH_SIZE);
            }
        }
        return true;
	};
	lock_free_queue()
	{
        head = 0;
        end = 0;
        now = 0;
        size = 0;
        cache = new unsigned long long[CACAH_SIZE];
	};

	~lock_free_queue()
	{
        delete [] cache;
	}

    volatile unsigned long long *cache;
    volatile int head, end;
    volatile int now;
    volatile int size;

    int get_next_push_position() {
        for (;;) {
            int current = end;
            int next = (current + 1) & CACAH_SIZE;
            //printf("- %d %d %d\n", current, next, end);
            if (__sync_bool_compare_and_swap(&end, current, next)) {
                size++;
                return current;
            } 
        }
    }

    int get_next_pop_position() {
        int num = 0;
        for (;;) {
            if (size < 0) {
                printf("Error!\n", size);
            }
            if (size == 0) {
                return -1;
            } 
            if (head == end) {
                return -1;
            }
            int current = head;
            int next = (current + 1) & CACAH_SIZE;
            //printf("-- %d %d %d\n", current, next, head);
            if (__sync_val_compare_and_swap(&head, current, next)) {
                size--;
                return current;
            } 
        } 
    }

    void print() {
        for (int i = head; i != end;) {
            printf("%u ", cache[i]);
            i = (i + 1) & CACAH_SIZE; 
        }
        printf("\n");
    }
};

void* pop(void* queue)
{
	lock_free_queue* lfq = (lock_free_queue*)queue;
	do{
	}while(lfq->pop());	
};

void* push(void* queue)
{
        lock_free_queue* lfq = (lock_free_queue*)queue;
	for(int i=0;i<count_per_thread_push/4;++i)
	{
		unsigned long long now = rdtsc();
        if (now == 0) {
            printf("Error!\n");
            continue;
        }
        lfq->push(now);
		lfq->push(now);
		lfq->push(now);
		lfq->push(now);
	}
	
};

void* push_end(void* queue)
{
	lock_free_queue* lfq = (lock_free_queue*)queue;
	for(int i=0;i<1000;++i)
	{
		lfq->push(0);
	}
}

int num_thread_pop = 0;
int num_thread_push = 10;

int main(void)
{
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

//    lfq.print();
	
	for(int i=0;i<num_thread_pop;++i)	//wait pop quit
        {
                pthread_join(thread_pop[i],NULL);
        }
	
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
