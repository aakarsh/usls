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
#include <fnmatch.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>



const char* program_name;

struct config {

};

void config_init(struct config * config){
}

struct cmd_cache{
  char* cmd;
  char* path;
  struct cmd_cache* next;
};
void add_cmd_cache(char* cmd, char* path);
struct cmd_cache* find_cmd_cache(char* cmd);
void build_cmd_cache();

void print_usage_cmd(FILE* stream, int exit_code);
void prompt(FILE* s);
char* find_cmd(char* s);

static char* create_path(char* dir,char* fname);

int main(int argc,char * argv[]){

  program_name = argv[0];

  int next_opt = 0;
  struct config config;
  config_init(&config);

  const char* short_options = "cmlLwh";

  const struct option long_options[] = {
    {"help",0,NULL,'h'},
    {"bytes",0,NULL,'c'},
    {"chars",0,NULL,'m'},
    {"lines",0,NULL,'l'},
    {"max-line-length",0,NULL,'L'},
    {"words",0,NULL,'w'}
  };

  do{    
    next_opt = getopt_long(argc,argv,
                           short_options,long_options, NULL);
    extern int optind;
    switch(next_opt){
    case 'h':
      print_usage_cmd(stdout,0);      
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
    
  FILE* s = stdout;
  prompt(s);
  size_t max_cmd_len = 4096;
  char line[max_cmd_len];  

  build_cmd_cache();

  while(NULL != fgets(line,max_cmd_len,stdin)){
    size_t len = strlen(line);
    line[len-1]= '\0';
    char* cmd = line;
    if(strcmp(cmd,"exit") == 0){
      break;      
    }    

    printf("[%s]\n",cmd);
    pid_t cid = fork();
    int child_status;
    if(cid != 0) {
      wait(&child_status);
      if(WIFEXITED(child_status)){
      }else{
        perror(cmd);
      }
    } else{ // child execute cmd;
      int err = execlp(cmd,"ush",NULL);
      if(err){
        perror("execlp");
        exit(1);
      }
    }  
    prompt(s);
  } 
  return 0;
}


static struct cmd_cache* cmd_list = NULL;

void add_cmd_cache(char* cmd, char* path){
  struct cmd_cache* c = malloc(sizeof (struct cmd_cache));
  c->cmd = strdup(cmd);
  c->path = strdup(path);
  c->next = cmd_list;  
  cmd_list = c;  
}

struct cmd_cache* find_cmd_cache(char* cmd){
  struct cmd_cache* c = NULL;
  for(c = cmd_list; c!=NULL; c=c->next) {
    if(strcmp(c->cmd,cmd) == 0){
      return c;
    }
  }
  return c;
}

void build_cmd_cache(){
  char* path_dirs = strdup(getenv("PATH"));
  char* path =  strtok(path_dirs,":");

  while(NULL != path ) {
    DIR* d = opendir(path);    
    if(!d){
      goto parse_next;
    }
    struct dirent* f ;

    while(NULL!=(f=readdir(d)) ) {
      if(DT_REG!= f->d_type){
        continue;
      }

      char* full_path = create_path(path,f->d_name);      
      struct stat* st = malloc(sizeof (struct stat));

      if(stat(full_path,st)){
        fprintf(stderr,"%s",path);
        perror("stat");
        if(full_path)
          free(full_path);
        continue;
      }

      if(S_IXUSR & st->st_mode) { 
        add_cmd_cache(f->d_name,full_path);
      }        
      free(st);
      if(full_path)
        free(full_path);      
    }
    closedir(d);

  parse_next:
    path = strtok(NULL,":");
  }
  free(path_dirs);
}

char* find_cmd(char* cmd){
  char* path_dirs = strdup(getenv("PATH"));
  char* path =  strtok(path_dirs,":");
  int found = 0;
  char* cmd_path = NULL;

  while(NULL != path && !found) {
    DIR* d = opendir(path);    
    if(!d){
      fprintf(stderr,"Cannot open %s\n",path);
      perror("opendir");
      return NULL;
    }

    struct dirent* f ;

    while(NULL!=(f=readdir(d)) && !found) {
      if(DT_REG!= f->d_type){
        continue;
      }
      char* full_path = create_path(path,f->d_name);      
      struct stat* st = malloc(sizeof (struct stat));

      if(stat(full_path,st)){
        fprintf(stderr,"%s",path);
        perror("stat");
        exit(1);
      }

      if(S_IXUSR & st->st_mode) { //user exec
        if(strcmp(cmd,f->d_name) == 0){
          found = 1;
          cmd_path = full_path;
        }
      }

      free(st);
      if(!found)
        free(full_path);      
    }
    closedir(d);
    path = strtok(NULL,":");

  }
  free(path_dirs);
  return cmd_path;
}
char* create_path(char* dir,char* fname){
  int sz =strlen(dir)+strlen(fname)+2;
  char* r = malloc(sz);
  sprintf(r,"%s/%s",dir,fname);
  return r;
}


void prompt(FILE* s){
  fprintf(s,"ush> ");
}


void print_usage_cmd(FILE* stream, int exit_code){
  fprintf(stream,"Usage: %s  <options> [input file] \n",program_name);
  fprintf(stream,
          "-h    --help    Display this usage information\n"
          "-l    --long    Display long list of information\n");
  exit(exit_code);
}
