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
#include <ctype.h>
#include <stdbool.h>

const char* program_name;

struct config {
  bool show_byte_counts;
  bool show_char_counts;
  bool show_line_counts;
  bool show_max_line;
  bool show_word_counts;
};

struct wc_result {
  int line_count;
  int byte_count;
  int word_count;  
};

void clear_result(struct wc_result* res)
{
  res->line_count = 0;
  res->byte_count = 0;
  res->word_count = 0;
}

void config_init(struct config * config)
{
  config->show_byte_counts = false;
  config->show_char_counts = false;
  config->show_line_counts = false;
  config->show_max_line = false;
  config->show_word_counts = false;
}

int wc_cmd(FILE* stream, struct config * config,struct wc_result* res);
void print_usage_cmd(FILE* stream, int exit_code);

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
    case 'c':
      config.show_byte_counts = true;
      break;
    case 'm':
      config.show_char_counts = true;
      break;
    case 'l':
      config.show_line_counts = true;
      break;
    case 'L':
      config.show_max_line = true;
      break;
    case 'w':
      config.show_word_counts = true;
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
  
  struct wc_result res;
  clear_result(&res);

  if(optind >= argc){
    wc_cmd(stdin,&config,&res);    
  } else {
    int i ;
    struct wc_result total;
    clear_result(&total);
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
      clear_result(&res);
      wc_cmd(str,&config,&res);

      total.word_count+=res.word_count;
      total.byte_count+=res.byte_count;
      total.line_count+=res.line_count;

      printf("%s\n",file_name);
      int err = fclose(str);
      if(err) {
        printf("Opening %s \n" ,file_name);
        perror("fclose");
        exit(1);
      }
    }
    if(argc-optind > 1){
      printf("%6d %6d %6d  total\n", total.line_count,total.word_count,total.byte_count);
    }
  }
  return 0;
}

int wc_cmd(FILE* stream, struct config * config,struct wc_result* res)
{ 
  int char_res;
  enum word_state { word_state, black_state };  
  enum word_state ws = black_state;
  while(EOF!= (char_res = fgetc(stream))) {
    char c = (unsigned char) char_res;   
    res->byte_count++;
    if(c == '\n')
      res->line_count++;  
    
    if(ws == black_state && !(isspace(c)) ){
      res->word_count++;
      ws = word_state;
    } else if(ws == word_state && isspace(c)){
      ws = black_state;
    }
  }  
  printf("%6d %6d %6d ", res->line_count,res->word_count,res->byte_count);
  return 0;  
}


void print_usage_cmd(FILE* stream, int exit_code)
{
  fprintf(stream,"Usage: %s  <options> [input file] \n",program_name);

  struct description{
    char* short_option;
    char* long_option;
    char* description;
  };

  const struct description desc_arr[] = {
    {"-h","--help","Show this help message."},
    {"-w","--words","Show word counts."},
    {"-c","--bytes","Show byte counts"},
    {"-m","--chars","Show char counts"},
    {"-l","--lines","Show line counts"},
    {"-L","--max-line-length","Show maximum line"},
  };

  int i = 0;
  for(i = 0 ; i < 5;i++) {
    fprintf(stream,"%-10s%-14s\t%-10s\n"
            ,desc_arr[i].short_option
            ,desc_arr[i].long_option
            ,desc_arr[i].description);                
  }
  exit(exit_code);
}
