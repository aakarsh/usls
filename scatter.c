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

#include <sys/types.h>
#include <dirent.h>

#include "queue.h" 

#define FREE_QUEUE_SIZE 30000
#define MAX_SEARCH_TERM_LEN 1024
#define IOVEC_LEN 4096
#define MAX_FILE_NAME 2048


// Free from which blocks are taken and read into from files
struct queue_head* free_iovec_queue = NULL;

// List on which search is to conducted
// Metadata of iovec we want to search
struct search_queue_node {
  char file_name[MAX_FILE_NAME]; 
  int file_name_len;
  int iovec_num;
  struct iovec* vec;  
};

struct queue_head* search_queue = NULL;

int search_queue_add(char* file, struct queue_head* search_queue);
struct queue_head* create_iovec_queue(int nblks ,int block_size);


struct queue_head* create_iovec_queue(int nblks ,int block_size) {

  struct queue_head* q = queue_new();  

  // Create list of empty iovec's
  struct search_queue_node* sqn = malloc(nblks*sizeof(struct search_queue_node));
  struct iovec* vec = malloc(nblks*sizeof(struct iovec));
  
  int i;
  for(i = 0 ; i < nblks;  i++) {
    vec[i].iov_base = malloc(block_size);
    vec[i].iov_len = block_size;
    sqn[i].vec = vec + i;
  }
  
  queue_prepend_all(q,sqn,sizeof(struct search_queue_node),nblks);
  return q; 
}

void search_buffer (int thread_id, const char* file_name, 
                    const char* query, int buf_num, 
                    struct iovec* buffer)
{

  fprintf(stderr,"search_buffer %d : %s %s %d \n"
          ,thread_id,file_name,query,buf_num);

  void* buf_base = buffer->iov_base;
  int   buf_len = buffer->iov_len;

  int   buf_end_pos = buf_len-1;
  int   buf_file_pos = buffer->iov_len*buf_num;
  
  void* match_idx = NULL;
  int offset = 0;
  int loop_count = 0;
  int lquery = strlen(query);

  while ( offset < buf_end_pos
          && (match_idx = memmem(buf_base+offset, /* haystack start */
                                 buf_len-offset,  /* haystack len   */
                                 query,
                                 lquery)) != NULL) {

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

    int match_file_pos =  buf_file_pos + match_pos;

    if(line_length < 0) {
      fprintf(stderr, "search_buffer WARNING negative line length  buffer length %d match_pos %d \n", buf_len, match_pos);
    }

    fprintf(stderr,"[T %d][loop %d][buf %d]search_buffers line offset %d [%p %p] [ %d -  %d] length %d\n"
            ,thread_id
            ,loop_count
            ,buf_num
            ,offset,line_begin, line_end,
            line_begin_pos, line_end_pos, line_length);
    

    char match_string[line_length+1];
    memcpy(&match_string, buf_base+line_begin_pos, line_length);
    match_string[line_length]='\0';   

    //fprintf(stderr,"[T:%d] match: [%d]%s: %d: [%s]  \n", thread_id,buf_num,file_name,match_file_pos,match_string);
    //TODO seeing duplicates in results
    fprintf(stdout,"%s:%d:%s\n",file_name,match_file_pos,match_string);

    loop_count++;

  }
}

struct queue* file_reader_tranform(void* obj, int id,void* priv, struct queue_head* out_q) {
  char* file = (char*)  obj;
  search_queue_add(file,out_q);
  return NULL;
}

struct queue* search_transform(void* obj, int id, void* priv,struct queue_head* oq) {
  struct search_queue_node* sqn = (struct search_queue_node*) obj;
  char*  search_term = (char*) priv;
  fprintf(stderr,"Call search_transform [%s] on file %s \n",
          search_term,sqn->file_name);

  search_buffer(id,sqn->file_name,search_term,sqn->iovec_num,sqn->vec);
  // After we have finished searching we return this iovec back to 
  // For safety we make sure to zero out the data before returning to free list
  memset(sqn->file_name, 0, MAX_FILE_NAME);
  memset(sqn->vec->iov_base, 0, sqn->vec->iov_len);
  struct queue* free_node = queue_create_node(sqn,sizeof(struct search_queue_node));
  fprintf(stderr,"Returning free node \n");

  return free_node;
}



/**
 * Reads a file and appends it to the search list
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

  struct search_queue_node*  sqn = (struct search_queue_node*) queue_take(free_iovec_queue, iovecs_to_read);
  if(sqn == NULL){    
    return 0;
  }

  struct iovec* buffers[iovecs_to_read];

  int i = 0;
  for(i = 0; i < iovecs_to_read; i++){
    buffers[i] = sqn[i].vec;    
  }

  // gather all files blocks that can be read into buffers
  int bytes_read = readv(fd,*buffers,iovecs_to_read);

  for(i = 0; i < iovecs_to_read; i++){
    strncpy(sqn[i].file_name,file,MAX_FILE_NAME);
    sqn[i].file_name_len = strlen(file);
    sqn[i].iovec_num = i;
    sqn[i].vec = buffers[i];
  }

  queue_prepend_all(search_queue, sqn ,sizeof(struct search_queue_node),iovecs_to_read);    

  fprintf(stderr,"Read file [%s] \n%d bytes num iovecs %d \n",file,bytes_read,iovecs_to_read);
  close(fd);

  return bytes_read;
}

void recursive_add_files(char* dir_path, struct queue_head* file_queue){
  fprintf(stderr,"Reading directory path %s\n",dir_path);
  DIR* dir = opendir(dir_path);
  
  struct dirent * f;
  while((f = readdir(dir))!=NULL){
    if(strcmp(".",f->d_name) == 0 || strcmp("..",f->d_name) == 0 || f->d_name[0] == '.')
      continue;

    char path[MAX_FILE_NAME];
    int len = snprintf(path,sizeof(path)-1,"%s/%s",dir_path,f->d_name);
    path[len]='\0';

    if(f->d_type == DT_DIR) {
      recursive_add_files(path,file_queue);
      continue;
    }

    if(!DT_REG == f->d_type){
      continue;   
    }

    char* file_path = strndup(path,MAX_FILE_NAME);
    struct queue* node = queue_create_node(file_path,strlen(file_path)+1);    
    queue_prepend(file_queue,node);
  }

  closedir(dir);  

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
  int num_searchers = 3;

  // Initialize Free Queue
  free_iovec_queue = create_iovec_queue(FREE_QUEUE_SIZE,IOVEC_LEN);

  // Initiazlie a Search Queue
  search_queue = queue_new();

  struct queue_head* file_queue = queue_new();

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
    struct stat file_info;
    if(stat(argv[2],&file_info)!=0) {
      fprintf(stderr, "Couldnt locate file %s\n",argv[2]);
      perror("fstat");
      return -1;
    }

    if(S_ISDIR(file_info.st_mode)){
      recursive_add_files(argv[2],file_queue);
    } else {    //regular file
      struct queue* node = queue_create_node(argv[2],strlen(argv[2])+1);
      queue_prepend(file_queue,node);
    }
  }

  // we are done filling this queue
  queue_mark_finish_filling(file_queue);

  pthread_t* reader_id = 
    start_tranformers("reader",file_reader_tranform,NULL,
                      file_queue,search_queue,num_readers);

  pthread_t* searcher_id = 
    start_tranformers("searcher",search_transform,argv[1],
                      search_queue,free_iovec_queue,num_searchers);
  
  fprintf(stderr,"Waiting for readers\n");
  join_transformers(reader_id,num_readers);

  /**
   * Once reader has ended we need to go ahead and cancel on searcher threads
   * which are waiting for data.
   */
  fprintf(stderr, "Finished running reader \n");

  queue_mark_finish_filling(search_queue);
  
  fprintf(stderr,"Waiting for searchers");
  join_transformers(searcher_id,num_searchers);

  //queue_destroy(search_queue);
  //queue_destroy(free_iovec_queue);
  //free(searcher_id);

  return 0;
}
