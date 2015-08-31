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


#define MAX_SEARCH_TERM_LEN 1024
#define IOVEC_LEN 4096

struct iovec_list {
    struct iovec* vec;
    struct iovec_list *next;
	  void* priv; // some cstom data;
};

struct iovec_list_head {
	struct iovec_list* head;
	int size;
	int finish_filling;
	pthread_cond_t modified_cv;
	pthread_mutex_t modified_mutex;
	pthread_mutex_t lock;
};

struct iovec_list_head* free_list = NULL;

// List of iovec's ready to be searched
struct iovec_list_head* search_list = NULL;

int search_list_add(char* file, struct iovec_list_head* search_list);


struct iovec_list_head* init_iovec_list_head(struct iovec_list_head** list) {
	struct iovec_list_head* new_list =	(struct iovec_list_head*) malloc(sizeof(struct iovec_list_head));

	if(list!=NULL){
		*list = new_list;
	}
	(new_list)->head = NULL;
	(new_list)->size = 0;
	(new_list)->finish_filling = 0;
	pthread_mutex_init(&(new_list)->lock, NULL);
	pthread_cond_init(&(new_list)->modified_cv, NULL);
	pthread_mutex_init(&(new_list)->modified_mutex, NULL);
	return new_list;	
}

void mark_finish_filling(struct iovec_list_head* list){
	pthread_mutex_lock(&list->lock);
	list->finish_filling = 1;
	pthread_cond_broadcast(&list->modified_cv);
	pthread_mutex_unlock(&list->lock);
}

/**
 * Adds a free'ed iovec_list node back into the free_list
 */
void iovec_list_prepend(struct iovec_list_head* list , struct iovec_list* node) {
	pthread_mutex_lock(&list->lock);
	node->next = list->head;
	list->head = node;
	list->size +=1;
	pthread_cond_signal(&list->modified_cv);
	pthread_mutex_unlock(&list->lock);
}

/**
 * Prepends
 */
void iovec_list_prepend_all(struct iovec_list_head* list , struct iovec* node, int nvecs) {
	int i = 0;
	pthread_mutex_lock(&list->lock);
	struct iovec_list* prev = NULL;
	struct iovec_list* head = NULL;
	for(i = 0; i < nvecs; i++) {
		struct iovec_list* list_node = malloc(sizeof(struct iovec_list));
		list_node->vec = &node[i];
		if(prev == NULL) {
			head = list_node;
		} else {
			prev->next = list_node;
		}
		prev = list_node;
	}
	list->head = head;

	list->size = list->size+ nvecs;
	pthread_cond_signal(&list->modified_cv);
	pthread_mutex_unlock(&list->lock);
}

struct iovec* iovec_list_take_one(struct iovec_list_head* list){
	struct iovec * retval = NULL;
	retval = malloc(sizeof(struct iovec));
	pthread_mutex_lock(&list->modified_mutex);
	while ((list->size) <= 0) {
		if(!list->finish_filling) {
			pthread_cond_wait (&list->modified_cv, &list->modified_mutex);
		}
		// woke up from sleep to find there is no more work to be done
		// return poison packet
		if(list->finish_filling) {
			pthread_mutex_unlock(&list->modified_mutex);
			return NULL;
		}

	}

	// Acquire the list lock knowing the flag is set
	pthread_mutex_lock(&list->lock);

	// Free the condistional variable mutex
	pthread_mutex_unlock(&list->modified_mutex);

	struct iovec_list * cur = list->head;
	retval->iov_base = cur->vec->iov_base;
	retval->iov_len = cur->vec->iov_len;

	struct iovec_list * next = cur->next;
	free(cur);
	list->head = next;
	list->size = list->size-1;
	pthread_mutex_unlock(&list->lock);
	return retval;
}

//TODO need to wait for queue to get elements once exhausted
// TODO use conditional waits
struct iovec* iovec_list_take(struct iovec_list_head* list, int nbuffers){
	int i = 0;
	struct iovec * retval = NULL;
	pthread_mutex_lock(&list->lock);


	retval = malloc(nbuffers*sizeof(struct iovec));
	struct iovec_list * cur = list->head;

	while(i< nbuffers && cur!=NULL){
		retval[i].iov_base = cur->vec->iov_base;
		retval[i].iov_len = cur->vec->iov_len;
		//fprintf(stderr,"iovec_list_take:adding %d \n",retval[i].iov_len);

		struct iovec_list * next = cur->next;
		free(cur);
		cur = next;
		i++;
	}
	list->head = cur;
	list->size = list->size-nbuffers;
	pthread_mutex_unlock(&list->lock);
	//assuming i can get nbuffers
	return retval;
}

/** 
 * Creates an iovec_list
 */
struct iovec_list_head* create_iovec_list(int nblks ,int block_size) {
  int i;	
	struct iovec_list_head* list_head = init_iovec_list_head(NULL);
	list_head->size = nblks;

	struct iovec_list* head = NULL; 
	struct iovec_list* prev = NULL;

  for( i = 0; i < nblks; i++) {
		struct iovec_list* node = malloc(sizeof(struct iovec_list));
		struct iovec* vec = malloc(sizeof(struct iovec));
		vec->iov_base = malloc(block_size);
		vec->iov_len = block_size;
		
		node->vec = vec;
		node->next = NULL;

		if(prev == NULL) { // head node
			head = node;
		} else {
			prev->next = node;
		}
		prev = node;
  }
	
	list_head->head = head;
	return list_head;
}

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

	fprintf(stdout,"[tid %d ] match: [%d]%s: %d: [%s]  \n", thread_id,buf_num,file_name,overall_pos,out);
}


void search_buffers(const char* file_name,const char* search_term,
										struct iovec* buffers,int nbuffers)
{
  int i=0;
  for(i = 0; i < nbuffers; i++){
		void* index = memmem(buffers[i].iov_base,buffers[i].iov_len,
												 search_term,strlen(search_term));

		if(index != NULL){
			int pos  = (index - buffers[i].iov_base );
			int overall_pos = buffers[i].iov_len*i + pos;
			char out[1024];
			int remndr_bytes =(buffers[i].iov_base+buffers[i].iov_len) - index;
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
			fprintf(stdout,"match: [%d]%s: %d: [%s]  \n", i,file_name,overall_pos,out);
		}
	}
}

struct search_buffers_arg {
	char* file_name;
	char* search_term;
	struct iovec* buffers;
	int nbuffers;
	int nthreads;
	int thread_index;
	
};

struct file_reader_thread_args {
	char* file;
	struct iovec_list_head* search_list;
};



void* file_reader_thread_start(void* arg){
	struct file_reader_thread_args* targ  = (struct file_reader_thread_args*) arg;
	search_list_add(targ->file,targ->search_list);
  return NULL;
}


struct searcher_thread_args { 
	char* search_term;
	struct iovec_list_head* search_list;
	int index;
};

void* searcher_thread_start(void* arg){
	struct searcher_thread_args* targ  = (struct searcher_thread_args*) arg;

	fprintf(stderr,"searcher %d thread started! \n",targ->index);

	while(1) {
		struct iovec* buffer = iovec_list_take_one(targ->search_list);
		if(buffer == NULL){ // done
			printf("Stopping %d end\n",targ->index);
			return NULL;
		}		//		fprintf(stderr,"searcher %d found buffer! \n",targ->index);
		search_buffer(targ->index,"",targ->search_term,0,buffer);
	}

	
	return NULL;
}

void* thread_search_buffers(void* arg)
{
	struct search_buffers_arg* targ  = (struct search_buffers_arg*) arg;
	fprintf(stderr,"Started thread %d\n",targ->thread_index);
	int i = 0;
	for(i=targ->thread_index; i < targ->nbuffers; i+= targ->nthreads){
		search_buffers(targ->file_name,targ->search_term,&targ->buffers[i],1);
	}      
	return NULL;
}

/**
 * Reads a file and appends it to the search list
 */
int search_list_add(char* file, struct iovec_list_head* search_list) {
  int fd = open(file, O_RDONLY);
  if(fd < 0){
    fprintf(stderr,"Couldn't open file %s \n", file);
    return -1;
  }
  struct stat file_info;

  int ret = fstat(fd,&file_info);
  if(ret){
    fprintf(stderr,"Couldnt stat file %s %d \n",file,ret);
    return -1;
  }

  int iovecs_to_read = (int)ceil((double)file_info.st_size/(1.0*IOVEC_LEN));
	struct iovec * buffers =	iovec_list_take(free_list,iovecs_to_read);

  int total_bytes = file_info.st_size;  
	// gather all files blocks that can be read into buffers
  int bytes_read = readv(fd,buffers,iovecs_to_read);


	iovec_list_prepend_all(search_list,buffers,iovecs_to_read);

  fprintf(stderr,"Read %d bytes num iovecs %d \n",bytes_read,iovecs_to_read);

	return bytes_read;
}


int search_file(char* file, char* search_term,int num_threads) {

  int fd = open(file, O_RDONLY);
  if(fd < 0){
    fprintf(stderr,"Couldn't open file %s \n", file);
    return -1;
  }


  struct stat file_info;
  int ret = fstat(fd,&file_info);
  if(ret){
    fprintf(stderr,"Couldnt stat file %s %d \n",file,ret);
    return -1;
  }	    

  fprintf(stderr,"Creating %d threads to search for %s in %s file size %d\n",
					num_threads,search_term,file,(int)file_info.st_size);

  int iovecs_to_read = (int)ceil((double)file_info.st_size/(1.0*IOVEC_LEN));
	struct iovec * buffers =	iovec_list_take(free_list,iovecs_to_read);

  int total_bytes = file_info.st_size;  
  int bytes_read = readv(fd,buffers,iovecs_to_read);
	
  fprintf(stderr,"Read %d bytes num iovecs %d \n",bytes_read,iovecs_to_read);

  pthread_t tid[num_threads];
  struct search_buffers_arg thread_args[num_threads];
	int i;
  for(i = 0 ; i < num_threads; i++) {     
		thread_args[i].thread_index =i;
		thread_args[i].file_name = file;
		thread_args[i].search_term = search_term;
		thread_args[i].buffers = buffers;
		thread_args[i].nbuffers = iovecs_to_read;
		thread_args[i].nthreads = num_threads;

		pthread_create(&tid[i],NULL,&thread_search_buffers,(void*)&thread_args[i]);

  }
  
  fprintf(stderr,"Done Creating %d threads \n",num_threads);

  for(i = 0; i < num_threads; i++) {
		pthread_join(tid[i],NULL);        
  }

  fprintf(stderr," Finished running threads \n");

	return 0;
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

	// create a free list 
	free_list = create_iovec_list(1000,IOVEC_LEN);

	// Initiazlie a search list
	init_iovec_list_head(&search_list);
	
  if((num_processors = sysconf(_SC_NPROCESSORS_ONLN)) < 0){
		fprintf(stderr,"Couldnt get number of processors  %d \n",errno);
		return -1;
  }

  int num_threads = num_processors*2;
	
	// start and run thread which will read from file and add to the search thread.
  pthread_t reader_id;
	struct file_reader_thread_args reader_args;
	reader_args.file = argv[2];
	reader_args.search_list = search_list;
	pthread_create(&reader_id,NULL,&file_reader_thread_start,(void*)&reader_args);

	int ret =  0;

	//	ret = search_file(argv[2],argv[1],num_threads);


  pthread_t searcher_id[4];
	struct searcher_thread_args searcher_args[4];
	int i = 0;
	for(i = 0 ; i < num_threads; i++){
		searcher_args[i].search_term = argv[1];
		searcher_args[i].search_list = search_list;
		searcher_args[i].index = i;
		pthread_create(&searcher_id[i],NULL,&searcher_thread_start,(void*)&(searcher_args[i]));
	}
	
	//wait for reader
	pthread_join(reader_id,NULL);     



  /**
	 * Once reader has ended we need to go ahead and cancel on searcher threads
	 * which are waiting for data.
	 */
  fprintf(stderr, "Finished running reader \n");
	mark_finish_filling(search_list);
	
	//wait for searchers
	for(i = 0 ; i < num_threads; i++){
		pthread_join(searcher_id[i],NULL);
	}




  return ret;
}
