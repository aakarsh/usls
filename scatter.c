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

#define FREE_QUEUE_SIZE 60000
#define MAX_SEARCH_TERM_LEN 1024
#define IOVEC_LEN 4096

// Free from which blocks are taken and read into from files
struct queue_head* free_iovec_queue = NULL;

// List on which search is to conducted
struct queue_head* search_queue = NULL;

int search_queue_add(char* file, struct queue_head* search_queue);
struct queue_head* create_iovec_queue(int nblks ,int block_size);


struct queue_head* create_iovec_queue(int nblks ,int block_size) {
  struct queue_head* q = queue_init(NULL);  
  int i;

	// Create list of empty iovec's
	struct iovec* vec = malloc(nblks*sizeof(struct iovec));
  for(i = 0 ; i < nblks;  i++) {
    vec[i].iov_base = malloc(block_size);
    vec[i].iov_len = block_size;
  }

	queue_prepend_all(q,vec,sizeof(struct iovec),nblks);
  return q; 
}

void search_buffer (int thread_id, const char* file_name, 
                    const char* search_term, int buf_num, 
                    struct iovec* buffer)
{

  void* buf_base = buffer->iov_base;
  int   buf_len = buffer->iov_len;

  int   buf_end_pos = buf_len-1;
  int   buf_file_pos = buffer->iov_len*buf_num;
  
  void* match_idx = NULL;
  int offset = 0;

  while ( offset < buf_end_pos
          && (match_idx = memmem(buf_base+offset,buf_len-offset,search_term,strlen(search_term))) != NULL) {

    // Found a match
    int match_pos  =  (match_idx - buf_base );
    // offset right after match
    offset = match_pos+1;

    //find end of line
    int line_end_pos = buf_end_pos;
    void* line_end = memchr(match_idx,'\n',buf_len-match_pos);

    if(line_end != NULL)
      line_end_pos = line_end - buf_base -1;

    //find beginning of line
    int line_begin_pos = 0;
    void* line_begin = memrchr(buf_base,'\n',match_pos-1);

    if(line_begin!=NULL){ // no begining of line found
      line_begin_pos = line_begin - buf_base+1;
    }
		
    int line_length = line_end_pos - line_begin_pos + 1;

    int file_pos =  buf_file_pos + match_pos;

    if(line_length < 0) {
      fprintf(stderr, "search_buffer WARNING negative line length  buffer length %d match_pos %d \n", buf_len, match_pos);
    }

    fprintf(stderr,"search_buffers line [%p %p] [ %d -  %d] length %d\n"
            ,line_begin, line_end,
            line_begin_pos, line_end_pos, line_length);

    char out[line_length+1];
    memcpy(&out, buf_base+line_begin_pos, line_length);
    out[line_length]='\0';

    fprintf(stdout,"[T:%d] match: [%d]%s: %d: [%s]  \n", thread_id,buf_num,file_name,file_pos,out);

  }
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
      fprintf(stderr,"Stopping reader %d end\n",targ->index);
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
      fprintf(stderr,"Stopping %d end\n",targ->index);
      return NULL;
    }

    search_buffer(targ->index,"",targ->search_term,0,buffer);

    // After we have finished searching we return this iovec back to 
    // list free iovec 
    struct queue* free_node = queue_create_node(buffer,sizeof(struct iovec));
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

	// we prepend the bare iovec
	// TODO Add file meta data
	// File name
	// Block number of the iovec
  queue_prepend_all(search_queue,buffers,sizeof(struct iovec),iovecs_to_read); 

  fprintf(stderr,"Read file [%s] \n%d bytes num iovecs %d \n",file,bytes_read,iovecs_to_read);
  close(fd);

  return bytes_read;
}

int main(int argc,char * argv[])
{
  int num_processors = 1;

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

  int num_readers = 1;
  int num_searchers = 3 * num_processors;

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

  pthread_t searcher_id[num_searchers];
  struct searcher_thread_args searcher_args[num_searchers];

  for(i = 0 ; i < num_searchers; i++){
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
  for(i = 0 ; i < num_searchers; i++){
    pthread_join(searcher_id[i],NULL);
  }

  return ret;
}
