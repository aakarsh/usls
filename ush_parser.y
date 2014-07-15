%{
#include <ctype.h>
#include <stdio.h>

extern FILE *yyin;
extern void yyerror(char *s); 
int yylex();


typedef struct word {
    char* value;
    int flags;
} WORD_DESC;

typedef struct word_list {
    int size;
    struct word* value;
    struct word_list*  next;
} WORD_LIST;

enum r_instruction {r_output_direction, r_input_direction};

typedef union {
    int dest;
    WORD_DESC* filename;
} REDIRECTEE;

struct redirect {
    struct redirect* next;
    enum r_instruction instruction;
    REDIRECTEE redirector;
};

enum command_type{c_simple,c_connection};

typedef struct simple_command{    
    WORD_LIST* value;    
    struct redirect* redirect;
    struct simple_command* next;
} COMMAND;

enum connection_type {connection_type_pipe,connection_type_and , connection_type_or};

typedef struct connection{
    COMMAND* first;
    COMMAND* second;
    enum connection_type type;
} CONNECTION;


COMMAND* parsed_command_list  = NULL;
struct redirect* make_redirect(struct word* word,enum r_instruction);
WORD_LIST* make_word_list_node(struct word* word);
WORD_LIST* add_word_list(WORD_LIST** list , struct word* word) ;
struct word*  make_word(char* str);
COMMAND* make_cmd(WORD_LIST* wd);
COMMAND* add_cmd(COMMAND* cmd);

%}

%union {
  char* sval;
  double dval;
  struct word* word_val;
  struct word_list* word_list;
  struct simple_command* command_list;
  struct simple_command* command;
  struct redirect* redirect_val;
}

%token <value> NAME 
%token <dval>   NUM_TOK
%token <sval>   WORD_TOK

%type <command>        command
%type <command_list>   command-list  
%type <command_list>   start
%type <word_list>      word-list
%type <word_val>       word
%type <redirect_val>   redirect
%type <redirect_val>   redirect-list
%%
start: command-list  
                     {  
                        parsed_command_list = $1; 
                        $$ = parsed_command_list; 
                     } 

command-list:   
       command '|' command-list   
             { 
                 printf("[parser piped]\n "); 
                 $1->next = $3;
                 $$ = $1;
             } 
       | command   { $$ = $1; } 
       | command redirect-list  {  
                              printf("[debug] redirection found \n"); fflush(stdout);                          
                              $1->redirect =  $2;           
                              $$ = $1; 
                           } 
;

command:  word-list ';' command
                      {
                          COMMAND*  cmd = make_cmd($1);
                          cmd->next = $3;
                          $$ = cmd;
                      }
       | word-list     
                      { 
                          $$ = make_cmd($1);
                      }
;

redirect-list: redirect  redirect-list  
                       {
                          struct redirect* r = $1 ;
                          $2->next = r;
                          $$ = $2; 
                       }
           |  redirect 
;

redirect:     '>' word  {
                          struct word* word = $2;
                          $$ = make_redirect(word,r_output_direction);
                        }
            | '<' word  {
                          struct word* word = $2;
                          $$ = make_redirect(word,r_input_direction);
                        }      
;

word-list: word word-list 
                    { 
                        WORD_LIST* lst = $2;
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

COMMAND* make_connection(COMMAND* first , COMMAND* second,enum connection_type type)
{
    CONNECTION* conn = malloc(sizeof(CONNECTION));
    conn->first = first;
    conn->second = second;
    conn->type  =type;

    //    COMMAND* cmd = malloc(sizeof(COMMAND));     
    //    COMMAND* cmd = malloc(sizeof(COMMAND));
    //    cmd->
    return NULL;
}
struct redirect* make_redirect(struct word* word,enum r_instruction instruction)
{
    struct redirect* r = malloc(sizeof(struct redirect));
    r->instruction = instruction;                                
    r->redirector.filename = word;
    r->next = NULL;
    return r;
}
WORD_LIST* add_word_list(WORD_LIST** list , struct word* word) 
{
    WORD_LIST* node = make_word_list_node(word);
    node->next = *list;
    *list = node;    
    return *list;
}

WORD_LIST* make_word_list_node(struct word* word) 
{
    WORD_LIST* word_list = malloc(sizeof (WORD_LIST));
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

COMMAND* make_cmd(WORD_LIST* wd) 
{
    COMMAND* cmd   = malloc(sizeof (COMMAND*));
    cmd->value = wd;
    cmd->next = NULL;
    return cmd;
}
