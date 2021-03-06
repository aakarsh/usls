/* -*- Mode: c; tab-width: 2; indent-tabs-mode: 0; c-basic-offset: 2; -*- */

#include <math.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <getopt.h>
#include <grp.h>
#include <linux/limits.h>
#include <math.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sysinfo.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "queue.h"

// Basic constructor
struct queue* queue_create_node(void* data, int data_len) {
  struct queue* node = malloc(sizeof(struct queue));
  node->data = data;
  node->data_len = data_len;
  return node;  
}

struct queue_head* queue_new() {
  struct queue_head* q =  malloc (sizeof(struct queue_head));
  q->head = NULL;
  q->size = 0;
  q->finish_filling = 0;
  q->free_data = NULL;
  pthread_mutex_init(&q->lock, NULL);
  pthread_cond_init(&q->modified_cv, NULL);
  return q;  
}


int queue_destroy(struct queue_head* q) {
  pthread_mutex_lock(&q->lock);
  struct queue * cur = q->head;
  while(cur!=NULL){
    struct queue* tmp = cur;
    if(tmp->data != NULL){
      //TODO free data
      //fprintf(stderr,"Trying to free [%p] \n",tmp->data);
      //TODO some pointers are looking at stale buffers?

      /* if(q->free_data!=NULL) {
       *   fprintf(stderr,"queue_destroy : free_data of len  %d \n", tmp->data_len);
       *   q->free_data(tmp->data);        
       * } */

      //fprintf(stderr,"Finished free [%p] \n",tmp->data);
    }
    free(tmp);
    cur = cur->next;    
  }
  
  q->size = 0;
  pthread_cond_init(&q->modified_cv, NULL);
  pthread_mutex_unlock(&q->lock);
  free(q);

  return 0;
}

/**
 * Adds a free'ed iovec_list node back into the free_list
 */
void queue_prepend(struct queue_head* q , struct queue* node) {
  pthread_mutex_lock(&q->lock);
  node->next = q->head;
  q->head = node;
  q->size +=1;
  pthread_cond_signal(&q->modified_cv);
  pthread_mutex_unlock(&q->lock);
}

void queue_prepend_one(struct queue_head* q , void* node, int node_sz) {
  queue_prepend_all(q,node,node_sz,1);
}

void queue_prepend_all_list(struct queue_head* q , struct queue* node_list){
  pthread_mutex_lock(&q->lock);

  struct queue* last = NULL;
  struct queue* cur = node_list;
  int sz = 0;
  while(cur!=NULL) {
    last = cur;
    cur = cur->next;
    sz++;
  }

  last->next = q->head;

  q->head = node_list;
  q->size += sz;
  pthread_cond_signal(&q->modified_cv);
  pthread_mutex_unlock(&q->lock);
}


void queue_prepend_all(struct queue_head* q , void* data, int node_sz,int n) {
  int i = 0;
  for (i = 0 ; i < n; i++) {
    struct queue* node = malloc(sizeof(struct queue));
    node->data = (data + i*node_sz);
    node->data_len = node_sz;   
    queue_prepend(q,node);
  }
}

/**
 * Mark the queue such that listeners know that there will be no new
 * items.
 */
void queue_mark_finish_filling(struct queue_head* list){
  pthread_mutex_lock(&list->lock);
  list->finish_filling = 1;
  pthread_cond_broadcast(&list->modified_cv);
  pthread_mutex_unlock(&list->lock);
}



// TODO start of thinking we can take as many items as we want
// package all data into linear array
// also ssume that all items have univorm size
/**
 * Try to take n items from queue if possible, blocking if for more
 * items if not possible.
 */
struct queue* queue_take(struct queue_head* queue, int n) 
{
	assert(n>0);
  int i = 0;
  pthread_mutex_lock(&queue->lock);

  while((queue->size < n)) {
    if(!queue->finish_filling)
      pthread_cond_wait(&queue->modified_cv,&queue->lock);
    else { 
      pthread_mutex_unlock(&queue->lock);
      return NULL;
    }
  }

  struct queue * last = NULL;
  struct queue * cur = queue->head; 

  struct queue* retval = cur;

 // what about the previous cur check
  while(i < n && cur!=NULL) {
    last = cur;
    cur = cur->next;
    i++;
  } 

  // unlink last node
	last->next = NULL;

  queue->head = cur;
  queue->size = queue->size - n;

  pthread_mutex_unlock(&queue->lock);

  return retval;
}





/**
 * Simple function which will keep running until we receive a NULL
 * item in the input queue.
 *
 * It will put into an output queue if such a queue has been specfied.
 */
void* run_queue_tranformer(void* arg) {

  struct queue_transformer_arg* qarg = arg;

  while(1) {
    struct queue* msg = queue_take(qarg->in_q,1);

    if(msg == NULL) { // done
      fprintf(stderr,"Stopping %s %d end\n",qarg->name,qarg->id);
      return NULL;
    }
    
    void* obj = msg->data;
    fprintf(stderr,"Running  %s:%d \n",qarg->name,qarg->id);

    struct queue* node = qarg->transform(obj,qarg->id,qarg->priv,qarg->in_q, qarg->out_q);  

    if(node!=NULL && qarg->out_q !=NULL) {
      fprintf(stderr,"Prepending to %p node %p \n",qarg->out_q,node);     
      queue_prepend(qarg->out_q, node);
    }
  }  

  return NULL;
}


void* run_queue_producer(void* arg) {
  struct queue_transformer_arg* qarg = arg;
	qarg->transform(NULL,qarg->id,qarg->priv,qarg->in_q,qarg->out_q);    
  return NULL;
}


/**
 * Starts n threads which will work take items from input queue and
 * move to output queue after performing a transform
 */
struct transformer_info* start_tranformers(char* name,
                             queue_transformer transform,void* priv, 
                             struct queue_head* in_q, 
                             struct queue_head* out_q, 
                             int n)
{
  struct transformer_info* info = malloc(sizeof(struct transformer_info));
  info->num_threads = n;
  info->thread_ids = malloc(sizeof(pthread_t)* n);
  info->args = malloc(sizeof (struct queue_transformer_arg) *n);

	if(in_q == NULL) {// pure producer thread does not use an input queue 
		int i = 0;
    struct queue_transformer_arg* args = info->args;
    strncpy(args[i].name,name,19);
    args[i].id = i;
    args[i].in_q = in_q;
    args[i].out_q = out_q;
    args[i].transform = transform;
    args[i].priv = priv;

		pthread_create(&info->thread_ids[0],NULL,run_queue_producer, &(args[i]));

	}  else {
		for(int i = 0 ; i < n; i++) {
			struct queue_transformer_arg* args = info->args;
			strncpy(args[i].name,name,19);
			args[i].id = i;
			args[i].in_q = in_q;
			args[i].out_q = out_q;
			args[i].transform = transform;
			args[i].priv = priv;

			fprintf(stderr,"Creating thread %s:%d %p %p \n",args[i].name,args[i].id, 
							args[i].in_q,args[i].out_q);
			pthread_create(&info->thread_ids[i],NULL,run_queue_tranformer,&(args[i]));
		}
	}
	
  return info;
}

void join_transformers(struct transformer_info* tr) {
  for(int i = 0; i < tr->num_threads;i++)
    pthread_join(tr->thread_ids[i],NULL);     
}


void free_transformers(struct transformer_info* tr){
  free(tr->args);
  free(tr->thread_ids);
  free(tr);
}
