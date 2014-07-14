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
#include <stdbool.h>


const char* program_name;

struct config {
  bool show_numbers;
  bool show_numbers_non_blank;
  bool show_tabs;
  bool show_ends;
  bool squeeze_blanks;
};

void config_init(struct config * config){
  config->show_numbers = false;
  config->show_numbers_non_blank = false;
  config->show_tabs = false;
  config->show_ends = false;
  config->squeeze_blanks = false;
}
int cat_cmd(FILE* stream, struct config * config);
void print_usage_cmd(FILE* stream, int exit_code);

int main(int argc,char * argv[]){

  program_name = argv[0];

  int next_opt = 0;
  struct config config;
  config_init(&config);

  const char* short_options = "nbsTE";

  const struct option long_options[] = {
    {"all",0,NULL,'a'},
    {NULL,0,NULL,'n'},
    {NULL,0,NULL,'b'},
    {"squeeze-blanks",0,NULL,'s'},
    {"show-ends",0,NULL,'E'},
    {"show-tabs",0,NULL,'T'}
  };

  do{    
    next_opt = getopt_long(argc,argv,
                           short_options,long_options, NULL);
    extern int optind;
    switch(next_opt){
    case 'h':
      print_usage_cmd(stdout,0);      
      break;
    case 'n':
      config.show_numbers = true;
      break;
    case 'b':
      config.show_numbers_non_blank = true;
      config.show_numbers = false;
      break;
    case 'T':
      config.show_tabs = true;
      break;
    case 'E':
      config.show_ends = true;
      break;
    case 's':
      config.squeeze_blanks = true;
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


  
  
  if(optind >= argc){
    cat_cmd(stdin,&config);    
  } else {
    int i ;
    for(i = optind; i < argc; i++){
      char* file_name = argv[i];
      FILE* str;
      if(strcmp(file_name,"-") == 0)
        str = stdin;
      else
        str = fopen(file_name,"r");
      if(!str){
        printf("Opening %s \n" ,file_name);
        perror("fopen");
        exit(1);
      }
      cat_cmd(str,&config);

      int err = fclose(str);
      if(err) {
        printf("Opening %s \n" ,file_name);
        perror("fclose");
        exit(1);
      }
    }
  }
  return 0;
}

int cat_cmd(FILE* stream, struct config * config){ 
  int  LINE_LEN = 2048;
  char line_buffer[LINE_LEN];
  int cnt = 1;
  bool last_line_empty = false;

  while(fgets(line_buffer,LINE_LEN,stream) != NULL){
    int len =  strlen(line_buffer);

    if(config->show_numbers){
      printf("%-6d ",cnt++);
    } 

    if(len <= 1){
      if(last_line_empty && config->squeeze_blanks ){
        goto loop_end;
      }
      last_line_empty = true;
    } else{
      last_line_empty = false;
    }

    if( len > 1 && config->show_numbers_non_blank) {
      printf("%-6d ",cnt++);
    }

    int i=0;
    for(i =0; i< len-1;i++) {
      char c = line_buffer[i];
      if('\t' == c && config->show_tabs){        
        printf("^I");
      }else {
        printf("%c",c);
      }
    }
    if(config->show_ends){
      printf("$");
    }
    printf("\n");
    
    loop_end : ;
  }      
  return 0;
}


void print_usage_cmd(FILE* stream, int exit_code){
  fprintf(stream,"Usage: %s  <options> [input file] \n",program_name);
  fprintf(stream,
          "-h    --help    Display this usage information\n"
          "-l    --long    Display long list of information\n");
  exit(exit_code);
}
