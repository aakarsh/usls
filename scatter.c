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

#define FREE_QUEUE_SIZE 5000
#define MAX_SEARCH_TERM_LEN 1024
#define IOVEC_LEN 4096


struct queue {
  void* data; // your data
  int data_len;
  struct queue* next;
};


struct queue_head{
  struct queue* head;
  int size;
  int finish_filling;
  pthread_cond_t modified_cv;
  pthread_mutex_t lock;
};

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
 * Add array of iovecs to the given list
 */
void queue_prepend_all(struct queue_head* list , struct iovec* node, int nvecs) {

  //Convert array to queue
  struct queue* head = NULL;
  struct queue* prev = NULL;

  int i = 0;
  for(i = 0; i < nvecs; i++) {

    struct queue* list_node = malloc(sizeof(struct queue));
    list_node->data = &node[i];
    list_node->data_len = sizeof(struct iovec);

    if(prev == NULL) {
      head = list_node;
    } else {
      prev->next = list_node;
    }
    prev = list_node;
  }

  // Append new iovec list to list at its head
  pthread_mutex_lock(&list->lock);
  prev->next = list->head;
  list->head = head;
  list->size = list->size + nvecs;
  pthread_cond_signal(&list->modified_cv);
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

  //TODO:Assumes a non empty queue when it actually should block waiting
  //for itemes like take one does
  // TODO SEGMENTATION VIOLATION

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


struct queue_head* create_iovec_queue(int nblks ,int block_size) {
  struct queue_head* q = queue_init(NULL);  
  int i;
  for(i = 0 ; i < nblks;  i++) {
    struct iovec* vec = malloc(sizeof(struct iovec));
    vec->iov_base = malloc(block_size);
    vec->iov_len = block_size;
    struct queue * queue_node = queue_create_node(vec,sizeof(struct iovec));
    queue_prepend(q,queue_node);
  }
  return q; 
}


struct queue_head* free_iovec_queue = NULL;

struct queue_head* search_queue = NULL;

int search_queue_add(char* file, struct queue_head* search_queue);

//TODO this only find's a single instance ina  block
// We need to find an instance in each line 
void search_buffer (int thread_id,const char* file_name, const char* search_term, int buf_num, struct iovec* buffer)
{

  void* index = memmem(buffer->iov_base,buffer->iov_len,
                       search_term,strlen(search_term));
  if(index == NULL){
    return ;
  }

  int pos  = (index - buffer->iov_base );
  int overall_pos = buffer->iov_len*buf_num + pos;
  char out[1024];
  int remndr_bytes =(buffer->iov_base+buffer->iov_len) - index;
  int nbytes = remndr_bytes > sizeof(out)-1 ? sizeof(out)-1 : remndr_bytes;	
  memcpy(&out,index, nbytes);

  out[nbytes+1]='\0';

  int k=0;
  while(k < nbytes ){
    if(out[k] == '\n' || out[k] == '\r'){
      out[k]= '\0';
      break;
    }
    k++;
  } 

  fprintf(stdout,"[T:%d] match: [%d]%s: %d: [%s]  \n", thread_id,buf_num,file_name,overall_pos,out);
}


struct file_reader_thread_args {
  int index ;
  struct queue_head* file_queue;
  struct queue_head* search_queue;
};


//TODO: Very similar to searcher queue need to simplyfy
void* file_reader_thread_start(void* arg) {

  struct file_reader_thread_args* targ  = (struct file_reader_thread_args*) arg;

  while(1) {
    char* file = queue_take(targ->file_queue,1);
    if(file == NULL){ // done
      printf("Stopping reader %d end\n",targ->index);
      return NULL;
    } 
    search_queue_add(file,targ->search_queue);
  }

  return NULL;
}


struct searcher_thread_args { 
  char* search_term;
  struct queue_head* search_queue;
  int index;
};

void* searcher_thread_start(void* arg){
  struct searcher_thread_args* targ  = (struct searcher_thread_args*) arg;

  fprintf(stderr,"searcher %d thread started! \n",targ->index);

  while(1) {
    struct iovec* buffer = queue_take(targ->search_queue,1);
    if(buffer == NULL){ // done
      printf("Stopping %d end\n",targ->index);
      return NULL;
    }   //    fprintf(stderr,"searcher %d found buffer! \n",targ->index);
    search_buffer(targ->index,"",targ->search_term,0,buffer);

    // After we have finished searching we return this iovec back to 
    // list free iovec 
    struct queue* free_node =queue_create_node(buffer,sizeof(struct iovec));
    queue_prepend(free_iovec_queue,free_node);
  }
  
  return NULL;
}


/**
 * Reads a file and appends it to the search list
 * 
 * TODO: Add buffers that have been searched back to the free list
 * TODO: Doesnt handle empty queue handle running out of  
 */
int search_queue_add(char* file, struct queue_head* search_queue) {
  int fd = open(file, O_RDONLY);
  if(fd < 0){
    fprintf(stderr,"Couldn't open file [%s] \n", file);
    return -1;
  }
  struct stat file_info;

  int ret = fstat(fd,&file_info);
  if(ret){
    fprintf(stderr,"Couldnt stat file %s %d \n",file,ret);
    return -1;
  }

  int total_bytes = file_info.st_size;  
  int iovecs_to_read = (int)ceil((double)total_bytes/(1.0*IOVEC_LEN));

  struct iovec*  buffers = (struct iovec*) queue_take(free_iovec_queue, iovecs_to_read);
	if(buffers == NULL){		
		return 0;
	}

  // gather all files blocks that can be read into buffers
  int bytes_read = readv(fd,buffers,iovecs_to_read);

  queue_prepend_all(search_queue,buffers,iovecs_to_read); 

  fprintf(stderr,"Read file [%s] \n%d bytes num iovecs %d \n",file,bytes_read,iovecs_to_read);
  close(fd);

  return bytes_read;
}


int main(int argc,char * argv[])
{
  int num_processors = 0 ;

  if(argc < 3) {
    fprintf(stderr, "Usage: %s [search_term] [file_name] \n",argv[0]);
    return -1;
  }

  if(strlen(argv[1]) > MAX_SEARCH_TERM_LEN) {
    fprintf(stderr,"search term %s is longer than %d \n",argv[1],MAX_SEARCH_TERM_LEN);
    return -1;
  }

  if((num_processors = sysconf(_SC_NPROCESSORS_ONLN)) < 0){
    fprintf(stderr,"Couldnt get number of processors  %d \n",errno);
    return -1;
  }

  int num_threads = num_processors*2;

  int num_readers = num_processors;
  int num_searchers = num_processors;


  // Initialize Free Queue
  free_iovec_queue = create_iovec_queue(FREE_QUEUE_SIZE,IOVEC_LEN);

  // Initiazlie a Search Queue
  search_queue = queue_init(NULL);


  struct queue_head* file_queue = queue_init(NULL);

  // add files to file_queue
  if(strcmp(argv[2],"-") == 0) { // read a list of files from stdin
    char* filename = NULL;
    size_t sz = 1024;
    while(getline(&filename,&sz,stdin) > 0 ) {      
      int l = strlen(filename);
      //  printf("file [%s] last char[%c]\n",filename,filename[l-1]);
      //Remove new line
      if(filename[l-1] == '\n') {
        filename[l-1] = '\0';
      }
      char* f = strdup(filename);
      int fl = strlen(f);
      struct queue* node = queue_create_node(f,fl+1);
      queue_prepend(file_queue,node);
    }
  } else{
    struct queue* node = queue_create_node(argv[2],strlen(argv[2]));
    queue_prepend(file_queue,node);
  }
  // we are done filling this queue
  queue_mark_finish_filling(file_queue);

  // Start and run thread which will read from file and add data to search queue
  pthread_t reader_id[num_readers];
  struct file_reader_thread_args reader_args[num_readers];
  //  reader_args.file = argv[2];
  int i;
  for(i = 0 ; i < num_readers; i++) {
    reader_args[i].index = 0;
    reader_args[i].search_queue = search_queue;
    reader_args[i].file_queue = file_queue;
    pthread_create(&reader_id[i],NULL,&file_reader_thread_start,(void*)&reader_args[i]);
  }

  int ret =  0;

  pthread_t searcher_id[num_threads];
  struct searcher_thread_args searcher_args[num_threads];

  for(i = 0 ; i < num_threads; i++){
    searcher_args[i].search_term = argv[1];
    searcher_args[i].search_queue = search_queue;
    searcher_args[i].index = i;
    pthread_create(&searcher_id[i],NULL,&searcher_thread_start,(void*)&(searcher_args[i]));
  }
  
  fprintf(stderr,"Waiting for readers\n");
  // Wait for reader
  for(i = 0; i < num_readers;i++)
    pthread_join(reader_id[i],NULL);     

  /**
   * Once reader has ended we need to go ahead and cancel on searcher threads
   * which are waiting for data.
   */
  fprintf(stderr, "Finished running reader \n");

  queue_mark_finish_filling(search_queue);
  
  fprintf(stderr,"Waiting for searchers");
  // Wait for searchers to finsih pending work and die
  for(i = 0 ; i < num_threads; i++){
    pthread_join(searcher_id[i],NULL);
  }

  return ret;
}
