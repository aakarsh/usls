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

#include "gen/ush_parser.tab.c"

void yyerror(char *s)
{
    fprintf(stderr, "%s\n", s);
}

//  Populated by the yacc parser.
extern struct simple_command* parsed_command_list;

const char* program_name;

struct config {

};

void config_init(struct config * config){
}

void usage(FILE* stream, int exit_code);
void prompt(FILE* s);
char* find_cmd(char* s);

void clear_commands(struct simple_command** pcl)
{
  struct simple_command *i = NULL,*next;
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
      usage(stdout,0);      
      break;
    case '?': // user specified invalid option
      usage(stderr,1);
    case -1:
      break;
    default:
      printf("unexpected exit");
      abort();
    }
  } while (next_opt != -1);  
    
  FILE* s = stdout;
  prompt(s);
  while(!feof(stdin)) {


    int retval = yyparse();
    if(retval!= 0) {      
      fprintf(s,"syntax error!\n");
      goto exit;
    }

    struct simple_command* i  = parsed_command_list;
    for(;i!=NULL; i = i->next) {      

      char* cmd = i->value->value->value;

      if(strcmp(cmd,"exit") == 0){
        fprintf(s,"Goodbye!\n");
        exit(EXIT_SUCCESS);
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

      if (strcmp(cmd,"cd") == 0 ) {

        char * dest_dir = NULL;
        if(arg_cnt<=1){
          dest_dir = getenv("HOME");
          if(!dest_dir) {
            fprintf(stderr,"Couldnt determine home directory\n");
            goto exit;
          }
        }else {
          dest_dir = cmd_args[1];
        }          
        int ret = chdir(dest_dir);
        if(ret) {
          perror("chdir");
        }
        goto exit;

      }
      FILE* r_stream = NULL;
      if(i->redirect){
        fprintf(stderr,"entry\n");
        char* r_file = i->redirect->redirector.filename->value;
        fprintf(stderr,"redirect file %s\n",r_file);
        switch(i->redirect->instruction){
        case r_input_direction:
          fprintf(stderr,"redirect read\n");
          r_stream = fopen(r_file,"r");
          break;
        case r_output_direction:
          fprintf(stderr,"redirect write\n");
          r_stream = fopen(r_file,"w");
        }
      }
      pid_t cid = fork();

      int child_status; 

      if(cid!= 0) {  //parent 
        wait(&child_status);
        if(WIFEXITED(child_status)){
          if(i->redirect)
            fclose(r_stream);
          goto exit;
        } else{
          perror(cmd);
        }
      } else { //child
        if(i->redirect){
          switch(i->redirect->instruction){
          case r_input_direction:    
            fprintf(stderr,"dup input stream \n");
            dup2(fileno(r_stream),0);
            break;
          case r_output_direction:
            fprintf(stderr,"dup output stream \n");
            dup2(fileno(r_stream),1);
            break;
          }
        }
        int err = execvp(cmd,cmd_args);
        if(err){
          perror("execlp");
          exit(EXIT_FAILURE);
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


void usage(FILE* stream, int exit_code){
  fprintf(stream,"Usage: %s  <options> [input file] \n",program_name);
  fprintf(stream,
          "-h    --help    Display this usage information\n"
          "-l    --long    Display long list of information\n");
  exit(exit_code);
}
