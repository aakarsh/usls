/* -*- Mode: c; tab-width: 8; indent-tabs-mode: 1; c-basic-offset: 8; -*- */
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

  int fd = open(argv[2], O_RDONLY);
  if(fd < 0){
    fprintf(stderr,"Couldn't open file %s \n", argv[2]);
    return -1;
  }

  struct stat file_info;
  int ret = fstat(fd,&file_info);
  if(ret){
    fprintf(stderr,"Couldnt stat file %s %d \n",argv[2],ret);
    return -1;
  }	  
  
  if((num_processors = sysconf(_SC_NPROCESSORS_ONLN)) < 0){
   fprintf(stderr,"Couldnt get number of processors  %d \n",errno);
   return -1;
  }

  int nthreads = num_processors*2;
  fprintf(stderr,"Creating %d threads to search for %s in %s file size %d\n",nthreads,argv[1],argv[2],(int)file_info.st_size);

  #define IOVEC_LEN 4096

  int iovecs_to_read = (int)ceil((double)file_info.st_size/(1.0*IOVEC_LEN));
  struct iovec buffers[iovecs_to_read];

  int i = 0;
  for( i = 0; i < iovecs_to_read; i++) {
    buffers[i].iov_base = malloc(IOVEC_LEN);
    buffers[i].iov_len = IOVEC_LEN;
  }

  int bytes = readv(fd,buffers,iovecs_to_read);
  fprintf(stderr,"Read %d bytes num iovecs %d \n",bytes,iovecs_to_read);

  for(i = 0; i < iovecs_to_read; i++){
	void* index = memmem(buffers[i].iov_base,buffers[i].iov_len,
		argv[1],strlen(argv[1]));
	
      if(index != NULL){
	int pos  = (index - buffers[i].iov_base );
	int overall_pos = buffers[i].iov_len*i + pos;
	char out[1024];
	int remndr_bytes =(buffers[i].iov_base+buffers[i].iov_len) - index;
	int nbytes = remndr_bytes > sizeof(out)-1 ? sizeof(out)-1 : remndr_bytes;
	//fprintf(stderr,"copy %d bytes \n",nbytes);
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
	
	fprintf(stderr,"match: [%d]%s: %d: [%s]  \n", i,argv[2],overall_pos,out);
      }

   }


  return 0;
}
