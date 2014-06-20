#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

/**
 *   Adapted from ALS.
 */

const char* program_name;

void print_usage_cmd(FILE* stream, int exit_code);
int file_status_cmd(const char* file_name );
int list_directory_cmd(const char* dir);

int main(int argc,char * argv[]){

  program_name = argv[0];

  int verbose = 0;  
  int next_opt = 0;
  const char* short_options = "shvd";

  const struct option long_options[] = {
    {"help",0,NULL,'h'},
    {"status",0,NULL,'s'},
    {"list-dir",0,NULL,'d'},
    {"verbose",1,NULL,'v'}
  };

  do{    
    next_opt = getopt_long(argc,argv,
                           short_options,long_options, NULL);
    extern int optind;
    switch(next_opt){
    case 'h':
      print_usage_cmd(stdout,0);      
      break;
    case 's':
      if(optind > argc){
        print_usage_cmd(stderr,1);
      }
      const char* file_name = argv[optind];
      file_status_cmd(file_name);
      break;
    case 'v':
      verbose = 1 ;
      break;
    case 'd':
      if(optind >= argc) {
        char* dir  = get_current_dir_name();
        list_directory_cmd(dir);
        free(dir);
      }else{
        char* dir = argv[optind];
        list_directory_cmd(dir);
      }
      break;
    case '?': // user specified invalid option
      print_usage_cmd(stderr,1);
    case -1:
      break;
    default:
      printf("unexpected exit");
      abort();
    }
  } while (next_opt != -1);  


  return 0;
}

enum filetype{
    unknown,
    fifo,
    chardev,
    directory,
    blockdev,
    normal,
    symbolic_link,
    sock,
    whiteout,
    arg_directory
};

struct fileinfo 
{
  char* name;
  enum filetype type;
  
};


int list_directory_cmd(const char* pwd) {
  printf("%s :\n",pwd);
  DIR* dir = opendir(pwd);
  struct dirent * entry;  

  int first = 1;
  while (NULL!= (entry = readdir(dir))) {    

    if(first){
      printf("%6s\t%10s\t%10s\t\n","Type","Inode#","Name");
      first = 0;
    }

    const char* desc;
    switch(entry->d_type){
    case DT_BLK:
      desc = "Block";
      break;
    case DT_CHR:
      desc = "Char";
      break;
    case DT_REG:
      desc = "-";
      break;
    case DT_DIR:
      desc = "d";
      break;
    case DT_FIFO:
      desc = "FIFO ";
      break;
    case DT_SOCK:
      desc = "SOCK";
      break;
   default:
     desc = "-";
     break;    
    }
    printf("%6s\t",desc);
    printf("%10ld\t",entry->d_ino);
    printf("%10.10s",entry->d_name);

    printf("\n");
  }
  closedir(dir);
  return 0;
}

int file_status_cmd(const char* file_name ){
  
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
     "Last Status Time : %ld \n\n" ;
    
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




void print_usage_cmd(FILE* stream, int exit_code){
  fprintf(stream,"Usage: %s  <options> [input file] \n",program_name);
  fprintf(stream,
          "-h    --help    Display this usage information\n"
          "-s    --stat    Display detailed file status information\n"
          "-v    --verbose Print verbose output \n");
  exit(exit_code);
}
