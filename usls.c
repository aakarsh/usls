#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 *   Adapted from ALS.
 */

const char* program_name;

void print_usage(FILE* stream, int exit_code){
  fprintf(stream,"Usage: %s  <options> [input file] \n",program_name);
  fprintf(stream,
          "-h    --help    Display this usage information\n"
          "-s    --stat    Display detailed file status information\n"
          "-v    --verbose Print verbose output \n");
  exit(exit_code);
}

int file_status(int optind,char* argv[]){

  const char* file_name = argv[optind];
  
  printf("Working on file %s \n",file_name);

  struct stat sb ;
  int ret = stat(file_name,&sb);
  if(ret){
    perror("stat");
    return 1;    
  }
  const char* desc = 
     "File Name: %s \n"
     "Device Id: %ld\n"
     "Inode Number: %ld\n"
     "Mode: %ld\n"
     "HardLinks: %ld\n"
     "User Id: %ld\n"
     "Group Id : %ld\n"
     "Device Id : %ld\n" 
     "Size : %ld bytes\n"
     "Block Size : %ld bytes\n"
     "Blocks Allocated : %ld \n"
     "Last Access Time : %ld \n"
     "Last Modified Time : %ld \n"
     "Last Status Time : %ld \n\n";
    
  printf(desc,
         file_name,
         sb.st_dev,
         sb.st_ino,
         sb.st_mode,
         sb.st_nlink,
         sb.st_uid,
         sb.st_gid,
         sb.st_dev,
         sb.st_size,
         sb.st_blksize,
         sb.st_blocks,
         sb.st_atime,
         sb.st_mtime,
         sb.st_ctime);
  return 0;
}


int main(int argc,char * argv[]){

  program_name = argv[0];

  int verbose = 0;
  int next_opt = 0;
  const char* short_options = "shv";

  const struct option long_options[] = {
    {"help",0,NULL,'h'},
    {"status",0,NULL,'s'},
    {"verbose",1,NULL,'v'}
  };

  do{    
    next_opt = getopt_long(argc,argv,
                           short_options,long_options, NULL);
    extern int optind;
    switch(next_opt){
    case 'h':
      print_usage(stdout,0);      
      break;
    case 's':
      if(optind > argc){
        print_usage(stderr,1);
      }
      file_status(optind,argv);
      break;
    case 'v':
      verbose = 1 ;
      break;
    case '?': // user specified invalid option
      print_usage(stderr,1);
    case -1:
      break;
    default:
      printf("unexpected exit");
      abort();
    }
  } while (next_opt != -1);
  


  
  
  return 0;
}
