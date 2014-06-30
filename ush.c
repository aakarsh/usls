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

#include "ush_parser.tab.c"

void yyerror(char *s)
{
    fprintf(stderr, "%s\n", s);
}

//  Populated by the yacc parser.
extern struct command* parsed_command_list;


const char* program_name;

struct config {

};

void config_init(struct config * config){
}


void print_usage_cmd(FILE* stream, int exit_code);
void prompt(FILE* s);
char* find_cmd(char* s);




void clear_commands(struct command** pcl){
  struct command *i = NULL,*next;
  for(i  = *pcl; i ; i = next) {
    free(i->value->value);
    free(i->value);
    next = i->next;
    free(i);
  }
  *pcl = NULL;
}

int main(int argc,char * argv[])
{

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
  //  build_cmd_cache();

  prompt(s);
  while(!feof(stdin)) {


    int retval = yyparse();
    if(retval!= 0) {      
      printf("syntax error!\n");
      goto exit;
    }

    struct command* i  = parsed_command_list;
    for(;i!=NULL; i = i->next) {      

      char* cmd = i->value->value->value;

      if(strcmp(cmd,"exit") == 0){
        printf("Goodbye!\n");
        exit(1);
      }
      int arg_cnt =  0 ; 
      struct word_list* p = i->value;
      for(; p ; p = p->next) arg_cnt++;

      char* cmd_args[arg_cnt+1];
      int c = 0;
      p = i->value;
      for(;p ;p = p-> next) {
        cmd_args[c++] = p->value->value; 
      }

      cmd_args[arg_cnt] = NULL;

      pid_t cid = fork();

      int child_status; 

      if(cid!= 0) {  //parent 
        wait(&child_status);
        if(WIFEXITED(child_status)){
          goto exit;
        } else{
          perror(cmd);
        }
      } else { //child
        int err = execvp(cmd,cmd_args);
        if(err){
          perror("execlp");
          exit(1);
        }
      }      
    }  
  exit:
    clear_commands(&parsed_command_list);
   prompt(s);    
  }
  clear_commands(&parsed_command_list);
  return 0;
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
