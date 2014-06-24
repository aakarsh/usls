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

#define _GNU_SOURCE         

const char* program_name;

struct config {

};

void config_init(struct config * config){
}

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
  int exit =0;

  while(NULL != fgets(line,max_cmd_len,stdin)){
    int len = strlen(line);
    char* cmd = malloc(len);    
    strncpy(cmd,line,len-1);    

    if(strcmp(cmd,"exit") == 0){
      exit =1;
    }    

    //    fprintf(stdout,"%s\n",cmd);    
    char* cmd_path = find_cmd(cmd);
    if(!cmd_path){
      printf("%s : Not found\n",cmd);
      goto prompt;
    }
    printf("Found: [%s] %s!\n",cmd_path,cmd);
    free(cmd_path);    

    if(exit)    
      break;

  prompt:
    prompt(s);
    free(cmd);
  } 
  return 0;
}

char* find_cmd(char* cmd){
  char* path_dirs = strdup(getenv("PATH"));
  char* path =  strtok(path_dirs,":");
  int found = 0;
  char* cmd_path = NULL;

  while(NULL != path && !found) {
    //    printf("%s\n",path);    
    DIR* d = opendir(path);    
    if(!d){
      fprintf(stderr,"Cannot open %s",path);
      perror("opendir");
      exit(1);
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
        //        printf("%s\n",full_path);
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
