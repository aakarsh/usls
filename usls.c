#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

/**
 *   Adapted from ALS.
 */

const char* program_name;

void print_usage_cmd(FILE* stream, int exit_code);
int file_status_cmd(const char* file_name );
int list_directory_cmd(const char* dir);
struct fileinfo* create_fileinfo(char* dir,struct dirent* entry) ;

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
  char* path;
  enum filetype type;
  size_t size;
  ino_t inode_num;
};

int fi_cmp_name(const void * i1, const void* i2){
   struct fileinfo** fi1 = (struct fileinfo**)i1;
   struct fileinfo** fi2 = (struct fileinfo**)i2;
   return strcmp((*fi1)->name,(*fi2)->name);
}

int fi_cmp_size(const void * i1, const void* i2){
   struct fileinfo** fi1 = (struct fileinfo**)i1;
   struct fileinfo** fi2 = (struct fileinfo**)i2;
   return (*fi1)->size > (*fi2)->size;
}

int list_directory_cmd(const char* pwd) {

  printf("%s :\n",pwd);  
  DIR* dir = opendir(pwd);
  struct dirent * entry;  

  int num_entries = 0;
  int alloc_entries = 100;

  struct fileinfo** files = malloc(alloc_entries*sizeof(struct fileinfo *));

  while (NULL!= (entry = readdir(dir))) {    
    files[num_entries++] = create_fileinfo(pwd,entry);

    if(num_entries>alloc_entries){
      files = realloc(files,alloc_entries);
      alloc_entries*=2;
    }
  }

  qsort(files,num_entries,sizeof(struct fileinfo*),  &fi_cmp_name);

  int i;
  for( i =0 ; i < num_entries; i++){
    print_file_info(i==0,files[i]);
    free(files[i]);
  }


  closedir(dir);
  return 0;
}

void file_type_desc(char** s,enum filetype type) {
  const char* desc;
    switch(type){
    case blockdev:
      desc = "Block";
      break;
    case chardev:
      desc = "Char";
      break;
    case normal:
      desc = "-";
      break;
    case directory:
      desc = "d";
      break;
    case fifo:
      desc = "FIFO ";
      break;
    case sock:
      desc = "SOCK";
      break;
   default:
     desc = "-";
     break;    
    }
    *s = strdup(desc);
}

void print_file_info(int heading,struct fileinfo* fi){
    if(heading){
      printf("%6s\t%10s\t%10s%10s\t\n","Type","Inode#","Size","Name");
    }
    char* desc;
    file_type_desc(&desc,fi->type);
    printf("%6s\t",desc);
    free(desc);
    printf("%10ld\t",fi->inode_num);
    printf("%10ld\t",fi->size);
    printf("%10.10s",fi->name);
    printf("\n");  
}

struct fileinfo* create_fileinfo(char* dir_path,struct dirent* entry) {
  struct fileinfo* fi = malloc(sizeof(struct fileinfo));
  fi->name = strdup(entry->d_name);
  asprintf(&fi->path,"%s/%s",dir_path,entry->d_name);

  struct stat sb ;
  int ret = stat(fi->path,&sb);

  if(ret){
    perror("stat");
    return NULL;    
  }
  
  fi->size = sb.st_size;
  fi->inode_num = sb.st_ino;

  enum filetype ft;

  switch(entry->d_type){
  case DT_BLK:
    ft = blockdev;
    break;
  case DT_CHR:
    ft = chardev;
    break;
  case DT_REG:
    ft = normal;
    break;
  case DT_DIR:
    ft = directory;
    break;
  case DT_FIFO:
    ft = fifo;
    break;
  case DT_SOCK:
    ft = sock;
    break;
  default:
    ft = unknown;
    break;    
  }
  fi->type = ft;

  return fi;
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
