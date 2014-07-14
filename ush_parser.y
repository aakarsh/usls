%{
#include <ctype.h>
#include <stdio.h>

extern FILE *yyin;
void yyerror(char *s);
int yylex();

struct command{    
    struct word_list* value;    
    struct command* next;
};

struct command* parsed_command_list  = NULL;

struct word{
    char* value;
    int flags;
};

struct word_list{
    int size;
    struct word* value;
    struct word_list*  next;
};
struct word_list* make_word_list_node(struct word* word);
struct word_list* add_word_list(struct word_list** list , struct word* word) ;
struct word*  make_word(char* str);
struct command* make_cmd(struct word_list* wd);
struct command* add_cmd(struct command* cmd);

%}

%union {
  char* sval;
  double dval;
  struct word* word_val;
  struct word_list* word_list;
  struct command* command_list;
  struct command* command;
}

%token <value> NAME 
%token <dval>   NUM_TOK
%token <sval>   WORD_TOK

%type <command>       command
%type <command_list>  command-list  
%type <command_list>  start
%type <word_list>     word-list
%type <word_val>      word

%%
start: command-list  
                     {  
                        parsed_command_list = $1; 
                        $$ = parsed_command_list; 
                     } 

command-list:   
       command '|' command-list   
             { 
                 //printf("[parser piped %s]\n ",$1->value->value); 
                 $1->next = $3;
                 $$ = $1;
             } 
       | command  { $$ = $1; } 
;

command:  word-list ';' command
                      {
                          //printf("[parser-; %s;] \n",$1);
                          //printf("[next command in ; %s;] \n",$3->value->value);
                          struct command*  cmd = make_cmd($1);
                          cmd->next = $3;
                          $$ = cmd;
                      }
       | word-list     
                      { 
                          //printf("[parser %s] \n",$1);
                          $$ = make_cmd($1);
                      }
;

word-list: word word-list 
                    { 
                        struct word_list* lst = $2;
                        $$ = add_word_list(&lst,$1);
                    }
          | word     {
                         $$ = make_word_list_node($1);
                    }
;

word: WORD_TOK      {    
                         $$ = make_word($1);
                    }

;

%%

struct word_list* add_word_list(struct word_list** list , struct word* word) 
{
    struct word_list* node = make_word_list_node(word);
    node->next = *list;
    *list = node;    
    return *list;
}

struct word_list* make_word_list_node(struct word* word) 
{
    struct word_list* word_list = malloc(sizeof (struct word_list));
    word_list->value = word;
    word_list->next = NULL;
    word_list->size = 1;
    return word_list;
}

struct word*  make_word(char* str) 
{
    struct word* wd   = malloc(sizeof (struct word*));
    wd->value = strdup(str);
    wd->flags = 0;
    return wd;
}

struct command* make_cmd(struct word_list* wd) 
{
    struct command* cmd   = malloc(sizeof (struct command*));
    cmd->value = wd;
    cmd->next = NULL;
    return cmd;
}



