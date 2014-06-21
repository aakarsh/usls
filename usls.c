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
#include <time.h>
#include <grp.h>
#include <pwd.h>

/**
 *   Adapted from ALS.
 */

const char* program_name;


enum ls_sort_by{ sort_by_filename , sort_by_size, sort_by_ctime };
enum ls_listing_type{ listing_type_simple, listing_type_long };

struct ls_config {
  enum ls_sort_by sort_by;
  int sorting_reverse;
  enum ls_listing_type listing_type;  
};


void print_usage_cmd(FILE* stream, int exit_code);
int file_status_cmd(const char* file_name );
int list_directory_cmd(const char* pwd, struct ls_config* config) ;

struct fileinfo* create_fileinfo(const char* dir,struct dirent* entry) ;
void print_fileinfo(int heading,struct fileinfo* fi);
void file_mode_string(mode_t md,char* str);

int main(int argc,char * argv[]){

  program_name = argv[0];

  int verbose = 0;  
  int next_opt = 0;

  struct ls_config config;

  const char* short_options = "shvl";

  const struct option long_options[] = {
    {"help",0,NULL,'h'},
    {"status",0,NULL,'s'},
    {"long",0,NULL,'l'},
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
    case 'l':
      config.listing_type = listing_type_long;
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


  char* dir;
  if(optind >= argc) {
    dir = get_current_dir_name();
  } else{
    dir = strdup(argv[optind]);
  }
  
  list_directory_cmd(dir,&config);

  free(dir);  

  return 0;
}

enum filetype { unknown,fifo,chardev,directory,blockdev,
              normal,symbolic_link,sock,whiteout,arg_directory 
};

struct fileinfo 
{
  char* name;
  char* path;
  enum filetype type;

  struct stat* stat;

};

int fi_cmp_name(const void * i1, const void* i2){
   struct fileinfo** fi1 = (struct fileinfo**)i1;
   struct fileinfo** fi2 = (struct fileinfo**)i2;
   return strcmp((*fi1)->name,(*fi2)->name);
}

int fi_cmp_size(const void * i1, const void* i2){
   struct fileinfo** fi1 = (struct fileinfo**)i1;
   struct fileinfo** fi2 = (struct fileinfo**)i2;
   return (*fi1)->stat->st_size > (*fi2)->stat->st_size;
}

int fi_cmp_type(const void * i1, const void* i2){
   struct fileinfo** fi1 = (struct fileinfo**)i1;
   struct fileinfo** fi2 = (struct fileinfo**)i2;
   return (*fi1)->type > (*fi2)->type;
}

int list_directory_cmd(const char* pwd, struct ls_config* config) {
  DIR* dir = opendir(pwd);

  if(NULL == dir){
    perror("opendir");
    return 1;
  }

  int num_entries = 0;
  int alloc_entries = 100;

  struct fileinfo** files = malloc(alloc_entries*sizeof(struct fileinfo *));

  struct dirent * entry;  

  while (NULL!= (entry = readdir(dir))) {    
    files[num_entries++] = create_fileinfo(pwd,entry);

    if(num_entries>alloc_entries){
      files = realloc(files,alloc_entries);
      alloc_entries*=2;
    }
  }

  qsort(files,num_entries,sizeof(struct fileinfo*), &fi_cmp_name);

  printf("total %d\n",num_entries);
  int i;
  for( i =0 ; i < num_entries; i++){
    print_fileinfo(i==0,files[i]);
    free(files[i]);
  }

  closedir(dir);
  return 0;
}

void filetype_sdesc(char** s,enum filetype type) {
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
void file_mode_string(mode_t md,char* str) {
  strcpy(str,"---------");
  if(md & S_IRUSR) str[0]='r';
  if(md & S_IWUSR) str[1]='w';
  if(md & S_IXUSR) str[2]='x';  

  if(md & S_IRGRP) str[3] ='r';
  if(md & S_IWGRP) str[4] ='w';
  if(md & S_IXGRP) str[5] ='x';

  if(md & S_IROTH) str[6] ='r';
  if(md & S_IWOTH) str[7] ='w';
  if(md & S_IXOTH) str[8] ='x';    
}

void print_fileinfo(int heading,struct fileinfo* fi){
    /* if(heading){
     *   printf("%6s\t%10s\t%10s%10s\t\n","Type","Inode#","Size","Name");
     * } */
    char* desc;
    filetype_sdesc(&desc,fi->type);
    printf("%1s",desc);
    free(desc);
    char mode_str[10];
    file_mode_string(fi->stat->st_mode,mode_str);
    printf("%9s ",mode_str);

    struct passwd* pw = getpwuid(fi->stat->st_uid);
    printf("%9s ",pw->pw_name);

    struct group* group_info =  getgrgid(fi->stat->st_gid);    
    printf("%9s ",group_info->gr_name);

    // For now
    //  printf("%10ld\t",fi->inode_num);

    printf("%8ld ",fi->stat->st_size);

    char mod_time[100];
    strftime(mod_time,100,"%b %d %H:%M",localtime(&(fi->stat->st_mtime)));
    printf("%9s ",mod_time);
    printf("%s",fi->name);


    printf("\n");  
}

struct fileinfo* create_fileinfo(const char* dir_path,struct dirent* entry) {
  struct fileinfo* fi = malloc(sizeof(struct fileinfo));
  fi->name = strdup(entry->d_name);
  asprintf(&fi->path,"%s/%s",dir_path,entry->d_name);

  struct stat* file_stat = malloc(sizeof(struct stat));

  int ret = stat(fi->path,file_stat);
  if(ret){
    perror("stat");
    return NULL;    
  }
  
  fi->stat = file_stat;
  /*
  fi->size = sb.st_size;
  fi->inode_num = sb.st_ino;
  fi->uid = sb.st_uid;     
  fi->gid = sb.st_gid;     
  fi->rdev = sb.st_rdev;    
  fi->blksize = sb.st_blksize; 
  fi->blocks = sb.st_blocks;  
  fi->atime = sb.st_atime;   
  fi->mtime = sb.st_mtime;   
  fi->mode = sb.st_mode;   
  fi->ctime = sb.st_ctime;   
  */
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
  int err = stat(file_name,&sb);
  if(err){
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
          "-l    --long    Display long list of information\n"
          "-v    --verbose Print verbose output \n");
  exit(exit_code);
}
