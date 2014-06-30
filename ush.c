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
/*
  populated by the yacc parser.
 */
extern struct command* parsed_command_list;


const char* program_name;

enum token_type {tok_semi,tok_ident,tok_pipe, tok_and ,tok_or,tok_unknown};

struct cmd_token{ 
  char* value;
  int start;
  int end;
  enum token_type type;
  struct cmd_token* next;
  struct cmd_token* prev;
};

struct cmd_token* tokenize(char* line);

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
  build_cmd_cache();

  prompt(s);
  while(!feof(stdin)) {


    int retval = yyparse();
    if(retval!= 0) {      
      printf("syntax error!\n");
      goto exit;
    }
    printf("retval : %d\n",retval);

    struct command* i  = parsed_command_list;
    for(;i!=NULL; i = i->next) {      

      char* cmd = i->value->value->value;
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

        //i->value->value;
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
        int err = execvp(cmd,cmd_args);//"ush",NULL);
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

  /**
  while(NULL != fgets(line,max_cmd_len,stdin)){
    size_t len = strlen(line);
    line[len-1]= '\0';
    char* cmd = line;
    tokenize(cmd);
    if(strcmp(cmd,"exit") == 0){
      break;      
    }    

    printf("[%s]\n",cmd);
    pid_t cid = fork();
    int child_status;
    if(cid != 0) {
      wait(&child_status);
      if(WIFEXITED(child_status)){
        goto prompt;
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
  prompt:
    prompt(s);
  } 
  */
  return 0;
}

struct cmd_token* create_token()
{
  struct cmd_token* cur = malloc(sizeof(struct cmd_token));  
  cur->start = 0;
  cur->end = 0;
  cur->value =NULL;
  cur->type = tok_unknown;
  cur->prev=NULL;
  cur->next =NULL;
  return cur;
}

struct cmd_token* create_token2(char* value,enum token_type ty,int start,int end)
{
  struct cmd_token* cur = create_token();
  cur->value = value;
  cur->start = start;
  cur->end = end;
  cur->type = ty;
  return cur;
}

int push_token(struct cmd_token** tokens, enum token_type ty,char* value, int start, int end)
{ 
  if(!tokens)
    return -1;

  struct cmd_token* cur = create_token2(value,ty,start,end);

  if(tokens) {
    cur->next = *tokens;
    if(*tokens){
      (*tokens)->prev=cur;
    }
    *tokens = cur;
  }
  return 0;
}

void print_token(struct cmd_token* token)
{
  printf("[%s (%d-%d)] \n",token->value,token->start,token->end);
}

int tokens_reverse(struct cmd_token** tokens )
{
  if(tokens == NULL || *tokens == NULL){
    return -1;
  }
  printf("tokens_reverse\n");
  struct cmd_token* head = *tokens;
  struct cmd_token* l = *tokens;
  while(l!=NULL) {
    struct cmd_token* saved_next = l->next;
    struct cmd_token* saved_prev = l->prev;
    l->prev = l->next;
    l->next = saved_prev;
    if(saved_next == NULL){
      head = l;
    } 
    l = saved_next;
  }
  *tokens  = head;
  printf("ordered {\n");  
  l=*tokens;
  while(l){
    print_token(l);
    l=l->next;
  }
  printf("}\n");  
  return 0;
}

char* substring(char* line, int start, int end)
{
  int n = end - start;      
  if(n < 0){
    return NULL;
  }
  char* str = malloc(n+1);  
  strncpy(str,line+start,n);
  str[n]='\0';
  return str;
}

struct cmd_token* tokenize(char* line){
  int i;
  enum state{word,blank} st;
  struct cmd_token* tokens = NULL;

  st = blank;
  int tok_end = 0, tok_start = 0;
  char* token_value;
  for(i =0; i< strlen(line); i++){
    printf("[%c]\n",line[i]);

    char* delim = ";|";

    if((isblank(line[i]) || strchr(delim,line[i]))) {

      if(st == word) {
        tok_end = i;
        token_value = substring(line,tok_start,tok_end);
        push_token(&tokens,tok_ident,token_value,tok_start, tok_end);
      }
      switch(line[i]) {
      case ';':
        push_token(&tokens,tok_semi,";",i,i);
        break;
      case '|':
        push_token(&tokens,tok_pipe,"|",i,i);
      default:;
      }
      st = blank;
    } else if((isalnum(line[i]) || strchr("-$_",line[i]))){
      if(st != word) {
        st = word;
        tok_start = i;
      }
    }
  }

  if(st == word ) { // in the middle of parsing a word
    tok_end = i;
    token_value = substring(line,tok_start,tok_end);
    push_token(&tokens,tok_ident,token_value,tok_start,tok_end);
  }

  struct cmd_token* l = NULL;
  for(l = tokens; l!= NULL; l = l->next) {
    printf("pre-reverse-token[%s:%d]\n",l->value,l->type);
  }

  tokens_reverse(&tokens);

  l = NULL;
  for(l = tokens; l!= NULL; l = l->next) {
    printf("post-rev-token[%s:%d]\n",l->value,l->type);
  }

  return tokens;
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
