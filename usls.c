/* -*- Mode: c; tab-width: 8; indent-tabs-mode: 1; c-basic-offset: 8; -*- */
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <fnmatch.h>
#include <math.h>
#include <stdbool.h>

const char* program_name;

enum ls_sort_by
{
    sort_by_nosort,
    sort_by_filename, 
    sort_by_size, 
    sort_by_mtime
};

enum ls_listing_type
{
    listing_type_simple, 
    listing_type_long
};

enum ls_filter_type
{
    filter_type_none, 
    filter_type_almost_none, 
    filter_type_normal
};

struct ignore_pattern
{
  char* pattern;
  struct ignore_pattern* next;
};

struct ls_config 
{
  bool recurse;
  bool filter_backups;
  enum ls_filter_type filter_type;
  bool display_inode_number;
  bool human_readable_size;
  bool sort_reverse ;
  enum ls_sort_by sort_by;
  enum ls_listing_type listing_type;  
  struct ignore_pattern* ignored_patterns;
};

enum filetype 
{
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
  char* path; // realpath
  char* suppied_path; 
  enum filetype type;
  struct dirent * dentry;
  struct stat* stat;
};

struct print_config
{
  int max_filename_len;
  long total;
  int line_len;
  int num_files;
};

static const int SCREEN_SIZE = 100;

static struct fileinfo* create_fileinfo(const char* dir_path,
                                          struct dirent* entry);

static void clear_fileinfo(struct fileinfo* fi);
static void clear_fileinfos(struct fileinfo** fi,int n);

typedef int (*sort_function_t)(const void *, const void *);

static sort_function_t sort_function(enum ls_sort_by sortby);

static void sort_files (struct ls_config* config, 
                          struct fileinfo** files, 
                          int num_entries);




static void print_files(struct ls_config* config, 
                          struct fileinfo** files, 
                          int num_entries);

static void print_simple_fileinfo(struct print_config* pc, 
                                  struct ls_config* config,
                                  struct fileinfo** fi);

static void print_long_fileinfos(struct ls_config* config, 
                                 struct fileinfo** files, 
                                   int num_entries);

static void print_long_fileinfo(struct ls_config* config,
                                  struct fileinfo* fi );


static void usage(FILE* stream, int exit_code);

static bool skip_file(struct ls_config* config, char* entry_name);

static int ls_cmd(const char* pwd, struct ls_config* config);


static int fi_cmp_mtime(const void * i1, const void* i2);
static int fi_cmp_name(const void * i1, const void* i2);
static int fi_cmp_size(const void * i1, const void* i2);

enum term_text_type
{
  Reset =0,
  Bright=1,
  Dim=2,
  Underline=3,
  Blink=4,
  Reverse=7,
  Hidden=8
};

enum term_colors
{
  Black=0,
  Red=1,
  Green=2,
  Yellow=3,
  Blue=4,
  Magenta=5,
  Cyan=6,
  White=7,
  Default=49
};

static void start_color(enum term_text_type attr, 
                        enum term_colors fg, 
                        enum term_colors bg);

static void reset_color();

static void file_mode_string(mode_t md, char* str);
enum filetype determine_filetype(unsigned char  d_type);

void add_ignored_patterns(char* pat, struct ls_config* ls_config )
{
  struct ignore_pattern* node = malloc(sizeof(struct ignore_pattern));
  node->pattern  = pat;
  node->next = ls_config->ignored_patterns;
  ls_config->ignored_patterns = node;
}

void clear_ignored_patterns(struct ignore_pattern* ps)
{
  struct ignore_pattern* t=NULL;
  struct ignore_pattern* i = ps;
  while(i!=NULL){
    t=i->next;
    free(i);
    i=t;
  }
}

void ls_config_init(struct ls_config * config)
{
  config->recurse = false;
  config->display_inode_number =false;
  config->human_readable_size =false;
  config->sort_reverse = false;
  config->sort_by = sort_by_filename;
  config->listing_type = listing_type_simple;
  config->filter_type = filter_type_normal;
  config->filter_backups = false;
  config->ignored_patterns = NULL;
}

int main(int argc,char * argv[])
{
  program_name = argv[0];

  int next_opt = false;

  struct ls_config config;
  ls_config_init(&config);

  const char* short_options = "iaABrSshvltUR?I:";

  const struct option long_options[] = {
    {"all",0,NULL,'a'},
    {"almost-all",0,NULL,'A'},
    {"ignore",1,NULL,'I'},
    {NULL,0,NULL,'i'},
    {"ignore-backups",0,NULL,'B'},
    {"human-readable",0,NULL,'h'},
    {NULL,0,NULL,'U'},
    {"status",0,NULL,'s'},
    {"sort-size",0,NULL,'S'},
    {"reverse",0,NULL,'r'},
    {"recurse",0,NULL,'R'},
    {NULL,0,NULL,'t'},
    {"long",0,NULL,'l'},
    {"verbose",1,NULL,'v'}
  };

  do{    
    next_opt = getopt_long(argc,argv,
                           short_options,long_options, NULL);
    extern int optind;
    switch(next_opt){
    case 'h':    
      config.human_readable_size = true;
      break;
    case 'i':
      config.display_inode_number = true;
      break;
    case 'I':
      add_ignored_patterns(optarg,&config);
      break;
    case 'B':
      config.filter_backups = true;
      break;
    case 'a':
      config.filter_type = filter_type_none;
      break;
    case 'A':
      config.filter_type = filter_type_almost_none;
      break;
    case 'r':
      config.sort_reverse = true;
      break;
    case 'R':
      config.recurse = true;
      break;
    case 'U':
      config.sort_by = sort_by_nosort;
      break;
    case 't':
      config.sort_by = sort_by_mtime;
      break;
    case 'S':
      config.sort_by = sort_by_size;
      break;
    case 's':
      break;
    case 'l':
      config.listing_type = listing_type_long;
      break;
    case '?': // user specified invalid option
      usage(stderr,0);
    case -1:
      break;
    default:
      printf("unexpected exit");
      abort();
    }
  } while (next_opt != -1);  
  
  char dir[PATH_MAX];
  if(optind >= argc) {
    char* done = (char*) getcwd(dir,PATH_MAX);
    if(!done) {
      perror("getcwd");
      exit(1);
    }
  } else {
    strncpy(dir,argv[optind],PATH_MAX);
  } 
  ls_cmd(dir,&config);
  //TODO rethink recursion
  return 0;
}

int ls_cmd(const char* pwd, struct ls_config* config) 
{

  DIR* dir = opendir(pwd);
  char err_no_dir[2048];
  snprintf(err_no_dir,2048,"opendir:No such directory [%s]",pwd);

  if(NULL == dir){
    perror(err_no_dir);
    return 1;
  }

  int num_entries = 0;
  int alloc_entries = 100;

  struct fileinfo** files = malloc(alloc_entries*sizeof(struct fileinfo *));
  struct dirent * entry;  
  
  while (NULL!= (entry = readdir(dir))) {    

    if(skip_file(config,entry->d_name)){
      goto outer_loop;
    }

    if (num_entries >= alloc_entries) {
      alloc_entries *=  2;
      struct fileinfo** p = realloc (files , alloc_entries * sizeof(struct fileinfo*));
      if(!p){
        printf("Error in realloc \n");
        return 1;
      } 
      files = p;
    } 

    struct fileinfo* fi = create_fileinfo(pwd,entry);    

    if(!fi) {
      printf("error getting fileinfo for %s/%s \n",pwd,entry->d_name);
      num_entries++;
      goto outer_loop;
    }
    files[num_entries++] = fi;

  outer_loop: ;//)

  }

  sort_files(config,files,num_entries);    

  print_files(config,files,num_entries);

  clear_fileinfos(files,num_entries);

  free(files);
  closedir(dir);
  return 0;
}

sort_function_t sort_function(enum ls_sort_by sortby)
{
  switch(sortby){
  case sort_by_size:
    return &fi_cmp_size;
  case sort_by_filename:
    return &fi_cmp_name;
  case sort_by_mtime:
    return &fi_cmp_mtime;
  case sort_by_nosort:
    return NULL;
  }
  return NULL;
}

long total_file_size(struct fileinfo** files, int num_entries)
{
  int i;
  long  total =0;
  for(i = 0; i < num_entries; i++) {
    total += files[i]->stat->st_size;
  }
  total = total/1024;
  return total;
}

int max_filename_length(struct fileinfo** files, int num_entries)
{  
  int i;
  int max_filename_len = 0;
  for(i =0;i<num_entries; i++){
    int len = strlen(files[i]->name);
    if(len > max_filename_len)
      max_filename_len = len;
  }  
  return max_filename_len;
}

static void print_files(struct ls_config* config, struct fileinfo** files, int num_entries)
{

  struct print_config pc;

  switch(config->listing_type){
  case listing_type_long:        
    printf("total %ld\n",total_file_size(files,num_entries)); 
    print_long_fileinfos(config,files,num_entries);
    break;
  case listing_type_simple:        
    pc.max_filename_len = max_filename_length(files,num_entries);
    pc.line_len = SCREEN_SIZE;
    pc.num_files = num_entries;
    print_simple_fileinfo(&pc,config,files);
    break;
  }
}

void sort_files (struct ls_config* config, struct fileinfo** files, int num_entries)
{
  sort_function_t sortfn = sort_function(config->sort_by);

  if(sortfn)
    qsort(files,num_entries,sizeof(struct fileinfo*), sortfn);

  if(config->sort_reverse) {
    struct fileinfo* tmp;
    int i;
    for(i=0; i < (num_entries+1)/2 ;i++){
      tmp = files[i];
      files[i] = files[num_entries-i];
      files[num_entries-i]=tmp;
    }
  }  
}

void filetype_sdesc(char** s,struct fileinfo* fi)
{
  const char* desc;
  switch(fi->dentry->d_type){
  case DT_LNK:
    desc= "l";
    break;
  case DT_BLK:
    desc="b";
    break;
  case DT_CHR:
    desc="c";
    break;
  case DT_REG:
    desc ="-";
    break;
  case DT_DIR:
    desc ="d";
    break;
  case DT_FIFO:
    desc ="f";
    break;
  case DT_SOCK:
    desc="s";
    break;    
  default:
    desc ="-";
    break;
  }
  *s = strdup(desc);
}

void file_mode_string(mode_t md,char* str)
{
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

void print_formatted_filename(struct fileinfo* fi,char* format,bool show_link);

void print_simple_fileinfo(struct print_config* pc, 
                           struct ls_config* config,
                           struct fileinfo** files) 
{

  int max_column_width =(pc->max_filename_len);
  int num_cols = pc->line_len/max_column_width;
  if(num_cols <= 0){
    num_cols = 1;
  }
  
  int col_widths[num_cols];
  int i;

  for(i=0;i< num_cols;i+=1) 
    col_widths[i]=0;

  for(i=0;i< pc->num_files;i+=1) {
    int l = strlen(files[i]->name);
    if(col_widths[i%num_cols] < l) {
      col_widths[i%num_cols] = l;
    }
  }

  for(i=0;i<pc->num_files;i++){
      if(files[i]==NULL)
        continue;
      struct fileinfo* fi = files[i];
      if(config->display_inode_number)
        printf("%7ld ",fi->stat->st_ino);
      char fmt[15];
      snprintf(fmt,15,"%%-%ds  ",col_widths[i%num_cols]);
      print_formatted_filename(fi,fmt,false);
      if((i+1)%num_cols==0 && i!=0){
        printf("\n");  
      }
  }
  if(i%num_cols!=0)
    printf("\n");
}

void print_long_fileinfos(struct ls_config* config, 
                          struct fileinfo** files, int num_entries)  
{
  int i;
  for( i =0 ; i < num_entries; i++) {
    if(files[i]!=NULL){ // filtered files end up as null      
      print_long_fileinfo(config,files[i]);
    }
  }
}

void print_long_fileinfo(struct ls_config* config ,struct fileinfo* fi )
{
  if(config->display_inode_number)
    printf("%7ld ",fi->stat->st_ino);

  char* desc;
  filetype_sdesc(&desc,fi);
  printf("%1s",desc);
  free(desc);

  char mode_str[10];
  file_mode_string(fi->stat->st_mode,mode_str);
  printf("%9s ",mode_str);

  printf("%3d",fi->stat->st_nlink);

  struct passwd* pw = getpwuid(fi->stat->st_uid);
  printf("%9s ",pw->pw_name);

  struct group* group_info =  getgrgid(fi->stat->st_gid);    
  printf("%9s ",group_info->gr_name);

  double sz = fi->stat->st_size;  
  if(config->human_readable_size) {
    if(sz > (1<<20)) {
      printf("%6.1fM ", sz/pow(2,20));
    } else if(sz > (1<<10)){
      printf("%6.1fK ",sz/pow(2,10));
    }else{
      printf("%6.0f  ",sz);
    }
  } else{
    printf("%8d  ",(int)sz);
  }

  char mod_time[100];
  strftime(mod_time,100,"%b %d %H:%M",localtime(&(fi->stat->st_mtime)));
  printf("%9s ",mod_time);
  print_formatted_filename(fi,NULL,true);
  printf("\n");

}

void print_formatted_filename(struct fileinfo* fi,char* format,bool show_link)
{  
  switch(fi->type) {
  case directory:
    start_color(Bright, Blue, Default);
    break;  
  case normal:
  default:
    reset_color();
  }
  if(fi->type == normal) {
    if(fi->stat->st_mode & S_IXUSR) 
      start_color(Bright, Green, Default);    
  }

  if(S_ISLNK(fi->stat->st_mode)) {
    start_color(Dim, Blue, Default);
    if(show_link)
      printf("%s -> %s",fi->name,fi->path);
    else
      printf("%s",fi->name);
    reset_color();
    return;
  }
  // hmm..
  if(format == NULL)
    format = "%s";
  printf(format,fi->name);
  reset_color();
}

struct fileinfo* create_fileinfo(const char* dir_path,struct dirent* entry)  
{
  struct fileinfo* fi = malloc(sizeof(struct fileinfo));
  fi->name =  strdup(entry->d_name);
  char p[PATH_MAX];
  sprintf(p,"%s/%s",dir_path,entry->d_name);
  fi->path = malloc(PATH_MAX);
  fi->suppied_path = strdup(p);
  if(!realpath(p,fi->path)) {
    clear_fileinfo(fi);
    perror("realpath");
    return NULL;
  }

  struct stat* file_stat = malloc(sizeof(struct stat));  
  int err = lstat(fi->suppied_path,file_stat);
  if(err){
    clear_fileinfo(fi);
    free(file_stat);    
    perror("stat");
    return NULL;
  }  

  fi->dentry = entry;
  fi->stat = file_stat;
  fi->type = determine_filetype(entry->d_type);
  return fi;
}

void clear_fileinfos(struct fileinfo** files,int num_entries)
{
  int i;
  for(i=0 ; i< num_entries; i++) {
    if(files[i]!=NULL)
      clear_fileinfo(files[i]);        
  }  
}

void clear_fileinfo(struct fileinfo* fi)
{
  if(fi) {
    if(fi->name) free(fi->name);
    if(fi->path) free(fi->path);
    if(fi->suppied_path) free(fi->suppied_path);
    if(fi->stat) free(fi->stat);
    free(fi);
  }
}

enum filetype determine_filetype(unsigned char  d_type)
{
  enum filetype ft;
  switch(d_type){
  case DT_LNK:
    ft = symbolic_link;
    break;
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
  return ft;
}

/**
 * Determine whether or not to skip file
 */
bool skip_file(struct ls_config* config, char* entry_name)
{
  char* ignored_files [] ={".",".."};
  int nignored = 2;
  if(config->filter_type != filter_type_none) {
    int i = 0 ;
    for(i=0;i< nignored; i++){
      if(strcmp(entry_name,ignored_files[i]) == 0) {
        return true;
      }
    }

    //Filter files all files starting with .
    if(config->filter_type != filter_type_almost_none && entry_name[0] == '.') {
      return true;
    }      
  }

  if(config->filter_backups && entry_name[strlen(entry_name)-1] == '~') {
    return true;
  }

  if(config->ignored_patterns !=NULL) {
    struct ignore_pattern* i = NULL;
    for(i = config->ignored_patterns;i!=NULL; i=i->next){
      if(fnmatch(i->pattern,entry_name,FNM_PATHNAME) == 0){
        return true;
      }
    }
  }
  return false;
}


int fi_cmp_mtime(const void * i1, const void* i2)
{
  struct fileinfo** fi1 = (struct fileinfo**)i1;
  struct fileinfo** fi2 = (struct fileinfo**)i2;
  return (*fi1)->stat->st_mtime < (*fi2)->stat->st_mtime;
}

int fi_cmp_name(const void * i1, const void* i2)
{
  struct fileinfo** fi1 = (struct fileinfo**)i1;
  struct fileinfo** fi2 = (struct fileinfo**)i2;
  return strcmp((*fi1)->name,(*fi2)->name);
}

int fi_cmp_size(const void * i1, const void* i2)
{
  struct fileinfo** fi1 = (struct fileinfo**)i1;
  struct fileinfo** fi2 = (struct fileinfo**)i2;
  return (*fi1)->stat->st_size > (*fi2)->stat->st_size;
}

int fi_cmp_type(const void * i1, const void* i2)
{
  struct fileinfo** fi1 = (struct fileinfo**)i1;
  struct fileinfo** fi2 = (struct fileinfo**)i2;
  return (*fi1)->type > (*fi2)->type;
}

void usage(FILE* stream, int exit_code)
{
  fprintf(stream,"Usage: %s  <options> [input file] \n",program_name);

  struct description{
    char* short_option;
    char* long_option;
    char* description;
  };
  
  const struct description desc_arr[] = {
    {"-a","--all","Display all including hidden files"},
    {"-A","--almost-all","Display almost all files"},
    {"-i","","Display inode numbers"},
    {"-I[pat]","--ignore","Display ignore files by matching patterns"},
    {"-l","--long","Display long list of file information"},
    {"-v","--verbose","Print verbose output"},
    {"-S","--sort-size","Sort by file size"},
    {"-t","","Sort by file modification time"},
    {"-B","","Filter out backup files"},
    {"-r","--reverse","Reverse files in a directory"},
    {"-h","--human-readable","Display file sizes in human readable format"},
    {"-R","--recurse","Recursive directory listing "}
  };

  int i = 0;
  for(i = 0 ; i < 11;i++) {
    fprintf(stream,"%-10s%-14s\t%-10s\n"
            ,desc_arr[i].short_option
            ,desc_arr[i].long_option
            ,desc_arr[i].description);                
  }
  exit(exit_code);
}

bool enable_colors()
{
  // no colrs for dumb terminals
  if(isatty(fileno(stdout))) {
    return true;
  }
  return false;
}

void start_color(enum term_text_type attr, enum term_colors fg, enum term_colors bg)
{
  if(!enable_colors())
    return;
  char cmd[13];
  sprintf(cmd, "%c[%d;%d;%dm", 0x1B, attr, fg + 30, bg + 40);
  printf("%s",cmd);
}

void reset_color() 
{
  if(!enable_colors())
    return;
  const char* reset_code = "\033[0m";
  printf("%s",reset_code);
}
