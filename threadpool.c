#ifndef THREADPOOL_C
#define THREADPOOL_C
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "threadpool.h"
/**
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 * this function should:
 * 1. input sanity check
 * 2. initialize the threadpool structure
 * 3. initialized mutex and conditional variables
 * 4. create the threads, the thread init function is do_work and its argument is the initialized threadpool.
 */
threadpool* create_threadpool(int num_threads_in_pool){
    if(num_threads_in_pool>MAXT_IN_POOL || num_threads_in_pool<1)
        return NULL;//print usage
    threadpool* tp = (threadpool*)malloc(sizeof(threadpool));
    if(!tp) {
        perror("malloc");
        return NULL;
    }
    tp->num_threads=num_threads_in_pool;
    tp->qhead=tp->qtail=NULL;
    tp->qsize=0;
    if(pthread_cond_init(&(tp->q_empty),NULL)) {
        free(tp);
        perror("cond init");
        return NULL;
    }
    if(pthread_cond_init(&(tp->q_not_empty),NULL)) {
        pthread_cond_destroy(&(tp->q_empty));
        free(tp);
        perror("cond init");
        return NULL;
    }
    tp->shutdown=tp->dont_accept=0;
    if(pthread_mutex_init (&(tp->qlock),NULL)<0){
        pthread_cond_destroy(&(tp->q_empty));
        pthread_cond_destroy(&(tp->q_not_empty));
        free(tp);
        perror("mutex init");
        return NULL;
    }
    tp->threads = (pthread_t*) malloc((sizeof(pthread_t)*tp->num_threads));
    if(!tp->threads){
        pthread_cond_destroy(&(tp->q_empty));
        pthread_cond_destroy(&(tp->q_not_empty));
        pthread_mutex_destroy(&(tp->qlock));
        free(tp);
        perror("malloc");
        return NULL;
    }
    for(int i=0;i<tp->num_threads;i++){
        if(pthread_create(&(tp->threads[i]),NULL,&do_work,(void*)tp)){
            pthread_cond_destroy(&(tp->q_empty));
            pthread_cond_destroy(&(tp->q_not_empty));
            pthread_mutex_destroy(&(tp->qlock));
            free(tp->threads);
            free(tp);
            perror("pthread_create");
            return NULL;
        }
    }
    return tp;
}

void dispatch(threadpool* tp, dispatch_fn dispatch_to_here, void *arg){
    if(tp->dont_accept)
        return;
    work_t* new_work = (work_t*) malloc(sizeof(work_t));
    if(!new_work){
        perror("malloc");
        destroy_threadpool(tp);
    }
    new_work->routine = (int (*)(void *))dispatch_to_here;
    new_work->arg = arg;
    pthread_mutex_lock(&(tp->qlock));
    if(tp->qsize==0)
        tp->qhead = new_work;
    else
        tp->qtail->next = new_work;

    tp->qtail = new_work;
    new_work->next = NULL;
    tp->qsize ++;
    pthread_cond_signal(&(tp->q_not_empty));
    pthread_mutex_unlock(&(tp->qlock));
}
void* do_work(void* p){
    threadpool* tp = (threadpool*)p;
    pthread_t self = pthread_self();
    while(1){
        pthread_mutex_lock(&(tp->qlock));
        if(!tp || tp->shutdown)
            break;
        else if(tp->dont_accept==1 && tp->qsize==0) {
            //pthread_cond_signal(&(tp->q_empty));
            //pthread_cond_wait(&(tp->q_not_empty),&(tp->qlock));
            break;
        }
        if(tp->qsize==0) {
            pthread_cond_wait(&(tp->q_not_empty), &(tp->qlock));
            pthread_mutex_unlock(&(tp->qlock));
        }
        else {
            work_t *to_do = tp->qhead;
            tp->qhead = to_do->next;
            tp->qsize--;
            pthread_mutex_unlock(&(tp->qlock));
            to_do->routine(to_do->arg);
            free(to_do);
        }
    }
    pthread_mutex_unlock(&(tp->qlock));
    pthread_cond_signal(&(tp->q_empty));
    return NULL;
}

void destroy_threadpool(threadpool* tp){
    pthread_mutex_lock(&(tp->qlock));
    tp->dont_accept=1;
    pthread_cond_broadcast(&(tp->q_not_empty));
    pthread_cond_wait(&(tp->q_empty), &(tp->qlock));
    tp->shutdown=1;
    pthread_mutex_unlock(&(tp->qlock));
    void *status;

    for(int i=0;i<tp->num_threads;i++)
        pthread_join(tp->threads[i], &status);

    pthread_mutex_destroy(&(tp->qlock));
    pthread_cond_destroy(&(tp->q_empty));
    pthread_cond_signal(&(tp->q_not_empty));
    pthread_cond_destroy(&(tp->q_not_empty));
    free(tp->threads);
    free(tp);
}
/*
int test_print(void* p){
    printf("in test_print()\n");
    int* r = (int*) p;
    for(int i=0;i<100;i++)
        printf("%d ",*r);
    printf("\n");
    return 0;
}

int main(int argc,char** argv){
    int num=0;
    int mult = 100;
    scanf("%d",&num);
    threadpool* tp = create_threadpool(num);
    if(!tp){
        printf("init field\n");
        return 1;
    }
    printf("len of thread array = %ld\n",sizeof(tp->threads));
    int *arr=(int*) malloc(sizeof(int)*num*mult);
    for(int r=0;r<num*mult;r++) {
        arr[r]=r;
        dispatch(tp, (int(*)(void *))test_print, (void *)(&arr[r]));
    }
    destroy_threadpool(tp);
    free(arr);
    return 0;
}
*/
#endif