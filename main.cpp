#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "time_count.h"
#include "stdint.h"

//const int count_per_thread_push = 100000000;
const int count_per_thread_push = 1000000;

const float CPU_MHZ = 2797; //use cat /proc/cpuinfo get the value
const float CPU_tick_count_per_second = CPU_MHZ*1000*1000;

const int CACAH_SIZE = 0xFFFFFF; 
int num_thread_pop = 10;
int num_thread_push = 10;
int real_queue_num = 5;

#include <stddef.h>

struct node_t;
struct pointer_t 
{
    node_t *ptr;
    unsigned int tag;
    pointer_t() {
        ptr = NULL;
        tag = 0;
    }
    pointer_t(node_t *a_ptr, unsigned int a_tag) {
        ptr = a_ptr; tag=a_tag;
    }

    friend
        bool operator==(pointer_t const &l, pointer_t const &r)
    {
        return l.ptr == r.ptr && l.tag == r.tag;
    }

    friend 
        bool operator!=(pointer_t const &l, pointer_t const &r)
    {
        return !(l == r);
    }
};

//typedef void * data_type;

#define dummy_val 0

typedef unsigned long long data_type;

struct node_t { 
    pointer_t next; // wgg 发现了64位错误的原因是 cmp16b需要16字节对齐，现在改正过来了。
    data_type value; 
    node_t() {
        value = dummy_val;
        next=  pointer_t(NULL,0);
    }
};



#ifdef __x86_64__
inline
bool CAS2(pointer_t *addr,
         pointer_t &old_value,
         pointer_t &new_value)
{
    bool  ret;
    __asm__ __volatile__(
        "lock cmpxchg16b %1;\n"
        "sete %0;\n"
        :"=m"(ret),"+m" (*(volatile pointer_t *) (addr))
        :"a" (old_value.ptr), "d" (old_value.tag), "b" (new_value.ptr), "c" (new_value.tag));
    return ret;
}

#else
inline
bool CAS2(pointer_t *addr,
         pointer_t &old_value,
         pointer_t &new_value)
{
    bool  ret;
    __asm__ __volatile__(
        "lock cmpxchg8b %1;\n"
        "sete %0;\n"
        :"=m"(ret),"+m" (*(volatile pointer_t *) (addr))
        :"a" (old_value.ptr), "d" (old_value.tag), "b" (new_value.ptr), "c" (new_value.tag));
    return ret;
}
#endif

class queue_t 
{
    pointer_t tail_;
    pointer_t head_;
public:
    queue_t() {

    }

    void init() {
        node_t *nd = new node_t();
        nd->next = pointer_t(NULL, 0);
        head_ = pointer_t(nd, 0);
        tail_ = pointer_t(nd, 0);
    }

    void enqueue(data_type val) {
        pointer_t tail, next;
        node_t* nd = new node_t();
        nd->value = val;
        //printf("%lld\n", val);
        while(true){
            tail = this->tail_; 
            next = tail.ptr->next;
            if (tail == this->tail_) {
                if(next.ptr == NULL) {
                    pointer_t new_pt(nd, next.tag+1);
                    if(CAS2(&(this->tail_.ptr->next), next, new_pt)){ 
                        break; // Enqueue done!
                    }
                }else {
                    pointer_t new_pt(next.ptr, tail.tag+1);
                    CAS2(&(this->tail_), tail, new_pt); 
                }
            }
        }
        pointer_t new_pt(nd, tail.tag+1);
        CAS2(&(this->tail_), tail, new_pt);
    }


    data_type dequeue() {
        pointer_t tail, head, next;
        data_type val=NULL;
        while(true){ 
            head = this->head_; 
            tail = this->tail_; 
            next = (head.ptr)->next; 
            if (head != this->head_) continue;

            if(head.ptr == tail.ptr){
                if (next.ptr == NULL){ 
                    return 1;
                }
                pointer_t new_pt(next.ptr, tail.tag+1);
                CAS2(&(this->tail_), tail, new_pt);
            } else{ 
                val = next.ptr->value;
                pointer_t new_pt(next.ptr, head.tag+1);
                if(CAS2(&(this->head_), head, new_pt)){
                    break;
                }
            }
        }
        delete head.ptr;
        return val;
    }
};


struct lock_free_queue   //ÎÒ°ÑÊµÏÖ²¿·ÖÈ¥µôÁË£¬¿òŒÜ¹©²Î¿Œ£¬ÆäÖÐŽòÓ¡²¿·Ö°ïÖúµ÷ÊÔ£¬È·¶šÑÓ³Ù¡£ÕýÊœ±ÈÆŽ¿ÉÒÔ×¢ÊÍµô¡£
{
	void push(unsigned long long pop_time)
	{
        real_queue->enqueue(pop_time);
	}
	bool pop()
	{
        if (real_queue->dequeue() == 0) 
            return false;
        return true;
	};

	lock_free_queue()
	{
        real_queue = new queue_t;
        real_queue->init();
	};

	~lock_free_queue()
	{
	}
    
    queue_t *real_queue;
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
