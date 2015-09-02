/* -*- Mode: c; tab-width: 2; indent-tabs-mode: 1; c-basic-offset: 2; -*- */

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

struct queue_head* queue_init(struct queue_head** list) {
  struct queue_head* new_list =  (struct queue_head*) malloc(sizeof(struct queue_head));

  if(list!=NULL){
    *list = new_list;
  }
  (new_list)->head = NULL;
  (new_list)->size = 0;
  (new_list)->finish_filling = 0;

  pthread_mutex_init(&(new_list)->lock, NULL);
  pthread_cond_init(&(new_list)->modified_cv, NULL);

  return new_list;  
}



/**
 * Adds a free'ed iovec_list node back into the free_list
 */
void queue_prepend(struct queue_head* list , struct queue* node) {
  pthread_mutex_lock(&list->lock);
  node->next = list->head;
  list->head = node;
  list->size +=1;
  pthread_cond_signal(&list->modified_cv);
  pthread_mutex_unlock(&list->lock);
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
void* queue_take(struct queue_head* queue, int n) {
  int i = 0;

  pthread_mutex_lock(&queue->lock);


  while((queue->size < n)) {
    if(!queue->finish_filling) {
      pthread_cond_wait(&queue->modified_cv,&queue->lock);
    }
    if(queue->finish_filling && queue->size < n) {
      pthread_mutex_unlock(&queue->lock);
      return NULL;
    }
  }

  struct queue * cur = queue->head; 
  int first_size = cur->data_len; 

  // Assumes items fit in n*first_size
  void* retval = malloc(n*first_size);

  while(i < n && cur!=NULL) { // what about the previous cur check
    int item_size = cur->data_len;
    memcpy(retval+(i*item_size),cur->data,item_size);
    struct queue * next = cur->next;
    free(cur);
    cur = next;
    i++;
  }

  queue->head = cur;
  queue->size = queue->size - n;
  pthread_mutex_unlock(&queue->lock);

  return retval;
}
