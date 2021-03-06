/* -*- Mode: c; tab-width: 8; indent-tabs-mode: 1; c-basic-offset: 8; -*- */
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
void usage(FILE* stream, int exit_code);

int main(int argc,char * argv[]){

  program_name = argv[0];

  int next_opt = 0;
  struct config config;
  config_init(&config);

  const char* short_options = "hlnbsTE";

  const struct option long_options[] = {
    {"all",0,NULL,'a'},
    {"help",0,NULL,'h'},
    {NULL,0,NULL,'n'},
    {"show-numbers",0,NULL,'l'},
    {NULL,0,NULL,'b'},
    {"squeeze-blanks",0,NULL,'s'},
    {"show-ends",0,NULL,'E'},
    {"show-tabs",0,NULL,'T'}
  };

  do{    
    next_opt = getopt_long(argc,argv,short_options
                           ,long_options, NULL);
    extern int optind;
    switch(next_opt){
    case 'h':
      usage(stdout,0);      
      break;
    case 'l':
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
      usage(stderr,1);
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
        exit(-1);
      }

      cat_cmd(str,&config);

      int err = fclose(str);
      if(err) {
        printf("Opening %s \n" ,file_name);
        perror("fclose");
        exit(-1);
      }
    }
  }
  return 0;
}

int cat_cmd(FILE* stream, struct config * config){ 
  int cnt = 1;
  bool last_line_empty = false;
  char* line_buffer = NULL;
  size_t  n;
  ssize_t ret = -1;
  while((ret = getline(&line_buffer,&n,stream)) > 0 ) {
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
    fflush(stdout);

    if(line_buffer)
      free(line_buffer);

    line_buffer = NULL;
    loop_end : ;
  } 

  if(line_buffer)
    free(line_buffer);
  return 0;
}


void usage(FILE* stream, int exit_code){
  fprintf(stream,"Usage: %s  <options> [input file] \n",program_name);

  struct description
  {
    char* shrt;
    char* lng;
    char* desc;
  };

  const struct description opts[] = {
    {"-h","--help","Show this help message."},
    {"-l ","--show-numbers","Display numbers along side output"},
    {"-a ","--all","Dsiplay all"},
    {"-n ","--show-numbers","Show cat with numbers"},
    {"-s ","--squeeze_blanks","Show remove consequtive blank lines"},
    {"-E ","--show-ends","Show end of file with $ marker."},
    {"-T ","--show-tabs","Show tabs visually as ^I"}
  };

  int i = 0;
  for(i =0; i < 2; i++) {
    fprintf(stream,"%-10s%-14s\t%-10s\n"
            ,opts[i].shrt
            ,opts[i].lng
            ,opts[i].desc);
  }

  exit(exit_code);
}
