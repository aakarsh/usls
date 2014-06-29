%{
#include <ctype.h>
#include <stdio.h>

extern FILE *yyin;
void yyerror(char *s);
int yylex();

struct command{
    struct word* value;    
    struct command* next;
};

struct command* parsed_command_list  = NULL;

struct word{
    char* value;
    int flags;
};
struct word*  make_word(char* str);
struct command* make_cmd(struct word* wd);
struct command* add_cmd(struct command* cmd);

%}

%union {
  char* sval;
  double dval;
  struct word* word_val;
  struct command* command_list;
  struct command* command;
}

%token <value> NAME 
%token <dval>   NUM_TOK
%token <sval>   WORD_TOK

%type <command>       command
%type <command_list>  command-list  
%type <command_list>  start
%%
start: command-list  {  parsed_command_list = $1; $$ = parsed_command_list; } 

command-list:   
       command '|' command-list   
             { 
                 printf("[parser piped %s]\n ",$1->value->value); 
                 $1->next = $3;
                 $$ = $1;
             } 
       | command  {$$ = $1;} 
;

command:  WORD_TOK ';' command
                      {
                          printf("[parser-; %s;] \n",$1);
                          printf("[next command in ; %s;] \n",$3->value->value);
                          struct command*  cmd = make_cmd(make_word($1));
                          cmd->next = $3;
                          $$ = cmd;
                      }
         | WORD_TOK  
                      { 
                          printf("[parser %s] \n",$1);
                          $$ = make_cmd(make_word($1));
                      }
;

%%
struct word*  make_word(char* str) 
{
    struct word* wd   = malloc(sizeof (struct word*));
    wd->value = strdup(str);
    wd->flags = 0;
    return wd;
}

struct command* make_cmd(struct word* wd) 
{
    struct command* cmd   = malloc(sizeof (struct command*));
    cmd->value = wd;
    cmd->next = NULL;
    return cmd;
}


/*
int main(int argc, char* argv[])
{
  while(!feof(stdin)) {
    yyparse();
  }
  return 0;
}

void yyerror(char *s)
{
    fprintf(stderr, "%s\n", s);
}
*/
