/* -*- Mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */

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

#define FREE_IOVEC_QUEUE_SIZE 8192
#define MAX_SEARCH_TERM_LEN 1024
#define IOVEC_BLOCK_SIZE 4096
#define MAX_FILE_NAME 2048


struct config cfg;

enum path_type {
  path_type_dir,
  path_type_file,
  path_type_stdin
};  

struct config 
{
  char* search_term;
  char* path;
  int num_readers;
  int num_searchers;
  int debug;
  int iovec_block_size;
  int iovec_queue_size;
  enum path_type path_type;
};

void config_init(struct config* cfg);

// List on which search is to conducted
// Metadata of iovec we want to search
struct search_queue_node {
  char file_name[MAX_FILE_NAME]; 
  int file_name_len;
  int iovec_num;
  struct iovec* vec;  
};


int search_queue_add(char* file, struct queue_head* free_iovec_queue, struct queue_head* search_queue) ;
struct queue_head* create_iovec_queue(int nblks ,int block_size);
void recursive_add_files(char* dir_path, struct queue_head* file_queue);

struct queue_head* create_iovec_queue(int nblks ,int block_size) {

  struct queue_head* q = queue_new();  

  // Create list of empty iovec's
  struct search_queue_node* sqn = malloc(nblks*sizeof(struct search_queue_node));
  struct iovec* vec = malloc(nblks*sizeof(struct iovec));  

  for(int i = 0 ; i < nblks;  i++) {
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

    fprintf(stderr,"MATCH : [T %d][loop %d][buf %d]search_buffers line offset %d [%p %p] [ %d -  %d] length %d\n"
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


struct queue* find_file_paths(void* obj, int id, void* priv, struct queue_head* in_q,struct queue_head* file_queue)
{

  char* read_from = priv;
  // add files to file_queue
  if(strcmp(read_from,"-") == 0) { // read a list of files from stdin
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
    if(stat(read_from,&file_info)!=0) {
      fprintf(stderr, "Couldnt locate file %s\n",read_from);
      perror("fstat");
      return NULL;
    }

    if(S_ISDIR(file_info.st_mode)){
      recursive_add_files(read_from,file_queue);
    } else {    //regular file
      struct queue* node = queue_create_node(read_from,strlen(read_from)+1);
      queue_prepend(file_queue,node);
    }
  }

  // we are done filling this queue
  queue_mark_finish_filling(file_queue);

  return NULL;
}

struct queue* file_reader_tranform(void* file, int id,void* priv, struct queue_head* in_q,struct queue_head* out_q) 
{
  struct queue_head* free_iovec_queue = priv;
  search_queue_add(file,free_iovec_queue,out_q);
  return NULL;
}

struct queue* search_transform(void* obj, int id, void* priv,struct queue_head* in_q,struct queue_head* oq) 
{  
  struct search_queue_node* sqn = obj;
  char*  search_term =  priv;

  fprintf(stderr,"Call search_transform [%s] on file %s \n",
          search_term,sqn->file_name);

  search_buffer(id,sqn->file_name,search_term,sqn->iovec_num,sqn->vec);
  return queue_create_node(sqn,sizeof(struct search_queue_node));  
}

/**
 * Reads a file and appends it to the search list
 */
int search_queue_add(char* file, struct queue_head* free_iovec_queue, struct queue_head* search_queue) {
  int bytes_read = 0;
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

  posix_fadvise(fd,0,0,POSIX_FADV_SEQUENTIAL|POSIX_FADV_NOREUSE);

  int total_bytes = file_info.st_size;  
  if(total_bytes == 0) {
    close(fd);
    return total_bytes;
  }

  int iovecs_to_read = (int)ceil((double)total_bytes/(1.0*cfg.iovec_block_size));

  struct queue*  assignable_nodes =  queue_take(free_iovec_queue, iovecs_to_read);

  struct search_queue_node*  sqn[iovecs_to_read];
  struct queue* cur = assignable_nodes;

  long long  i = 0;
  while(cur!=NULL){
    sqn[i] = assignable_nodes->data;
    strncpy(sqn[i]->file_name,file,MAX_FILE_NAME);
    sqn[i]->file_name_len = strlen(file);
    sqn[i]->iovec_num = i;

    cur = cur->next;
    i++;
  }
    
  if(sqn == NULL){    
    return 0;
  }  

  struct iovec buffers[iovecs_to_read];
  
  // Assign base address from nodes gotten from free list
  for(i = 0;i < iovecs_to_read; i++){
    // also populate list
    buffers[i].iov_base = sqn[i]->vec->iov_base;
    buffers[i].iov_len = sqn[i]->vec->iov_len;
  }
  
  // gather all files blocks that can be read into buffers
  bytes_read = readv(fd, buffers , iovecs_to_read);

  queue_prepend_all_list(search_queue, assignable_nodes);    

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



int get_num_processors(){
  int num_processors = 1;

  if((num_processors = sysconf(_SC_NPROCESSORS_ONLN)) < 0){
    fprintf(stderr,"Couldnt get number of processors assuming single processor  %d \n",errno);
    return 1;
  }  
  return num_processors;
}

void config_init(struct config* cfg){
  int num_processors = get_num_processors();

  cfg->search_term = NULL;
  cfg->path = NULL;
  cfg->num_searchers= num_processors;
  cfg->num_readers = num_processors;
  cfg->debug = 0; //default debug level
  cfg->iovec_block_size = IOVEC_BLOCK_SIZE;
  cfg->iovec_queue_size = FREE_IOVEC_QUEUE_SIZE;
  cfg->path_type = path_type_file;

}


char* program_name;

void usage(FILE* stream, int exit_code);


int main(int argc,char * argv[])
{
  program_name = argv[0];
  config_init(&cfg);

  const char* short_options = "r:s:D:b:q:";
  const struct option long_options[] = {
    {"num-readers",1,NULL,'r'},
    {"num-searchers",1,NULL,'s'},
    {"block-size",1,NULL,'b'},
    {"queue-size",1,NULL,'q'},
    {"debug",1,NULL,'D'}};

  int next_opt = false;
  extern int optind;
  extern char *optarg;
  do {
    next_opt = getopt_long(argc,argv,
                           short_options,long_options, NULL);

    switch(next_opt){
    case 'b':
      cfg.iovec_block_size = atoi(optarg);
      break;
    case 'q':
      cfg.iovec_queue_size = atoi(optarg);
      break;
    case 'r':
      cfg.num_readers = atoi(optarg);
      break;
    case 's':
      cfg.num_searchers = atoi(optarg);
      break;
    case 'D':
      cfg.debug = atoi(optarg);
      break;
    case '?': // usage
      usage(stderr,0);
    case -1:
      break;
    default:
      printf("unexpected exit");
      abort();
    }
  } while (next_opt != -1);  

  int remaining_args = argc - optind;
    
  if( remaining_args < 2) {
    fprintf(stderr, "Usage: %s [search_term] [file_name] \n",argv[0]);
    return -1;
  }
  
  char* search_term = argv[optind];
  char* read_from = argv[optind+1];

  if(strlen(search_term) > MAX_SEARCH_TERM_LEN) {
    fprintf(stderr,"search term %s is longer than %d \n",search_term,MAX_SEARCH_TERM_LEN);
    return -1;
  }

  // Initialize Free Queue
  // Free from which blocks are taken and read into from files
  struct queue_head* free_iovec_queue = NULL;
  free_iovec_queue = create_iovec_queue(cfg.iovec_queue_size,cfg.iovec_block_size);
  free_iovec_queue->free_data = free;

  // Queue containing file to be searched
  struct queue_head* file_queue = queue_new();

  // Queue used containing buffers to be searched
  struct queue_head* search_queue = queue_new();
  
  //find_file_paths
  struct transformer_info* file_finder = 
    start_tranformers("finder",find_file_paths,read_from,
                      NULL,file_queue, 1);

  struct transformer_info* readers = 
    start_tranformers("reader",file_reader_tranform,free_iovec_queue,
                      file_queue,search_queue,cfg.num_readers);

  struct transformer_info* searchers = 
    start_tranformers("searcher",search_transform,search_term,
                       search_queue,free_iovec_queue,cfg.num_searchers);
  
  join_transformers(file_finder);

  fprintf(stderr,"Waiting for readers\n");
  join_transformers(readers);

  /**
   * Once reader has ended we need to go ahead and cancel on searcher threads
   * which are waiting for data.
   */
  fprintf(stderr, "Finished running reader \n");

  queue_mark_finish_filling(search_queue);
  
  fprintf(stderr,"Waiting for searchers");
  join_transformers(searchers);
  
  free_transformers(searchers);
  free_transformers(readers);
  free_transformers(file_finder);

  queue_destroy(search_queue);
  queue_destroy(file_queue);
  queue_destroy(free_iovec_queue);

  return 0;
}


void usage(FILE* stream, int exit_code)
{
  fprintf(stream,"Usage: %s  [search_term] [file/dir] \n",program_name);

  struct description{
    char* short_option;
    char* long_option;
    char* description;
  };
  
  const struct description desc_arr[] = {
    {"-r[n]","--num-readers","Number of reader threads to use"},
    {"-s[n]","--num-searchers","Number of searcher threads to use"},
    {"-b[n]","--block-size","size of blocks to on search queue"},
    {"-q[n]","--queue-size","len free list  queue"},
    {"-D[n]","--debug","verbosity of debug logs"}
  };

  int i = 0;
  for(i = 0 ; i < 4;i++) {
    fprintf(stream,"%-10s%-14s\t%-10s\n"
            ,desc_arr[i].short_option
            ,desc_arr[i].long_option
            ,desc_arr[i].description);                
  }
  exit(exit_code);
}
